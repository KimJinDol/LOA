// LOADamageExecution.h
// [배관] 데미지 계산식 (ExecCalc). 데미지 GE(GE_Damage_Basic 등)의 Executions에 지정해서 쓴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   ULOACombatLibrary::ApplyPayload
//      Payload의 값(SetByCaller.DamageRate = 스킬 배율, 예: 3.0)을 데미지 GE Spec에 넣어 적에게 적용
//         ▼ [서버]
//   ULOADamageExecution::Execute_Implementation   ← 이 클래스
//      공격자의 AttackPower(Combat 스탯창) × DamageRate
//      → 결과를 피격자의 Damage(메타, Common 스탯창)에 더한다
//         ▼
//   ULOAAttributeSet_Common::PostGameplayEffectExecute (피격자)
//      Damage를 꺼내 0으로 비우고 → Health 감소 (0 아래로 안 내려감)
//
// ─────────────────────────────────────────────────────────────────────────────
// [계산(여기)과 적용(받는 쪽 AttributeSet)을 나누는 이유]
//   여기     : "얼마나 아픈가"  — 공격자와 피격자의 스탯을 "둘 다" 읽을 수 있는 유일한 곳
//              (나중에) 피격자 방어력, 치명타, 백어택 보정도 여기에 추가한다.
//   받는 쪽  : "어떻게 받는가" — 보호막 먼저, 체력, 사망 판정, 피격 알림
//
// ─────────────────────────────────────────────────────────────────────────────
// [공격력을 읽는 시점 - Snapshot]
// AttackPower는 Snapshot = true : 데미지 Spec을 "만든" 순간의 공격력.
//   지금은 Spec을 타격 노티파이(히트 순간)에서 만들므로 히트 순간 값과 같다.
//   로아처럼 "발동 순간의 공격력"을 쓰려면 Spec을 CommitAbility 시점에 만들도록 옮긴다. (버프 작업 때)
// 나중에 추가할 치명타 확률 등은 Snapshot = false : 맞는 순간의 값.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "LOADamageExecution.generated.h"

UCLASS()
class LOA_API ULOADamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:
	/** 어떤 스탯을 누구에게서 읽을지(캡처) 등록한다. */	ULOADamageExecution();

	/** [서버] 데미지 GE가 적용될 때 호출. 공격력 × 배율을 계산해 피격자의 Damage에 넣는다. */
	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};