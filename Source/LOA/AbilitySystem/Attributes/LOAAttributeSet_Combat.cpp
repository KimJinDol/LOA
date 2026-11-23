// LOAAttributeSet_Combat.cpp

#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAttributeSet_Combat)

ULOAAttributeSet_Combat::ULOAAttributeSet_Combat()
{
	// 초기값을 여기서 넣지 않는다. (하드코딩 금지)
	// 초기화 GE(AbilityDataSet)가 CT_ClassStats의 공격력 행을 읽어 채운다.
}

void ULOAAttributeSet_Combat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 공격력: 본인에게만 복제 (내 스탯창 표시용). 데미지 계산은 서버가 하므로 다른 사람은 몰라도 된다.
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Combat, AttackPower, COND_OwnerOnly, REPNOTIFY_Always);

	// 백 / 헤드 어택 추가 피해: 공격력과 같은 이유로 본인에게만 (나중에 스탯창 표시용)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Combat, ExtraBackAttackDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Combat, ExtraHeadAttackDamage, COND_OwnerOnly, REPNOTIFY_Always);

	// 주는 피해 증가: 같은 이유로 본인에게만 (버프 아이콘은 태그로 보이므로 남은 몰라도 된다)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Combat, OutgoingDamageBonus, COND_OwnerOnly, REPNOTIFY_Always);

	// 치명타 피해 추가분·치명타 저항: 같은 이유로 본인에게만 (굴리기·계산은 서버. 보스는 주인이 없어 아무에게도 안 감)
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Combat, ExtraCritDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Combat, CritResistance, COND_OwnerOnly, REPNOTIFY_Always);
}

void ULOAAttributeSet_Combat::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetAttackPowerAttribute())
	{
		// 공격력 감소 디버프가 아무리 커도 0까지만. (음수 공격력 → 음수 데미지 = 회복이 되는 사고 방지)
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetExtraBackAttackDamageAttribute() || Attribute == GetExtraHeadAttackDamageAttribute()
		|| Attribute == GetOutgoingDamageBonusAttribute() || Attribute == GetExtraCritDamageAttribute())
	{
		// 추가 피해 감소 디버프가 커도 0까지만. (기본 보너스 아래로 깎이지 않게)
		// 주는 피해 증가도 0까지만 (감소는 나중에 따로: 지금은 "증가"만 다룬다)
		// 치명타 피해 추가분도 0까지만 (기본 200% 아래로 깎이지 않게, 2026-11-13)
		// (치명타 저항은 막지 않는다: 약점 노출로 음수가 되는 게 정상)
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void ULOAAttributeSet_Combat::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Combat, AttackPower, OldValue);
}

void ULOAAttributeSet_Combat::OnRep_ExtraBackAttackDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Combat, ExtraBackAttackDamage, OldValue);
}

void ULOAAttributeSet_Combat::OnRep_ExtraHeadAttackDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Combat, ExtraHeadAttackDamage, OldValue);
}

void ULOAAttributeSet_Combat::OnRep_OutgoingDamageBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Combat, OutgoingDamageBonus, OldValue);
}

void ULOAAttributeSet_Combat::OnRep_ExtraCritDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Combat, ExtraCritDamage, OldValue);
}

void ULOAAttributeSet_Combat::OnRep_CritResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Combat, CritResistance, OldValue);
}
