// LOAOverlayWidgetController.cpp

#include "UI/LOAOverlayWidgetController.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Mana.h"
#include "AbilitySystem/Effects/LOAGE_Cooldown.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

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
	BindAttributeChanged(ULOAAttributeSet_Mana::GetManaAttribute(), &ThisClass::HandleManaChanged);
	BindAttributeChanged(ULOAAttributeSet_Mana::GetMaxManaAttribute(), &ThisClass::HandleManaChanged);

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
}

void ULOAOverlayWidgetController::BroadcastInitialValues()
{
	BroadcastHealth();
	BroadcastMana();

	// 호스트는 이미 스킬이 있다. 클라는 아직 비어 있을 수 있다 (도착하면 OnAbilitiesChanged로 다시 보낸다)
	BroadcastSkillSlots();

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
	}

	AttributeChangedHandles.Reset();
	EffectAddedHandle.Reset();
	AbilitiesChangedHandle.Reset();
	CooldownTagEventHandles.Reset();

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

	OnHealthChanged.Broadcast(
		MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute()),
		MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute()));
}

void ULOAOverlayWidgetController::BroadcastMana() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return;
	}

	OnManaChanged.Broadcast(
		MyASC->GetNumericAttribute(ULOAAttributeSet_Mana::GetManaAttribute()),
		MyASC->GetNumericAttribute(ULOAAttributeSet_Mana::GetMaxManaAttribute()));
}

// =============================================================================
// 스킬 슬롯
// =============================================================================

void ULOAOverlayWidgetController::BroadcastSkillSlots()
{
	TArray<FLOASkillSlotInfo> SkillSlots;

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

			// ③ 한 줄
			FLOASkillSlotInfo& SkillSlot = SkillSlots.AddDefaulted_GetRef();
			SkillSlot.InputTag = InputTag;
			SkillSlot.DisplayInfo = PlayerAbility->GetSkillDisplayInfo();
		}
	}

	// 비어 있어도 보낸다. (위젯이 "아이콘 없음"으로 지우도록)
	OnSkillSlotsChanged.Broadcast(SkillSlots);
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
