// LOADamageExecution.cpp

#include "AbilitySystem/Executions/LOADamageExecution.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Boss.h"
#include "Library/LOACombatLibrary.h"
#include "Library/LOACombatSettings.h"
#include "Core/LOAGameplayTags.h"
#include "AbilitySystemComponent.h"

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

		/** 공격자의 백 / 헤드 어택 추가 피해 (각인·버프로 더해지는 몫, 기본 0). 맞는 순간 값 (apply119) */
		FGameplayEffectAttributeCaptureDefinition ExtraBackAttackDamageDef;
		FGameplayEffectAttributeCaptureDefinition ExtraHeadAttackDamageDef;

		/** 공격자의 주는 피해 증가 (버프, 기본 0). 맞는 순간 값 (2026-11-10, apply119) */
		FGameplayEffectAttributeCaptureDefinition OutgoingDamageBonusDef;

		/** 맞는 쪽의 받는 피해 증가 (낙인 등, 기본 0). 맞는 순간의 값 (2026-11-07) */
		FGameplayEffectAttributeCaptureDefinition IncomingDamageBonusDef;

		/** 공격자의 치명타 피해 추가분 (각인·버프, 기본 0). 맞는 순간 값 (2026-11-13, apply119) */
		FGameplayEffectAttributeCaptureDefinition ExtraCritDamageDef;

		FLOADamageStatics()
		{
			AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetAttackPowerAttribute(),	// 어떤 스탯
				EGameplayEffectAttributeCaptureSource::Source,		// 누구에게서: 공격자 (Spec을 만든 쪽)
				false);												// 언제: 데미지가 계산되는 순간 (맞는 순간 걸린 버프. apply119, 헤더 [스탯을 읽는 시점])

			ExtraBackAttackDamageDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetExtraBackAttackDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, false);
			ExtraHeadAttackDamageDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetExtraHeadAttackDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, false);
			OutgoingDamageBonusDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetOutgoingDamageBonusAttribute(), EGameplayEffectAttributeCaptureSource::Source, false);
			IncomingDamageBonusDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetIncomingDamageBonusAttribute(),	// 어떤 스탯 (2026-11-13 Common → Combat. 맞는 쪽 모두 Combat을 가진다)
				EGameplayEffectAttributeCaptureSource::Target,				// 누구에게서: 맞는 쪽
				false);														// 언제: 데미지가 계산되는 순간 (맞는 순간 걸려 있는 낙인)
			ExtraCritDamageDef = FGameplayEffectAttributeCaptureDefinition(
				ULOAAttributeSet_Combat::GetExtraCritDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, false);
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
	RelevantAttributesToCapture.Add(DamageStatics().ExtraBackAttackDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().ExtraHeadAttackDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().OutgoingDamageBonusDef);
	RelevantAttributesToCapture.Add(DamageStatics().IncomingDamageBonusDef);
	RelevantAttributesToCapture.Add(DamageStatics().ExtraCritDamageDef);
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

	// ④ 백 / 헤드 어택 보너스 (2026-10-25)
	//    그 속성이 붙은 스킬로 맞은 쪽 뒤 / 앞에서 쳤을 때만.
	//    보너스 = 모든 직업 공통 기본값(Project Settings → LOA Combat) + 공격자의 추가분(Combat 스탯, 각인·버프. 기본 0)
	//    예) 300 × (1 + 0.05 + 0) = 315 (백) / 300 × (1 + 0.20 + 0) = 360 (헤드) / 옆이면 300
	//        기습의 대가로 추가분 0.25 → 300 × (1 + 0.05 + 0.25) = 390
	//    (2026-11-02) 뒤인지 앞인지는 여기서 정하지 않는다. ApplyPayload가 정해 Effect Context에 적은 결과를 읽기만 한다.
	//      공격자 판정은 "내 화면에서 본 보스 자리"로 정해야 하는데, 그 자리는 판정하는 쪽만 안다.
	//    데미지 숫자(ULOAAttributeSet_Common)도 같은 칸을 읽는다 → 숫자 옆 글자와 보너스가 어긋나지 않는다.
	float DirectionalBonus = 0.f;
	const ULOACombatSettings* CombatSettings = GetDefault<ULOACombatSettings>();
	float ExtraBonus = 0.f;
	switch (ULOACombatLibrary::GetDirectionalAttackFromContext(Spec.GetContext()))
	{
	case ELOADirectionalAttack::Back:
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ExtraBackAttackDamageDef, EvaluateParams, ExtraBonus);
		DirectionalBonus = CombatSettings->BaseBackAttackDamageBonus + FMath::Max(ExtraBonus, 0.f);
		break;
	case ELOADirectionalAttack::Head:
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ExtraHeadAttackDamageDef, EvaluateParams, ExtraBonus);
		DirectionalBonus = CombatSettings->BaseHeadAttackDamageBonus + FMath::Max(ExtraBonus, 0.f);
		break;
	default:
		break;
	}

	// ⑤-1 공격자의 주는 피해 증가 (아이덴티티 오라 버프 등, 2026-11-10)
	//    스탯창이 없거나 버프가 없으면 0 → 그대로.  예) 300, +0.15 → 345
	float OutgoingDamageBonus = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().OutgoingDamageBonusDef, EvaluateParams, OutgoingDamageBonus);

	// ⑤ 맞는 쪽의 받는 피해 증가 (낙인 등, 2026-11-07)
	//    맞는 쪽에 스탯이 없거나 디버프가 없으면 0 → 그대로. 1 + 값이 0 아래로 가지 않게 (나중에 받피감이 생겨도 데미지가 음수가 되지 않게)
	//    예) 300, 낙인 +0.1 → 330
	float IncomingDamageBonus = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().IncomingDamageBonusDef, EvaluateParams, IncomingDamageBonus);

	// ⑤-1 · ⑤ 곱하기는 공용 함수 하나로 (레이드 기록의 조력 역산과 같은 공식, apply119)
	const float BuffMultiplier = CalcBuffMultiplier(OutgoingDamageBonus, IncomingDamageBonus);

	// ⑤-2 치명타 (2026-11-13)
	//    치명타였나는 여기서 정하지 않는다. ApplyPayloads가 굴려 Effect Context(공격 판정 기록)에 적은 결과를 읽기만 한다
	//      (데미지 숫자도 같은 칸을 읽는다 → 노란 숫자와 배율이 어긋나지 않는다. 백 / 헤드와 같은 방식)
	//    배율 = 모든 직업 공통 기본(Project Settings → LOA Combat, 2.0 = 200%) + 공격자 추가분(Combat 스탯, 기본 0)
	//    예) 300, 치명타 → 300 × 2.0 = 600 / 각인 +0.5 → 300 × 2.5 = 750
	float CritMultiplier = 1.f;
	if (ULOACombatLibrary::GetAttackJudgeRecordFromContext(Spec.GetContext()).bCritical)
	{
		float ExtraCritDamage = 0.f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ExtraCritDamageDef, EvaluateParams, ExtraCritDamage);
		CritMultiplier = CombatSettings->BaseCritDamage + FMath::Max(ExtraCritDamage, 0.f);
	}

	// ⑤-3 대난투 (2026-11-18 apply125). 맞는 쪽이 대난투 진행 중(State.Brawl.Active)인 보스면
	//    배율: × 설정 값 (기본 2.0). 다른 배율과 곱한다 (사용자: 곱하기). 예) 300, 낙인 +10% → 330 → × 2.0 = 660
	//    입구: 체력(Common의 Damage)이 아니라 보스 스탯창의 대난투 입구(BrawlDamage) → 체력은 원래 차원에 돌아온 뒤 한 번에 (⑦)
	//    맞는 쪽 태그는 엔진이 이 계산 직전에 모아 둔다 (EvaluateParams.TargetTags = Spec.CapturedTargetTags)
	const bool bBrawl = EvaluateParams.TargetTags && EvaluateParams.TargetTags->HasTagExact(LOAGameplayTags::State_Brawl_Active);
	const float BrawlMultiplier = bBrawl ? CombatSettings->BrawlDamageMultiplier : 1.f;

	// ⑥ 최종 데미지
	//    (나중에) 피격자 방어력을 여기에 추가한다.
	const float FinalDamage = AttackPower * DamageRate * ChargeMultiplier * (1.f + DirectionalBonus) * BuffMultiplier * CritMultiplier * BrawlMultiplier;
	if (FinalDamage <= 0.f)
	{
		return;
	}

	// ⑦ 피격자의 Damage(메타)에 더한다. Health를 직접 깎지 않는다.
	//    → 피격자 AttributeSet이 Damage를 받아서 보호막/체력/사망을 처리한다.
	//    대난투 중 보스(⑤-3)는 보스 스탯창의 BrawlDamage(메타)에 → 체력은 그대로, 알림만 (ULOAAttributeSet_Boss)
	const FGameplayAttribute OutputAttribute = bBrawl
		? ULOAAttributeSet_Boss::GetBrawlDamageAttribute()
		: ULOAAttributeSet_Common::GetDamageAttribute();
	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(OutputAttribute, EGameplayModOp::Additive, FinalDamage));
}

float ULOADamageExecution::CalcBuffMultiplier(float OutgoingDamageBonus, float IncomingDamageBonus)
{
	// 주는 쪽: 1 + 값 (음수 버프는 0으로) / 받는 쪽: 1 + 값이 0 아래로 가지 않게 (받피감이 생겨도 데미지가 음수가 되지 않게)
	const float OutgoingMultiplier = 1.f + FMath::Max(OutgoingDamageBonus, 0.f);
	const float IncomingMultiplier = FMath::Max(1.f + IncomingDamageBonus, 0.f);
	return OutgoingMultiplier * IncomingMultiplier;
}
