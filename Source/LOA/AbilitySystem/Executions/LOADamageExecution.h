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
//      공격자의 AttackPower(Combat 스탯창) × DamageRate × 차지 배율 × (1 + 백/헤드 어택 보너스: 기본값(LOA Combat 설정) + 공격자 추가분)
//      × (1 + 공격자의 주는 피해 증가: 아이덴티티 오라 버프 등, Combat 스탯창. 2026-11-10)
//      × (1 + 맞는 쪽의 받는 피해 증가: 낙인 등, Common 스탯창. 2026-11-07)
//      × 치명타 배율 × 대난투 배율 (맞는 쪽에 State.Brawl.Active가 있으면 LOA Combat 설정 값, 2026-11-18 apply125)
//      → 대난투 중 보스는 Damage 대신 보스 스탯창의 BrawlDamage(메타)에 (체력은 돌아온 뒤 한 번에, ALOABrawlArena)
//      → 결과를 피격자의 Damage(메타, Common 스탯창)에 더한다
//         ▼
//   ULOAAttributeSet_Common::PostGameplayEffectExecute (피격자)
//      Damage를 꺼내 0으로 비우고 → 보호막 먼저 → Health 감소 (0 아래로 안 내려감)
//
// ─────────────────────────────────────────────────────────────────────────────
// [계산(여기)과 적용(받는 쪽 AttributeSet)을 나누는 이유]
//   여기     : "얼마나 아픈가"  — 공격자와 피격자의 스탯을 "둘 다" 읽을 수 있는 유일한 곳
//              백 / 헤드 어택 보정 (2026-10-25). 뒤·앞 판단은 여기서 안 한다 — ApplyPayload가 Effect Context에 적은 결과를 읽어 곱하기만 (2026-11-02)
//              치명타 배율 (2026-11-13). 치명타였나는 여기서 안 굴린다 — 공격 판정 기록(Effect Context)의 결과를 읽어 곱하기만
//              (나중에) 피격자 방어력도 여기에 추가한다.
//   받는 쪽  : "어떻게 받는가" — 보호막 먼저, 체력, 사망 판정, 피격 알림
//
// ─────────────────────────────────────────────────────────────────────────────
// [스탯을 읽는 시점 - Snapshot] (2026-11-16 apply119: 전부 "맞는 순간", 사용자: 버프는 맞는 순간 기준)
// 캡처는 모두 Snapshot = false : 데미지가 계산되는 순간(맞는 순간)의 값.
//   때린 쪽 : 공격력, 백 / 헤드 어택 추가 피해, 주는 피해 증가(공증 등), 치명타 피해 추가분
//   맞은 쪽 : 받는 피해 증가(낙인 등)
//   엔진 (GameplayEffect.cpp): Snapshot = true는 Spec을 만들 때 값을 복사해 고정(TakeSnapshotOf), false는 그 ASC의 값 묶음을 그대로 가리킨다.
//                              주기 GE(도트)는 틱마다 처음 Spec으로 이 계산을 다시 돌린다(InternalExecutePeriodicGameplayEffect)
//   → 도트 틱 · 날아가는 투사체도 맞는 순간 걸린 버프를 받는다. 예) 도트를 건 뒤 세라스 공증이 붙으면 다음 틱부터 +15%
//   전에는 때린 쪽이 true(Spec을 만든 순간)라 레이드 기록의 조력 역산(맞는 순간 GE)과 어긋났다 (도트 틱 사이에 붙은 공증에 없는 조력)
// 치명타 확률은 여기서 캡처하지 않는다: 굴리는 곳이 ULOACombatLibrary(공격 판정 기록)라 그 순간 ASC 값을 읽는다.
//
// [버프 배율 - CalcBuffMultiplier] (apply119)
//   (1 + 주는 피해 증가) × (1 + 받는 피해 증가)를 이 클래스의 static 함수 하나로 둔다.
//   레이드 기록의 조력 역산(ULOARaidRecordSubsystem::RecordAssist)도 같은 함수를 부른다 → 공식을 고치면 둘 다 바뀐다

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

	/**
	 * 버프 배율 = (1 + 주는 피해 증가, 최소 0) × (1 + 받는 피해 증가, 최소 0). 헤더 [버프 배율]
	 * 데미지 계산과 레이드 기록의 조력 역산이 같이 쓴다 (공식 한 곳, apply119)
	 * 예) 공증 +0.15, 낙인 +0.1 → 1.15 × 1.1 = 1.265
	 */
	static float CalcBuffMultiplier(float OutgoingDamageBonus, float IncomingDamageBonus);
};