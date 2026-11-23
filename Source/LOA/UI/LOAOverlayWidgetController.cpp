// LOAOverlayWidgetController.cpp

#include "UI/LOAOverlayWidgetController.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "AbilitySystem/Effects/LOAGE_Cooldown.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "Core/LOALog.h"
#include "Core/LOAGameplayTags.h"
#include "Game/LOAGameState.h"
#include "Player/LOAPlayerController.h"
#include "UI/Party/LOAPartyMemberController.h"
#include "UI/LOATooltipWidget.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAOverlayWidgetController)

// =============================================================================
// 시작 / 끝
// =============================================================================

void ULOAOverlayWidgetController::Init(APlayerController* InPlayerController, APlayerState* InPlayerState, UAbilitySystemComponent* InASC)
{
	// 혹시 두 번 불리면 이전 알림부터 정리한다. (같은 알림이 두 번 걸리면 위젯도 두 번 갱신됨)
	Shutdown();

	PlayerController = InPlayerController;
	PlayerState = InPlayerState;
	ASC = InASC;

	if (!InASC)
	{
		LOA_LOG(InPlayerController, Warning, TEXT("[OverlayWidgetController] ASC is null. UI will not update."));
		return;
	}

	// ---------- 체력 / 마나 ----------
	// 현재 값과 최대 값 중 어느 쪽이 바뀌어도 바를 다시 그려야 하므로 둘 다 건다.
	// (예: 버프로 최대 체력이 늘면 현재 체력은 그대로여도 바 길이가 바뀐다)
	BindAttributeChanged(ULOAAttributeSet_Common::GetHealthAttribute(), &ThisClass::HandleHealthChanged);
	BindAttributeChanged(ULOAAttributeSet_Common::GetMaxHealthAttribute(), &ThisClass::HandleHealthChanged);
	// 보호막도 체력바의 분모를 바꾼다 (헤더 [체력바 · 보호막], 2026-11-07)
	BindAttributeChanged(ULOAAttributeSet_Common::GetShieldAttribute(), &ThisClass::HandleHealthChanged);
	BindAttributeChanged(ULOAAttributeSet_Player::GetManaAttribute(), &ThisClass::HandleManaChanged);
	BindAttributeChanged(ULOAAttributeSet_Player::GetMaxManaAttribute(), &ThisClass::HandleManaChanged);

	// ---------- 아이덴티티 (헤더 [아이덴티티]) ----------
	BindAttributeChanged(ULOAAttributeSet_Player::GetIdentityGaugeAttribute(), &ThisClass::HandleIdentityGaugeChanged);
	BindAttributeChanged(ULOAAttributeSet_Player::GetMaxIdentityGaugeAttribute(), &ThisClass::HandleIdentityGaugeChanged);
	IdentityActiveTagChangedHandle = InASC->RegisterGameplayTagEvent(LOAGameplayTags::State_Identity_Active, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleIdentityActiveTagChanged);

	// ---------- 쿨타임 시작 ----------
	// 시간이 있는 GE가 붙을 때마다 호출된다. [서버 + 본인 컴퓨터 둘 다]
	// 본인 컴퓨터에서는 예측으로 먼저 붙을 때, 또는 서버 것이 복제로 도착할 때 호출된다.
	EffectAddedHandle = InASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);

	// ---------- 스킬 슬롯 ----------
	// 스킬 목록이 바뀌면(복제 도착, 부여, 나중에 슬롯 이동) 슬롯 목록을 다시 보낸다.
	if (ULOAAbilitySystemComponent* LOAASC = Cast<ULOAAbilitySystemComponent>(InASC))
	{
		AbilitiesChangedHandle = LOAASC->OnAbilitiesChanged.AddUObject(this, &ThisClass::BroadcastSkillSlots);
	}
	// 툴팁 {Damage}: 공격력 BaseValue가 바뀌면 다시 보낸다 (헤더 [툴팁 숫자]). 클라는 공격력이 HUD보다 늦게 올 수 있다
	BindAttributeChanged(ULOAAttributeSet_Combat::GetAttackPowerAttribute(), &ThisClass::HandleAttackPowerChanged);

	// ---------- 차지 게이지 ----------
	// 차지 GA가 조종하는 사람 컴퓨터에서 보내는 신호 (헤더 [차지 게이지])
	const FGameplayTag ChargeTags[] = { LOAGameplayTags::Event_Skill_Charge_Started, LOAGameplayTags::Event_Skill_Charge_Released, LOAGameplayTags::Event_Skill_Charge_Cancelled };
	for (const FGameplayTag& ChargeTag : ChargeTags)
	{
		ChargeEventHandles.Add(ChargeTag, InASC->GenericGameplayEventCallbacks.FindOrAdd(ChargeTag).AddUObject(this, &ThisClass::HandleChargeEvent));
	}

	// ---------- 사망 / 부활 ----------
	// 내 State.Body.Dead (서버가 붙이고 복제됨) → 부활 UI 보이기/숨기기
	DeadTagChangedHandle = InASC->RegisterGameplayTagEvent(LOAGameplayTags::State_Body_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDeadTagChanged);

	// 공용 목숨 (GameState). HUD는 캐릭터가 준비된 뒤 만들어지므로 GameState는 이미 와 있다.
	const UWorld* World = InPlayerController ? InPlayerController->GetWorld() : nullptr;
	if (ALOAGameState* LOAGameState = World ? World->GetGameState<ALOAGameState>() : nullptr)
	{
		RaidGameState = LOAGameState;
		ReviveLivesChangedHandle = LOAGameState->OnReviveLivesChanged.AddUObject(this, &ThisClass::BroadcastReviveLives);

		// 파티 (헤더 [파티])
		PartyChangedHandle = LOAGameState->OnPartyChanged.AddUObject(this, &ThisClass::RebuildParty);
	}
	else
	{
		LOA_LOG(InPlayerController, Warning, TEXT("[OverlayWidgetController] GameState is not ALOAGameState. Revive UI will not update."));
	}
}

void ULOAOverlayWidgetController::BroadcastInitialValues()
{
	BroadcastHealth();
	BroadcastMana();
	BroadcastIdentityGauge();
	BroadcastIdentityActive();

	// 호스트는 이미 스킬이 있다. 클라는 아직 비어 있을 수 있다 (도착하면 OnAbilitiesChanged로 다시 보낸다)
	BroadcastSkillSlots();

	// 늦게 들어왔는데 이미 죽어 있거나, 전투 중이라 목숨이 이미 있을 수 있다.
	BroadcastDead();
	BroadcastReviveLives();

	// 파티: 이미 들어와 있는 사람으로 목록을 만들어 보낸다 (뒤에 들어오는 사람은 OnPartyChanged로)
	RebuildParty();

	// 쿨타임은 보내지 않는다. HUD는 게임 시작 때 한 번 만들어지므로 이 시점에 돌고 있는 쿨타임이 없다.
}

void ULOAOverlayWidgetController::Shutdown()
{
	// ASC(PlayerState 소유)가 이 컨트롤러보다 오래 살 수 있으므로, 걸어 둔 알림을 직접 해제한다.
	// 해제하지 않으면 ASC가 이미 사라진 컨트롤러를 부르려 한다. (AddUObject는 대상이 사라지면 건너뛰긴 하지만, 목록에 쓰레기가 쌓인다)
	if (UAbilitySystemComponent* MyASC = ASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : AttributeChangedHandles)
		{
			MyASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}

		MyASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(EffectAddedHandle);

		if (ULOAAbilitySystemComponent* LOAASC = Cast<ULOAAbilitySystemComponent>(MyASC))
		{
			LOAASC->OnAbilitiesChanged.Remove(AbilitiesChangedHandle);
		}

		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : CooldownTagEventHandles)
		{
			MyASC->UnregisterGameplayTagEvent(Pair.Value, Pair.Key, EGameplayTagEventType::NewOrRemoved);
		}

		MyASC->UnregisterGameplayTagEvent(DeadTagChangedHandle, LOAGameplayTags::State_Body_Dead, EGameplayTagEventType::NewOrRemoved);
		MyASC->UnregisterGameplayTagEvent(IdentityActiveTagChangedHandle, LOAGameplayTags::State_Identity_Active, EGameplayTagEventType::NewOrRemoved);

		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : ChargeEventHandles)
		{
			if (FGameplayEventMulticastDelegate* Callbacks = MyASC->GenericGameplayEventCallbacks.Find(Pair.Key))
			{
				Callbacks->Remove(Pair.Value);
			}
		}
	}

	if (ALOAGameState* LOAGameState = RaidGameState.Get())
	{
		LOAGameState->OnReviveLivesChanged.Remove(ReviveLivesChangedHandle);
		LOAGameState->OnPartyChanged.Remove(PartyChangedHandle);
	}
	ClearParty();
	PartyChangedHandle.Reset();

	AttributeChangedHandles.Reset();
	EffectAddedHandle.Reset();
	AbilitiesChangedHandle.Reset();
	CooldownTagEventHandles.Reset();
	DeadTagChangedHandle.Reset();
	IdentityActiveTagChangedHandle.Reset();
	ReviveLivesChangedHandle.Reset();
	RaidGameState.Reset();
	ChargeEventHandles.Reset();
	bCharging = false;

	PlayerController.Reset();
	PlayerState.Reset();
	ASC.Reset();
}

// =============================================================================
// 체력 / 마나
// =============================================================================

void ULOAOverlayWidgetController::BindAttributeChanged(const FGameplayAttribute& Attribute, void (ULOAOverlayWidgetController::*Handler)(const FOnAttributeChangeData&))
{
	UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return;
	}

	const FDelegateHandle Handle = MyASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, Handler);
	AttributeChangedHandles.Emplace(Attribute, Handle);
}

void ULOAOverlayWidgetController::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// Data에는 바뀐 쪽(현재 또는 최대) 값만 들어 있다. 바를 그리려면 둘 다 필요하므로 다시 읽어서 보낸다.
	BroadcastHealth();
}

void ULOAOverlayWidgetController::HandleManaChanged(const FOnAttributeChangeData& Data)
{
	BroadcastMana();
}

void ULOAOverlayWidgetController::BroadcastHealth() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return;
	}

	const float Health = MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute());
	const float MaxHealth = MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute());
	const float Shield = MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetShieldAttribute());

	OnHealthChanged.Broadcast(Health, MaxHealth);

	// 체력바 (헤더 [체력바 · 보호막])
	float HealthPercent = 0.f;
	float ShieldEndPercent = 0.f;
	ComputeHealthBar(Health, MaxHealth, Shield, HealthPercent, ShieldEndPercent);
	OnHealthBarChanged.Broadcast(HealthPercent, ShieldEndPercent, Shield);
}

void ULOAOverlayWidgetController::ComputeHealthBar(float Health, float MaxHealth, float Shield, float& OutHealthPercent, float& OutShieldEndPercent)
{
	// 분모 = 최대 체력과 (체력 + 보호막) 중 큰 쪽. 예) 최대 1000, 체력 800, 보호막 500 → 1300
	const float SafeHealth = FMath::Max(Health, 0.f);
	const float SafeShield = FMath::Max(Shield, 0.f);
	const float Denominator = FMath::Max(MaxHealth, SafeHealth + SafeShield);
	if (Denominator <= 0.f)
	{
		// 초기화 전 (전부 0)
		OutHealthPercent = 0.f;
		OutShieldEndPercent = 0.f;
		return;
	}

	OutHealthPercent = FMath::Clamp(SafeHealth / Denominator, 0.f, 1.f);
	OutShieldEndPercent = FMath::Clamp((SafeHealth + SafeShield) / Denominator, 0.f, 1.f);
}

void ULOAOverlayWidgetController::BroadcastMana() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return;
	}

	OnManaChanged.Broadcast(
		MyASC->GetNumericAttribute(ULOAAttributeSet_Player::GetManaAttribute()),
		MyASC->GetNumericAttribute(ULOAAttributeSet_Player::GetMaxManaAttribute()));
}

// =============================================================================
// 스킬 슬롯
// =============================================================================

void ULOAOverlayWidgetController::BroadcastSkillSlots()
{
	TArray<FLOASkillSlotInfo> SkillSlots;

	// {Damage}에 쓸 공격력. 이번에 쓴 값을 기억해 두고 바뀔 때만 다시 보낸다 (HandleAttackPowerChanged)
	LastAttackPowerBase = ReadAttackPowerBase();

	if (const UAbilitySystemComponent* MyASC = ASC.Get())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : MyASC->GetActivatableAbilities())
		{
			// ① 키가 없는 스킬(사망, 스턴 같은 자동 스킬)은 슬롯이 없다
			const FGameplayTag InputTag = GetInputTagOfSpec(AbilitySpec);
			if (!InputTag.IsValid())
			{
				continue;
			}

			// ② 플레이어 스킬이고 아이콘이 있나. (Spec.Ability = 클래스 기본값. 클라에서 클래스 로드 전이면 비어 있다 → 준비되면 목록 바뀜 알림이 다시 온다)
			//    아이콘이 없는 스킬(회피, 평타)은 슬롯 UI가 없다
			const ULOAPlayerAbility* PlayerAbility = Cast<ULOAPlayerAbility>(AbilitySpec.Ability);
			if (!PlayerAbility || PlayerAbility->GetSkillDisplayInfo().Icon.IsNull())
			{
				continue;
			}

			// ③④ 한 줄 + 툴팁 숫자 (그 스킬의 레벨, 내 공격력). 로비와 같은 함수
			FLOASkillSlotInfo& SkillSlot = SkillSlots.AddDefaulted_GetRef();
			LOATooltipFormat::FillSkillSlotInfo(SkillSlot, InputTag, *PlayerAbility, static_cast<float>(AbilitySpec.Level), LastAttackPowerBase);
		}
	}

	// 비어 있어도 보낸다. (위젯이 "아이콘 없음"으로 지우도록)
	OnSkillSlotsChanged.Broadcast(SkillSlots);
}

float ULOAOverlayWidgetController::ReadAttackPowerBase() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	const FGameplayAttribute AttackPower = ULOAAttributeSet_Combat::GetAttackPowerAttribute();
	if (!MyASC || !MyASC->HasAttributeSetForAttribute(AttackPower))
	{
		return 0.f;
	}
	// BaseValue: 버프가 켜져도 그대로 (사용자 A, 11-13)
	return MyASC->GetNumericAttributeBase(AttackPower);
}

void ULOAOverlayWidgetController::HandleAttackPowerChanged(const FOnAttributeChangeData& Data)
{
	// 버프(현재 값만 바뀜)면 BaseValue가 그대로 → 다시 보내지 않는다
	if (!FMath::IsNearlyEqual(ReadAttackPowerBase(), LastAttackPowerBase))
	{
		BroadcastSkillSlots();
	}
}

FGameplayTag ULOAOverlayWidgetController::GetInputTagOfSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	// InputTag는 부여할 때 플레이어 꾸러미가 Spec의 DynamicSpecSourceTags에 넣어 둔다. (지금은 InputTag 하나만 들어감)
	// 슬롯을 옮기면 이 값이 바뀌므로, 매번 여기서 읽어야 항상 "지금 키"가 나온다.
	const FGameplayTagContainer& SourceTags = AbilitySpec.GetDynamicSpecSourceTags();
	return SourceTags.IsEmpty() ? FGameplayTag() : SourceTags.First();
}

// =============================================================================
// 쿨타임
// =============================================================================

void ULOAOverlayWidgetController::HandleEffectAdded(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle ActiveHandle)
{
	// ① 쿨타임 GE만 본다. (버프, 디버프 등 다른 GE는 무시)
	if (!Spec.Def || !Spec.Def->IsA<ULOAGE_Cooldown>())
	{
		return;
	}

	// ② 이 GE가 주는 태그 = 쿨타임 태그. ApplyCooldown이 DynamicGrantedTags에 넣은 것이 여기 들어 있다.
	FGameplayTagContainer CooldownTags;
	Spec.GetAllGrantedTags(CooldownTags);

	for (const FGameplayTag& CooldownTag : CooldownTags)
	{
		// 끝 알림은 InputTag를 못 찾아도 걸어 둔다. (나중에 슬롯에 놓일 수도 있으므로)
		ListenCooldownTagRemoved(CooldownTag);

		// ③ 이 쿨타임을 쓰는 스킬이 지금 어느 키에 있나
		const FGameplayTag InputTag = FindInputTagForCooldownTag(CooldownTag);
		if (!InputTag.IsValid())
		{
			continue;   // 키가 없는 스킬. 슬롯 UI가 없으니 보낼 곳이 없다
		}

		// ④ 실제로 붙은 GE에서 시간을 읽는다. (쿨감 버프가 생겨도 화면이 맞도록)
		float TimeRemaining = 0.f;
		float Duration = 0.f;
		if (!GetCooldownTime(CooldownTag, TimeRemaining, Duration))
		{
			// 검색에 안 걸리는 경우를 대비한 예비값: 방금 붙은 GE의 전체 시간
			Duration = Spec.GetDuration();
			TimeRemaining = Duration;
		}

		// ⑤ 위젯에게 알림
		OnSkillCooldownStarted.Broadcast(InputTag, TimeRemaining, Duration);
	}
}

void ULOAOverlayWidgetController::HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount)
{
	// 태그가 "생길 때"도 호출된다. 시작 알림은 HandleEffectAdded가 보내므로 여기서는 "사라질 때"만 본다.
	if (NewCount > 0)
	{
		return;
	}

	const FGameplayTag InputTag = FindInputTagForCooldownTag(CooldownTag);
	if (InputTag.IsValid())
	{
		OnSkillCooldownEnded.Broadcast(InputTag);
	}
}

void ULOAOverlayWidgetController::ListenCooldownTagRemoved(const FGameplayTag& CooldownTag)
{
	UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC || CooldownTagEventHandles.Contains(CooldownTag))
	{
		return;
	}

	// NewOrRemoved: 개수가 0 → 1 이상(생김), 1 이상 → 0(사라짐)일 때만 호출. (2 → 3 같은 변화는 무시)
	const FDelegateHandle Handle = MyASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCooldownTagChanged);

	CooldownTagEventHandles.Add(CooldownTag, Handle);
}

FGameplayTag ULOAOverlayWidgetController::FindInputTagForCooldownTag(const FGameplayTag& CooldownTag) const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return FGameplayTag();
	}

	for (const FGameplayAbilitySpec& AbilitySpec : MyASC->GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability)
		{
			continue;
		}

		// 이 스킬의 쿨타임 태그에 찾는 태그가 있나 (ULOAGameplayAbility::GetCooldownTags가 BP에서 고른 값을 돌려줌)
		const FGameplayTagContainer* AbilityCooldownTags = AbilitySpec.Ability->GetCooldownTags();
		if (!AbilityCooldownTags || !AbilityCooldownTags->HasTagExact(CooldownTag))
		{
			continue;
		}

		// 이 스킬에 키가 있으면 그 키. 없으면 같은 쿨타임 태그를 가진 다른 스킬을 계속 찾는다 (공유 쿨타임)
		const FGameplayTag InputTag = GetInputTagOfSpec(AbilitySpec);
		if (InputTag.IsValid())
		{
			return InputTag;
		}
	}

	return FGameplayTag();
}

bool ULOAOverlayWidgetController::GetCooldownTime(const FGameplayTag& CooldownTag, float& OutTimeRemaining, float& OutDuration) const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return false;
	}

	// "이 쿨타임 태그를 주는 GE"를 전부 찾는다. (DynamicGrantedTags도 검색 대상에 포함된다)
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTag.GetSingleTagContainer());
	const TArray<TPair<float, float>> Times = MyASC->GetActiveEffectsTimeRemainingAndDuration(Query);
	if (Times.IsEmpty())
	{
		return false;
	}

	// 여러 개면 가장 오래 남은 것 기준. (예측 GE와 서버 GE가 잠깐 같이 있을 때, 공유 쿨타임일 때)
	OutTimeRemaining = Times[0].Key;
	OutDuration = Times[0].Value;
	for (const TPair<float, float>& Time : Times)
	{
		if (Time.Key > OutTimeRemaining)
		{
			OutTimeRemaining = Time.Key;
			OutDuration = Time.Value;
		}
	}

	return true;
}

// =============================================================================
// 아이덴티티
// =============================================================================

void ULOAOverlayWidgetController::HandleIdentityGaugeChanged(const FOnAttributeChangeData& Data)
{
	BroadcastIdentityGauge();
}

void ULOAOverlayWidgetController::HandleIdentityActiveTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	BroadcastIdentityActive();
}

void ULOAOverlayWidgetController::BroadcastIdentityGauge() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return;
	}

	OnIdentityGaugeChanged.Broadcast(
		MyASC->GetNumericAttribute(ULOAAttributeSet_Player::GetIdentityGaugeAttribute()),
		MyASC->GetNumericAttribute(ULOAAttributeSet_Player::GetMaxIdentityGaugeAttribute()));
}

void ULOAOverlayWidgetController::BroadcastIdentityActive() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	OnIdentityActiveChanged.Broadcast(MyASC && MyASC->HasMatchingGameplayTag(LOAGameplayTags::State_Identity_Active));
}

// =============================================================================
// 사망 / 부활
// =============================================================================

void ULOAOverlayWidgetController::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	BroadcastDead();
}

void ULOAOverlayWidgetController::BroadcastDead() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	OnDeadChanged.Broadcast(MyASC && MyASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead));
}

void ULOAOverlayWidgetController::BroadcastReviveLives() const
{
	if (const ALOAGameState* LOAGameState = RaidGameState.Get())
	{
		OnReviveLivesChanged.Broadcast(LOAGameState->GetReviveLives(), LOAGameState->GetMaxReviveLives());
	}
}

void ULOAOverlayWidgetController::RequestRevive()
{
	if (ALOAPlayerController* LOAPC = Cast<ALOAPlayerController>(PlayerController.Get()))
	{
		LOAPC->RequestRevive();
	}
}

float ULOAOverlayWidgetController::GetSecondsUntilNextReviveLife() const
{
	const ALOAGameState* LOAGameState = RaidGameState.Get();
	return LOAGameState ? LOAGameState->GetSecondsUntilNextReviveLife() : -1.f;
}

// =============================================================================
// 차지 게이지 (헤더 [차지 게이지])
// =============================================================================

void ULOAOverlayWidgetController::HandleChargeEvent(const FGameplayEventData* Payload)
{
	const ULOAGA_Charge* ChargeAbility = Payload ? Cast<ULOAGA_Charge>(Payload->OptionalObject.Get()) : nullptr;
	if (!ChargeAbility)
	{
		return;
	}

	if (Payload->EventTag == LOAGameplayTags::Event_Skill_Charge_Started)
	{
		// 숫자와 시작 시각을 기억 → 위젯은 GetChargeGaugeState로 매 프레임 그린다
		bCharging = true;
		ChargeInfo = ChargeAbility->MakeChargeGaugeInfo();
		ChargeStartTime = GetLocalTimeSeconds();
		OnChargeStarted.Broadcast();
	}
	else if (Payload->EventTag == LOAGameplayTags::Event_Skill_Charge_Released)
	{
		// 차지 GA가 잰 누른 시간(EventMagnitude)으로 도달 단계·퍼펙트
		const float HeldTime = Payload->EventMagnitude;
		bool bPerfect = false;
		ChargeAbility->CalculateChargeMultiplier(HeldTime, bPerfect);
		bCharging = false;
		OnChargeReleased.Broadcast(ChargeAbility->GetReachedStageCount(HeldTime), bPerfect);
	}
	else if (Payload->EventTag == LOAGameplayTags::Event_Skill_Charge_Cancelled)
	{
		bCharging = false;
		OnChargeCancelled.Broadcast();
	}
}

FLOAChargeGaugeState ULOAOverlayWidgetController::GetChargeGaugeState() const
{
	if (!bCharging)
	{
		return FLOAChargeGaugeState();
	}
	return ComputeChargeGaugeState(ChargeInfo, static_cast<float>(GetLocalTimeSeconds() - ChargeStartTime));
}

FLOAChargeGaugeState ULOAOverlayWidgetController::ComputeChargeGaugeState(const FLOAChargeGaugeInfo& Info, float HeldTime)
{
	FLOAChargeGaugeState State;
	State.bActive = true;

	const float MaxTime = FMath::Max(Info.MaxChargeTime, KINDA_SMALL_NUMBER);
	const float Time = FMath::Clamp(HeldTime, 0.f, MaxTime);

	// ① 칸 경계: 0, 단계 시각들 (최대보다 작은 것). 예) [0, 0.8, 1.6]. 단계가 없으면 [0, 최대]
	TArray<float, TInlineAllocator<8>> Bounds;
	Bounds.Add(0.f);
	for (const float StageTime : Info.StageTimes)
	{
		if (StageTime > Bounds.Last() && StageTime <= MaxTime)
		{
			Bounds.Add(StageTime);
		}
	}
	if (Bounds.Num() == 1)
	{
		Bounds.Add(MaxTime);
	}

	// ② 지금 칸: Bounds[Index] ≤ Time < Bounds[Index + 1]. 마지막 단계를 채웠으면 마지막 칸 꽉 참
	int32 Index = Bounds.Num() - 2;
	for (int32 Candidate = 0; Candidate < Bounds.Num() - 1; ++Candidate)
	{
		if (Time < Bounds[Candidate + 1])
		{
			Index = Candidate;
			break;
		}
	}
	const float SegmentStart = Bounds[Index];
	const float SegmentEnd = Bounds[Index + 1];
	const float SegmentLength = FMath::Max(SegmentEnd - SegmentStart, KINDA_SMALL_NUMBER);
	State.Progress = FMath::Clamp((Time - SegmentStart) / SegmentLength, 0.f, 1.f);

	// ③ 지금 채우는 단계 (게이지 왼쪽 위 숫자) = 지금 칸 번호 + 1. 마지막 단계를 채웠으면 마지막 칸 그대로
	State.Stage = Index + 1;

	// ④ 퍼펙트 존: 지금 칸에 걸쳐 있으면 칸 안의 비율로
	if (Info.bUsePerfectZone && Info.PerfectZoneEnd > SegmentStart && Info.PerfectZoneStart < SegmentEnd)
	{
		State.bShowPerfectZone = true;
		State.PerfectZoneStart = FMath::Clamp((Info.PerfectZoneStart - SegmentStart) / SegmentLength, 0.f, 1.f);
		State.PerfectZoneEnd = FMath::Clamp((Info.PerfectZoneEnd - SegmentStart) / SegmentLength, 0.f, 1.f);
	}
	State.bInPerfectZone = Info.bUsePerfectZone && HeldTime >= Info.PerfectZoneStart && HeldTime <= Info.PerfectZoneEnd;
	return State;
}

double ULOAOverlayWidgetController::GetLocalTimeSeconds() const
{
	const APlayerController* PC = PlayerController.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.0;
}

// =============================================================================
// 파티 (헤더 [파티])
// =============================================================================

void ULOAOverlayWidgetController::RebuildParty()
{
	const ALOAGameState* LOAGameState = RaidGameState.Get();
	if (!LOAGameState)
	{
		return;
	}

	const APlayerState* MyPlayerState = PlayerState.Get();

	// 순서: 나 먼저, 나머지는 PlayerArray 순서
	TArray<APlayerState*> Ordered;
	for (APlayerState* Member : LOAGameState->PlayerArray)
	{
		if (!IsValid(Member))
		{
			continue;
		}
		if (Member == MyPlayerState)
		{
			Ordered.Insert(Member, 0);
		}
		else
		{
			Ordered.Add(Member);
		}
	}

	// 이미 있는 사람은 컨트롤러를 그대로, 새 사람만 만든다
	TArray<TObjectPtr<ULOAPartyMemberController>> NewMembers;
	for (APlayerState* Member : Ordered)
	{
		const int32 ExistingIndex = PartyMembers.IndexOfByPredicate([Member](const ULOAPartyMemberController* Existing)
		{
			return Existing && Existing->GetPlayerState() == Member;
		});

		if (ExistingIndex != INDEX_NONE)
		{
			NewMembers.Add(PartyMembers[ExistingIndex]);
			PartyMembers.RemoveAt(ExistingIndex);
		}
		else
		{
			ULOAPartyMemberController* NewMember = NewObject<ULOAPartyMemberController>(this);
			NewMember->Init(Member, Member == MyPlayerState);
			NewMembers.Add(NewMember);
		}
	}

	// 남은 것 = 나간 사람 (또는 PlayerState가 사라짐)
	for (ULOAPartyMemberController* Gone : PartyMembers)
	{
		if (Gone)
		{
			Gone->Shutdown();
		}
	}
	PartyMembers = MoveTemp(NewMembers);

	TArray<ULOAPartyMemberController*> Members;
	Members.Reserve(PartyMembers.Num());
	for (ULOAPartyMemberController* Member : PartyMembers)
	{
		Members.Add(Member);
	}
	OnPartyChanged.Broadcast(Members);
}

void ULOAOverlayWidgetController::ClearParty()
{
	for (ULOAPartyMemberController* Member : PartyMembers)
	{
		if (Member)
		{
			Member->Shutdown();
		}
	}
	PartyMembers.Reset();
}
