// LOAGE_Kill.cpp

#include "AbilitySystem/Effects/LOAGE_Kill.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_Kill)

ULOAGE_Kill::ULOAGE_Kill()
{
	// 한 번 바꾸고 끝 (붙어 있지 않음)
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// "Health를 0으로 덮어쓴다"
	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = ULOAAttributeSet_Common::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Override;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));

	Modifiers.Add(HealthModifier);
}
