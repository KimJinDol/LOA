// LOAShieldMagnitude.cpp

#include "AbilitySystem/Calculations/LOAShieldMagnitude.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAShieldMagnitude)

ULOAShieldMagnitude::ULOAShieldMagnitude()
{
	AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
		ULOAAttributeSet_Combat::GetAttackPowerAttribute(),	// 어떤 스탯
		EGameplayEffectAttributeCaptureSource::Source,		// 누구에게서: 힐러 (Spec을 만든 쪽)
		true);												// 언제: Spec을 만든 순간 (Snapshot)

	// 이 목록에 넣은 스탯만 GAS가 Spec에 캡처해 둔다
	RelevantAttributesToCapture.Add(AttackPowerDef);
}

float ULOAShieldMagnitude::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// 스탯을 읽을 때의 조건 (태그 조건 붙은 버프를 거른다. 데미지 계산과 같음)
	FAggregatorEvaluateParameters EvaluateParams;
	EvaluateParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float AttackPower = 0.f;
	GetCapturedAttributeMagnitude(AttackPowerDef, Spec, EvaluateParams, AttackPower);
	AttackPower = FMath::Max(AttackPower, 0.f);

	// 스킬 데이터의 배율 (없으면 경고 + 0), 차지 배율 (없으면 1), 깎인 양 (처음엔 없음 → 0)
	const float ShieldRate = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ShieldRate, true, 0.f);
	const float ChargeMultiplier = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ChargeMultiplier, false, 1.f);
	const float Used = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ShieldUsed, false, 0.f);

	return FMath::Max(AttackPower * ShieldRate * ChargeMultiplier - Used, 0.f);
}
