// LOAHealExecution.cpp

#include "AbilitySystem/Executions/LOAHealExecution.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Core/LOAGameplayTags.h"

#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAHealExecution)

namespace
{
	struct FLOAHealStatics
	{
		/** 힐러의 공격력. Spec을 만든 순간 (데미지 계산과 같음) */
		FGameplayEffectAttributeCaptureDefinition AttackPowerDef;

		FLOAHealStatics()
		{
			AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetAttackPowerAttribute(),	// 어떤 스탯
				EGameplayEffectAttributeCaptureSource::Source,		// 누구에게서: 힐러 (Spec을 만든 쪽)
				true);												// 언제: Spec을 만든 순간 (Snapshot)
		}
	};

	const FLOAHealStatics& HealStatics()
	{
		static FLOAHealStatics Statics;
		return Statics;
	}
}

ULOAHealExecution::ULOAHealExecution()
{
	RelevantAttributesToCapture.Add(HealStatics().AttackPowerDef);
}

void ULOAHealExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvaluateParams;
	EvaluateParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// ① 힐러의 공격력. Combat 스탯창이 없으면 0 → 아무것도 안 낸다
	float AttackPower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HealStatics().AttackPowerDef, EvaluateParams, AttackPower);
	AttackPower = FMath::Max(AttackPower, 0.f);

	// ② 배율 (ULOACombatLibrary::ApplyPayload가 Payload 값으로 넣어 준 것). 없으면 경고 + 0
	const float HealRate = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_HealRate, true, 0.f);

	// ③ 차지 배율 (데미지 계산과 같음. 차지 스킬이 아니면 1)
	const float ChargeMultiplier = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ChargeMultiplier, false, 1.f);

	// ④ 받는 쪽의 입구(메타)에 더한다. Health를 직접 바꾸지 않는다 → 받는 쪽 스탯창이 시체 확인·최대치 제한을 한 곳에서
	const float HealAmount = AttackPower * HealRate * ChargeMultiplier;
	if (HealAmount > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(ULOAAttributeSet_Common::GetHealingAttribute(), EGameplayModOp::Additive, HealAmount));
	}
}
