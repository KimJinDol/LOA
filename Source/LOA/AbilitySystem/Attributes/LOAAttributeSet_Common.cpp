// LOAAttributeSet_Common.cpp

#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "GameplayEffectExtension.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
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

	// (마나는 ULOAAttributeSet_Mana로 옮겼다. 플레이어만 가지는 값이라서)
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
	else if (Attribute == GetMaxHealthAttribute())
	{
		// 최대 체력은 음수가 될 수 없다.
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
			// ② (나중에) 보호막을 먼저 깎고 남은 만큼만 체력으로

			// ③ 체력 감소. 0 아래로 내려가지 않는다.
			const float Max = GetMaxHealth();
			const float NewHealth = GetHealth() - IncomingDamage;
			SetHealth((Max > 0.f) ? FMath::Clamp(NewHealth, 0.f, Max) : FMath::Max(NewHealth, 0.f));

			// ④ 때린 플레이어 화면에 데미지 숫자 (보낸 사람이 플레이어가 아니면 라이브러리가 무시)
			//    맞은 지점: 히트 판정이 있는 공격이면 Context의 HitResult.
			//    주기 GE(도트)의 틱은 처음 걸 때의 HitResult가 그대로 남아 있을 수 있다. 그 위치는 이미 지난 위치라 쓰지 않는다.
			//    → 맞은 지점 없음으로 보내면 클라가 "내 캐릭터와 가장 가까운 점"에 띄운다.
			const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
			const bool bIsPeriodicTick = Data.EffectSpec.GetPeriod() > 0.f;
			const FHitResult* HitResult = bIsPeriodicTick ? nullptr : Context.GetHitResult();

			ULOACombatLibrary::NotifyDamageDealt(
				Context.GetInstigator(),
				Data.Target.GetAvatarActor(),
				IncomingDamage,
				HitResult);

			// ⑤ 체력이 처음 0이 됐으면 사망 이벤트 (받는 쪽이 무엇을 할지는 이 클래스가 모른다)
			SendDeathEventIfJustDied(Data, IncomingDamage);

			// (나중에) 피격 이벤트
		}
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
	Payload.EventTag = LOAGameplayTags::Event_Death;
	Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
	Payload.Target = Data.Target.GetAvatarActor();
	Payload.EventMagnitude = LastDamage;
	Payload.ContextHandle = Data.EffectSpec.GetContext();

	LOA_LOG(Payload.Target.Get(), Log, TEXT("[Death] Health reached 0. Sending Event.Death (killer: %s)"),
		*GetNameSafe(Payload.Instigator.Get()));

	// 이 ASC의 구독자에게 알린다. Trigger가 Event.Death인 Ability(GA_Death)가 여기서 바로 발동한다.
	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
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
