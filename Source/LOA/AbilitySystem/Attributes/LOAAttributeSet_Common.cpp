// LOAAttributeSet_Common.cpp

#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Library/LOACombatLibrary.h"
#include "Combat/LOACombatNoticeSubsystem.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "GameplayEffectExtension.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

ULOAAttributeSet_Common::ULOAAttributeSet_Common()
{
	// 초기값을 여기서 넣지 않는다. (하드코딩 금지)
	// 모든 스탯은 0으로 시작하고, 캐릭터가 준비될 때 데이터(초기화용 GameplayEffect)로 채워진다.
}

void ULOAAttributeSet_Common::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// REPNOTIFY_Always : 값이 같아도 OnRep을 호출한다.
	// GAS는 클라이언트가 먼저 예측해서 값을 바꿔 두기 때문에, 서버 값이 "같게" 도착해도 알림이 필요하다.

	// 체력: 모두에게 복제 (파티창, 보스 체력바)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Common, Health,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Common, MaxHealth, COND_None, REPNOTIFY_Always);
	// 보호막: 모두에게 복제 (파티창) (2026-11-07)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Common, Shield,    COND_None, REPNOTIFY_Always);

	// (마나는 ULOAAttributeSet_Player(옛 Mana)로 옮겼다. 플레이어만 가지는 값이라서)
}

void ULOAAttributeSet_Common::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		// 최대 체력이 아직 0이면(초기화 도중) 위쪽 제한을 걸지 않는다.
		// 초기화 순서가 "체력 → 최대 체력"이면 체력이 0으로 잘려버리는 사고를 막기 위함.
		const float Max = GetMaxHealth();
		NewValue = (Max > 0.f) ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetShieldAttribute())
	{
		// 최대 체력, 보호막은 음수가 될 수 없다.
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

bool ULOAAttributeSet_Common::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	// [서버에서만 호출됨]
	// 적용 직전 체력을 적어 둔다. Post에서 "0보다 컸는데 0이 됨"을 판단하는 기준.
	// (Post 시점에는 이미 체력이 바뀌어 있어서 이전 값을 알 수 없다)
	HealthBeforeExecute = GetHealth();

	// false를 돌려주면 이 GE 적용 자체가 취소된다. 여기서는 막지 않는다.
	return Super::PreGameplayEffectExecute(Data);
}

void ULOAAttributeSet_Common::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// [서버에서만 호출됨]
	// 데미지/회복이 "실제로 확정된" 뒤 처리한다.
	//   Damage(메타) → 꺼내서 Health 감소 (받는 쪽 처리)
	//   Health       → 최종 값을 범위 안으로 맞춤
	// PreAttributeChange는 보여지는 값만 맞추고, 여기서는 저장된 기본값까지 맞춘다.

	// ---------- 데미지 입구 (메타) ----------
	// ULOADamageExecution이 넣어 준 데미지를 꺼내서 Health에 반영한다.
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// ① 꺼내고 즉시 비운다. (입구일 뿐 보관하는 값이 아니다. 다음 데미지는 다시 0부터)
		const float IncomingDamage = GetDamage();
		SetDamage(0.f);

		// ①-1 맞을 수 없는 대상(시체)이면 버린다. 체력도, 데미지 숫자도 없다.
		//      예) 보스가 죽은 뒤에도 남아 있는 장판의 다음 틱, 도트의 다음 틱
		//      판정이 대상을 고를 때와 같은 함수를 쓴다. (규칙은 한 곳)
		if (!ULOACombatLibrary::IsTargetable(Data.Target.GetAvatarActor()))
		{
			return;
		}

		if (IncomingDamage > 0.f)
		{
			// ② 보호막을 먼저 깎고 남은 만큼만 체력으로 (2026-11-07)
			//    예) 보호막 200, 데미지 300 → 보호막 0, 체력 -100 / 보호막 500, 데미지 300 → 보호막 200, 체력 그대로
			//    데미지 숫자와 사망 이벤트에는 들어온 데미지 전체를 쓴다 (보호막이 막은 몫 포함)
			//    보호막은 보호막 GE들의 몫이라 값을 직접 깎지 않는다. 곧 끝날 GE부터 "깎인 양"을 늘린다 (ConsumeShield)
			const float Absorbed = (GetShield() > 0.f) ? ConsumeShield(FMath::Min(GetShield(), IncomingDamage)) : 0.f;
			const float HealthDamage = IncomingDamage - Absorbed;

			// ③ 체력 감소. 0 아래로 내려가지 않는다.
			const float Max = GetMaxHealth();
			const float HealthBefore = GetHealth();
			const float NewHealth = HealthBefore - HealthDamage;
			SetHealth((Max > 0.f) ? FMath::Clamp(NewHealth, 0.f, Max) : FMath::Max(NewHealth, 0.f));

			// ④ 전투 알림판에 "피해가 들어감" (2026-11-15 apply115. 전에는 데미지 숫자를 직접 불렀다)
			//    듣는 쪽: 때린 플레이어 PC의 ULOACombatRPCComponent → 데미지 숫자 / 레이드 기록 → 준 피해·조력·파티가 맞은 양 (누가 듣는지 이 클래스는 모른다)
			//    데미지 숫자 = 들어온 데미지 전체 / 레이드 기록 = 실제로 깎인 양 (보호막 + 체력, 넘친 양 뺌)
			//      예) 체력 200에 900 → 숫자 900, 기록 200 / 보호막 200 + 체력 1000에 500 → 숫자 500, 기록 500
			//    알림에 무엇을 담을지(맞은 지점, 공격 판정 기록 …)는 알림판이 만든다 (2026-11-18 apply125: 여기 있던 것을 옮김)
			//    사망 이벤트(⑤)보다 먼저 → 보스를 죽인 마지막 한 방도 레이드 기록에 들어간다
			if (ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(&Data.Target))
			{
				CombatNotice->NotifyDamageApplied(ULOACombatNoticeSubsystem::MakeDamageInfo(Data, IncomingDamage, Absorbed + (HealthBefore - GetHealth())));
			}

			// ⑤ 체력이 처음 0이 됐으면 사망 이벤트 (받는 쪽이 무엇을 할지는 이 클래스가 모른다)
			SendDeathEventIfJustDied(Data, IncomingDamage);

			// (나중에) 피격 이벤트
		}
		return;
	}

	// ---------- 회복 입구 (메타, 2026-11-07) ----------
	// ULOAHealExecution이 넣어 준 회복량을 꺼내서 Health에 더한다. 최대 체력까지.
	if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float IncomingHealing = GetHealing();
		SetHealing(0.f);

		// 시체는 회복하지 않는다 (부활은 따로: ALOACharacter::ReviveFromDeath)
		if (IncomingHealing <= 0.f || !ULOACombatLibrary::IsTargetable(Data.Target.GetAvatarActor()))
		{
			return;
		}

		const float Max = GetMaxHealth();
		const float HealthBefore = GetHealth();
		const float NewHealth = HealthBefore + IncomingHealing;
		SetHealth((Max > 0.f) ? FMath::Min(NewHealth, Max) : NewHealth);

		// 전투 알림판에 "회복함" (2026-11-15 apply115). 듣는 쪽: 받은 플레이어 PC의 ULOACombatRPCComponent → 회복 숫자(들어온 양, 2026-11-16) / 레이드 기록
		//   실제로 채운 양 = 넘친 회복을 뺀 것. 예) 900/1000에서 300 → 100
		if (ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(&Data.Target))
		{
			FLOAHealAppliedInfo Info;
			Info.Healer = Data.EffectSpec.GetContext().GetInstigator();
			Info.Target = Data.Target.GetAvatarActor();
			Info.TargetOwner = Data.Target.GetOwnerActor();	// 받은 사람별 구독의 열쇠 (플레이어면 PlayerState, 2026-11-16)
			Info.IncomingHealing = IncomingHealing;
			Info.HealedAmount = GetHealth() - HealthBefore;
			CombatNotice->NotifyHealApplied(Info);
		}

		LOA_LOG(Data.Target.GetAvatarActor(), Log, TEXT("[Heal] +%.0f from %s -> health %.0f / %.0f"),
			IncomingHealing, *GetNameSafe(Data.EffectSpec.GetContext().GetInstigator()), GetHealth(), Max);
		return;
	}

	// 최대치가 아직 0이면(초기화 도중) 위쪽 제한을 걸지 않는다. (PreAttributeChange와 같은 이유)
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float Max = GetMaxHealth();
		SetHealth((Max > 0.f) ? FMath::Clamp(GetHealth(), 0.f, Max) : FMath::Max(GetHealth(), 0.f));

		// Damage 입구를 거치지 않고 Health를 직접 바꾸는 GE도 사망을 알린다.
		// (예: 나중에 "체력을 0으로 Override" 하는 전멸기 GE. 데미지 숫자는 없음)
		SendDeathEventIfJustDied(Data, 0.f);
	}
}

void ULOAAttributeSet_Common::SendDeathEventIfJustDied(const FGameplayEffectModCallbackData& Data, float LastDamage) const
{
	// "처음 0이 됨"만. 이미 0이었으면(0 → 0) 보내지 않는다.
	// 예) 체력 300에 데미지 900 → 300 > 0 이고 지금 0 → 보냄
	//     체력 0에 데미지 500   → 0 > 0 아님         → 안 보냄
	if (HealthBeforeExecute <= 0.f || GetHealth() > 0.f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// 이벤트 내용. 받는 쪽(GA_Death)이 TriggerEventData로 읽는다.
	//   Instigator : 막타를 친 쪽 (플레이어면 PlayerState, 보스면 보스 액터. 데미지 GE를 보낸 쪽 그대로)
	//   Target     : 죽은 몸
	FGameplayEventData Payload;
	Payload.EventTag = LOAGameplayTags::Event_Trigger_Death;
	Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
	Payload.Target = Data.Target.GetAvatarActor();
	Payload.EventMagnitude = LastDamage;
	Payload.ContextHandle = Data.EffectSpec.GetContext();

	LOA_LOG(Payload.Target.Get(), Log, TEXT("[Death] Health reached 0. Sending Event.Trigger.Death (killer: %s)"),
		*GetNameSafe(Payload.Instigator.Get()));

	// 이 ASC의 구독자에게 알린다. Trigger가 Event.Death인 Ability(GA_Death)가 여기서 바로 발동한다.
	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
}

float ULOAAttributeSet_Common::ConsumeShield(float Amount) const
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	const UWorld* World = ASC ? ASC->GetWorld() : nullptr;
	if (Amount <= 0.f || !ASC || !World)
	{
		return 0.f;
	}

	// 보호막 GE 하나 = 한 줄. 남은 시간과 지금 몫을 적어 둔다
	struct FShieldEntry
	{
		FActiveGameplayEffectHandle Handle;
		TWeakObjectPtr<AActor> Giver;	// 이 보호막 GE를 보낸 사람 (전투 알림판 → 레이드 기록 "방어", 2026-11-15)
		float TimeRemaining = 0.f;	// 기간 없음(무한)은 아주 큰 값 → 맨 뒤
		float Amount = 0.f;			// 이 GE가 지금 얹은 몫
		float Used = 0.f;			// SetByCaller.ShieldUsed (이미 깎인 양)
	};
	TArray<FShieldEntry> Entries;

	const float WorldTime = World->GetTimeSeconds();
	const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(LOAGameplayTags::State_Buff_Shield)));
	for (const FActiveGameplayEffectHandle& Handle : Handles)
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (!Effect || !Effect->Spec.Def)
		{
			continue;
		}

		// 이 GE의 Shield Modifier가 지금 얹은 몫 (중첩 수까지 곱한 값)
		float EffectAmount = 0.f;
		const TArray<FGameplayModifierInfo>& Modifiers = Effect->Spec.Def->Modifiers;
		for (int32 Index = 0; Index < Modifiers.Num(); ++Index)
		{
			if (Modifiers[Index].Attribute == GetShieldAttribute())
			{
				EffectAmount += Effect->Spec.GetModifierMagnitude(Index, true);
			}
		}
		if (EffectAmount <= 0.f)
		{
			continue;
		}

		FShieldEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Handle = Handle;
		Entry.Giver = Effect->Spec.GetContext().GetInstigator();
		const float Remaining = Effect->GetTimeRemaining(WorldTime);
		Entry.TimeRemaining = (Remaining < 0.f) ? TNumericLimits<float>::Max() : Remaining;
		Entry.Amount = EffectAmount;
		Entry.Used = Effect->Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ShieldUsed, false, 0.f);
	}

	// 곧 끝날 것부터 (사용자 결정 2026-11-07: 어차피 사라질 보호막을 먼저 쓴다)
	Entries.Sort([](const FShieldEntry& A, const FShieldEntry& B) { return A.TimeRemaining < B.TimeRemaining; });

	// 전투 알림판에 "보호막이 막음": 보호막 GE마다 막은 양 + 그 보호막을 건 사람 (2026-11-15 apply115. 듣는 쪽: 레이드 기록)
	ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(ASC);

	float Left = Amount;
	for (const FShieldEntry& Entry : Entries)
	{
		if (Left <= 0.f)
		{
			break;
		}
		const float Take = FMath::Min(Entry.Amount, Left);
		Left -= Take;

		if (CombatNotice)
		{
			FLOAShieldAbsorbedInfo Info;
			Info.ShieldGiver = Entry.Giver.Get();
			Info.Target = ASC->GetAvatarActor();
			Info.Amount = Take;
			CombatNotice->NotifyShieldAbsorbed(Info);
		}

		if (Take >= Entry.Amount)
		{
			// 다 깎임 → 지운다 (태그·아이콘도 같이 사라진다). 몫이 0이 된 GE가 시간 끝까지 남지 않게
			ASC->RemoveActiveGameplayEffect(Entry.Handle);
		}
		else
		{
			// 깎인 양을 늘린다 → 엔진이 바로 Modifier를 다시 계산한다 (CalculateModifierMagnitudes, GameplayEffect.cpp)
			ASC->UpdateActiveGameplayEffectSetByCallerMagnitude(Entry.Handle, LOAGameplayTags::SetByCaller_ShieldUsed, Entry.Used + Take);
		}
	}

	return Amount - Left;
}

// ---------- 복제 도착 알림 ----------
// GAMEPLAYATTRIBUTE_REPNOTIFY : GAS에게 "서버 값이 도착했다"고 알린다.
// 이게 없으면 UI 갱신 알림이 안 오거나, 클라이언트 예측 값이 서버 값과 어긋난 채로 남는다.

void ULOAAttributeSet_Common::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Common, Health, OldValue);
}

void ULOAAttributeSet_Common::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Common, MaxHealth, OldValue);
}

void ULOAAttributeSet_Common::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Common, Shield, OldValue);
}
