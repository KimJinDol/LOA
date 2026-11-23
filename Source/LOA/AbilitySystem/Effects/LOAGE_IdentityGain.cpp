// LOAGE_IdentityGain.cpp

#include "AbilitySystem/Effects/LOAGE_IdentityGain.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "Core/LOAGameplayTags.h"

#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_IdentityGain)

ULOAGE_IdentityGain::ULOAGE_IdentityGain()
{
	// 한 번 바꾸고 끝
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// "IdentityGauge에 SetByCaller.IdentityGain을 더한다" (깎을 때는 음수)
	FSetByCallerFloat GainValue;
	GainValue.DataTag = LOAGameplayTags::SetByCaller_IdentityGain;

	FGameplayModifierInfo GaugeModifier;
	GaugeModifier.Attribute = ULOAAttributeSet_Player::GetIdentityGaugeAttribute();
	GaugeModifier.ModifierOp = EGameplayModOp::Additive;
	GaugeModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(GainValue);

	Modifiers.Add(GaugeModifier);
}

void ULOAGE_IdentityGain::ApplyGaugeDelta(UAbilitySystemComponent* ASC, float Delta)
{
	// 게이지는 서버만 바꾼다 (예측하지 않음: 늦어도 복제 한 번이면 화면이 맞는다)
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || FMath::IsNearlyZero(Delta))
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_IdentityGain, Delta);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}
