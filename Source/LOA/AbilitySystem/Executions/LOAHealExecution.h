// LOAHealExecution.h
// [배관] 회복 계산식 (ExecCalc). 회복 GE(GE_Heal 등)의 Executions에 지정해서 쓴다. (2026-11-07)
// 보호막은 여기가 아니다: GE_Shield의 Modifier 계산(ULOAShieldMagnitude). 보호막은 스킬마다 따로 양·시간을 가져야 해서 기간형 GE의 몫으로 한다
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (데미지 계산 ULOADamageExecution과 같은 모양)
//
//   ULOACombatLibrary::ApplyPayload
//      Payload의 값(SetByCaller.HealRate)을 GE Spec에 넣어 아군에게 적용
//         ▼ [서버]
//   ULOAHealExecution::Execute_Implementation   ← 이 클래스
//      힐러의 AttackPower(Combat 스탯창) × HealRate × 차지 배율 → 받는 쪽의 Healing(메타)
//      예) 공격력 1000, HealRate 0.3 → 300 회복
//         ▼
//   ULOAAttributeSet_Common::PostGameplayEffectExecute (받는 쪽)
//      Healing → 꺼내 0으로 비우고 Health + (최대 체력까지)
//
// [공격력 = 힐러 쪽] (사용자 결정 2026-11-07: 회복량 = 힐러 공격력 × 배율)
//   데미지와 같은 공격력을 읽으므로 공격력 버프가 회복에도 그대로 들어간다.
//   Snapshot = true : Spec을 만든 순간의 공격력 (데미지 계산과 같음)
//
// [네트워크] 서버만 계산한다. 결과(Health)는 Common 스탯창 복제로 모두에게 간다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "LOAHealExecution.generated.h"

UCLASS()
class LOA_API ULOAHealExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:
	/** 어떤 스탯을 누구에게서 읽을지(캡처) 등록한다. */
	ULOAHealExecution();

	/** [서버] 회복 GE가 적용될 때 호출. 공격력 × 배율을 받는 쪽의 Healing에 넣는다. */
	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
