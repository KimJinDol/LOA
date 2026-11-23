// LOAGE_IdentityDrain.cpp

#include "AbilitySystem/Effects/LOAGE_IdentityDrain.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_IdentityDrain)

ULOAGE_IdentityDrain::ULOAGE_IdentityDrain()
{
	// 뗄 때까지 붙어 있다
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 0.1초마다 한 번씩 적용. 붙는 순간에는 깎지 않는다 (켜자마자 99가 되지 않게)
	Period = FScalableFloat(DrainPeriod);
	bExecutePeriodicEffectOnApplication = false;

	// "IdentityGauge에 SetByCaller.IdentityGain을 더한다" (GA가 음수를 넣는다)
	FSetByCallerFloat DrainValue;
	DrainValue.DataTag = LOAGameplayTags::SetByCaller_IdentityGain;

	FGameplayModifierInfo GaugeModifier;
	GaugeModifier.Attribute = ULOAAttributeSet_Player::GetIdentityGaugeAttribute();
	GaugeModifier.ModifierOp = EGameplayModOp::Additive;
	GaugeModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DrainValue);

	Modifiers.Add(GaugeModifier);
}
