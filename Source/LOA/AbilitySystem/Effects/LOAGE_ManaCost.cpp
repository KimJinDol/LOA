// LOAGE_ManaCost.cpp

#include "AbilitySystem/Effects/LOAGE_ManaCost.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Mana.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_ManaCost)

ULOAGE_ManaCost::ULOAGE_ManaCost()
{
	// 한 번 바꾸고 끝 (붙어 있지 않음)
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// "Mana에 SetByCaller.ManaCost 값을 더한다"
	// 깎으려면 음수가 들어와야 한다. → ULOAPlayerAbility::ApplyCost가 -ManaCost를 넣는다.
	FSetByCallerFloat ManaCostValue;
	ManaCostValue.DataTag = LOAGameplayTags::SetByCaller_ManaCost;

	FGameplayModifierInfo ManaModifier;
	ManaModifier.Attribute = ULOAAttributeSet_Mana::GetManaAttribute();
	ManaModifier.ModifierOp = EGameplayModOp::Additive;
	ManaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(ManaCostValue);

	Modifiers.Add(ManaModifier);
}
