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
}

void ULOAAttributeSet_Combat::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetAttackPowerAttribute())
	{
		// 공격력 감소 디버프가 아무리 커도 0까지만. (음수 공격력 → 음수 데미지 = 회복이 되는 사고 방지)
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void ULOAAttributeSet_Combat::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Combat, AttackPower, OldValue);
}
