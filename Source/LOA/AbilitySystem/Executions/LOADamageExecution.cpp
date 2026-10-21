// LOADamageExecution.cpp

#include "AbilitySystem/Executions/LOADamageExecution.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOADamageExecution)

// =============================================================================
// 캡처 정의: "어떤 스탯을, 누구에게서, 언제 읽을지"
// =============================================================================
// 캡처 정의는 모든 데미지 계산에서 똑같으므로 한 번만 만들어 두고 계속 쓴다.
// (함수 안의 static 변수: 처음 부를 때 한 번만 만들어지고 프로그램이 끝날 때까지 남는다)
namespace
{
	struct FLOADamageStatics
	{
		/** 공격자의 공격력 */
		FGameplayEffectAttributeCaptureDefinition AttackPowerDef;

		FLOADamageStatics()
		{
			AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetAttackPowerAttribute(),	// 어떤 스탯
				EGameplayEffectAttributeCaptureSource::Source,		// 누구에게서: 공격자 (Spec을 만든 쪽)
				true);												// 언제: Spec을 만든 순간 (Snapshot)
		}
	};

	const FLOADamageStatics& DamageStatics()
	{
		static FLOADamageStatics Statics;
		return Statics;
	}
}

ULOADamageExecution::ULOADamageExecution()
{
	// 이 목록에 넣은 스탯만 GAS가 Spec에 캡처해 둔다.
	// 빠뜨리면 Execute에서 공격력을 읽으려 해도 값이 없어서 실패한다.
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
}

void ULOADamageExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// 스탯을 읽을 때 쓰는 조건. (나중에 "보스에게만 +10%" 같은 태그 조건부 버프가 생기면 이 태그로 걸러진다)
	FAggregatorEvaluateParameters EvaluateParams;
	EvaluateParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// ① 공격자의 공격력 읽기
	//    공격자에게 Combat 스탯창이 없으면(장판 같은 환경 데미지 등) 0으로 남는다.
	float AttackPower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvaluateParams, AttackPower);
	AttackPower = FMath::Max(AttackPower, 0.f);

	// ② 스킬 배율 읽기 (ULOACombatLibrary::ApplyPayload가 Payload의 값으로 넣어 준 것)
	const float DamageRate = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_DamageRate, true, 0.f);

	// ③ 차지 배율 (ULOACombatLibrary::ApplyPayload가 항상 넣어 준다. 차지 스킬이 아니면 1)
	//    값이 없으면(라이브러리를 거치지 않은 GE) 1로 읽는다. 경고도 띄우지 않는다.
	const float ChargeMultiplier = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ChargeMultiplier, false, 1.f);

	// ④ 최종 데미지
	//    (나중에) 피격자 방어력, 치명타, 백어택 보정을 여기에 추가한다.
	const float FinalDamage = AttackPower * DamageRate * ChargeMultiplier;
	if (FinalDamage <= 0.f)
	{
		return;
	}

	// ⑤ 피격자의 Damage(메타)에 더한다. Health를 직접 깎지 않는다.
	//    → 피격자 AttributeSet이 Damage를 받아서 보호막/체력/사망을 처리한다.
	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(ULOAAttributeSet_Common::GetDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
}
