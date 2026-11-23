// LOAAttributeSet_Player.cpp

#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAttributeSet_Player)

ULOAAttributeSet_Player::ULOAAttributeSet_Player()
{
	// 초기값을 여기서 넣지 않는다. (하드코딩 금지) 마나는 직업 초기화 GE가 채운다. 낙인력은 기본 0
}

void ULOAAttributeSet_Player::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 전부 본인에게만. (다른 사람 마나는 화면에 안 보임 → 네트워크 절약. 낙인력은 서버 계산만 쓰고 본인 스탯창 표시용)
	// REPNOTIFY_Always : 값이 같아도 OnRep 호출. 클라가 예측으로 먼저 깎아 둔 뒤 서버 값이 "같게" 도착해도 알림이 필요하다.
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, Mana,      COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, MaxMana,   COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, ManaRegen, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, MarkPower, COND_OwnerOnly, REPNOTIFY_Always);
	// 아이덴티티 게이지도 본인 화면에만 보인다 (파티원 게이지 표시가 생기면 그때 바꿈)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, IdentityGauge, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, MaxIdentityGauge, COND_OwnerOnly, REPNOTIFY_Always);

	// 이동속도 증가: 본인에게만. 내 컴퓨터의 이동 예측이 서버와 같은 값을 써야 한다 (2026-11-13)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, MoveSpeedBonus, COND_OwnerOnly, REPNOTIFY_Always);

	// 치명타 적중률 추가분: 본인에게만 (나중에 스탯창 표시용. 굴리기는 서버)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Player, ExtraCritRate, COND_OwnerOnly, REPNOTIFY_Always);
}

void ULOAAttributeSet_Player::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetManaAttribute())
	{
		// 최대 마나가 아직 0이면(초기화 도중) 위쪽 제한을 걸지 않는다.
		// 초기화 GE의 순서가 "마나 → 최대 마나"여도 마나가 0으로 잘리지 않게. (Common의 체력과 같은 이유)
		const float Max = GetMaxMana();
		NewValue = (Max > 0.f) ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxManaAttribute() || Attribute == GetManaRegenAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMarkPowerAttribute())
	{
		// 낙인력 감소 디버프가 커도 0까지만 (기본 낙인 아래로 깎이지 않게)
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetIdentityGaugeAttribute())
	{
		// 0 ~ 최대. 최대가 아직 0이면(초기화 전) 위쪽은 묶지 않는다 (마나와 같은 이유)
		const float Max = GetMaxIdentityGauge();
		NewValue = (Max > 0.f) ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxIdentityGaugeAttribute() || Attribute == GetMoveSpeedBonusAttribute() || Attribute == GetExtraCritRateAttribute())
	{
		// 이동속도 증가·치명타 적중률 추가분도 0까지만 (감소는 지금 다루지 않는다, 2026-11-13)
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void ULOAAttributeSet_Player::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// [서버에서만 호출됨] PreAttributeChange는 보여지는 값만 맞춘다. 여기서는 저장된 기본값까지 맞춘다.
	if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		const float Max = GetMaxMana();
		SetMana((Max > 0.f) ? FMath::Clamp(GetMana(), 0.f, Max) : FMath::Max(GetMana(), 0.f));
	}
	else if (Data.EvaluatedData.Attribute == GetIdentityGaugeAttribute())
	{
		// 수급(+)·켜진 동안 감소(0.1초마다 −)·환급 모두 여기를 지난다. 기본값을 0 ~ 최대로 맞춘다
		const float Max = GetMaxIdentityGauge();
		SetIdentityGauge((Max > 0.f) ? FMath::Clamp(GetIdentityGauge(), 0.f, Max) : FMath::Max(GetIdentityGauge(), 0.f));
	}
}

// ---------- 복제 도착 알림 ----------
// GAMEPLAYATTRIBUTE_REPNOTIFY : GAS에게 "서버 값이 도착했다"고 알린다. (UI 알림, 예측 값 보정)

void ULOAAttributeSet_Player::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, Mana, OldValue);
}

void ULOAAttributeSet_Player::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, MaxMana, OldValue);
}

void ULOAAttributeSet_Player::OnRep_ManaRegen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, ManaRegen, OldValue);
}

void ULOAAttributeSet_Player::OnRep_MarkPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, MarkPower, OldValue);
}

void ULOAAttributeSet_Player::OnRep_IdentityGauge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, IdentityGauge, OldValue);
}

void ULOAAttributeSet_Player::OnRep_MaxIdentityGauge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, MaxIdentityGauge, OldValue);
}

void ULOAAttributeSet_Player::OnRep_MoveSpeedBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, MoveSpeedBonus, OldValue);
}

void ULOAAttributeSet_Player::OnRep_ExtraCritRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Player, ExtraCritRate, OldValue);
}
