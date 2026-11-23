// LOAGE_HealthChange.cpp

#include "AbilitySystem/Effects/LOAGE_HealthChange.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_HealthChange)

ULOAGE_HealthChange::ULOAGE_HealthChange()
{
	// 한 번 바꾸고 끝 (붙어 있지 않음)
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// "Health에 SetByCaller.HealthChange를 더한다". 예) −9,000 → 체력 1,000,000 → 991,000
	//   값이 없으면 엔진이 0으로 보고 에러 로그를 남긴다 (넣는 쪽 실수를 알 수 있게)
	FSetByCallerFloat ChangeValue;
	ChangeValue.DataTag = LOAGameplayTags::SetByCaller_HealthChange;

	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = ULOAAttributeSet_Common::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(ChangeValue);

	Modifiers.Add(HealthModifier);
}
