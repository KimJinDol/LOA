// LOAShieldMagnitude.h
// [배관] 보호막 양 계산 (MMC = Modifier Magnitude Calculation, "Modifier의 숫자를 계산하는 작은 계산기"). (2026-11-07)
// GE_Shield의 Modifier(Shield, Add)에 Custom Calculation Class로 지정해서 쓴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [계산] 이 보호막 GE 하나의 몫 = 힐러 공격력 × ShieldRate × 차지 배율 − 깎인 양 (0 아래로 안 감)
//   힐러 공격력  : 힐러(Source)의 AttackPower. Spec을 만든 순간 값 (Snapshot, 데미지·회복 계산과 같음)
//                  계산기(Aggregator)로 읽으므로 태그 조건 붙은 공격력 버프도 걸러서 들어간다
//   ShieldRate   : 스킬 데이터 (Payload 값 SetByCaller.ShieldRate). 예) 0.5
//   차지 배율    : SetByCaller.ChargeMultiplier (차지 스킬이 아니면 1)
//   깎인 양      : SetByCaller.ShieldUsed. 처음 0. 맞을 때 ULOAAttributeSet_Common::ConsumeShield가 늘린다
//   예) 공격력 1000, ShieldRate 0.5 → 500 / 300 맞으면 깎인 양 300 → 200
//
// [다시 계산되는 때] 엔진이 부른다
//   GE가 붙을 때, 그리고 깎인 양을 고칠 때 (UpdateActiveGameplayEffectSetByCallerMagnitude → CalculateModifierMagnitudes, GameplayEffect.cpp 3667줄)
//   공격력은 Snapshot이라 다시 계산해도 처음 값 그대로 → "처음 양 − 깎인 양"이 된다
//
// [네트워크] 계산은 서버. 결과(Shield)는 Common 스탯창 복제로 모두에게

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "LOAShieldMagnitude.generated.h"

UCLASS()
class LOA_API ULOAShieldMagnitude : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	/** 힐러 공격력을 읽도록(캡처) 등록한다 */   
	ULOAShieldMagnitude();

	//~ Begin UGameplayModMagnitudeCalculation Interface
	/** 이 보호막 GE의 몫 = 공격력 × ShieldRate × 차지 배율 − 깎인 양 */
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
	//~ End UGameplayModMagnitudeCalculation Interface

private:
	/** 힐러의 공격력. Spec을 만든 순간 (Snapshot) */
	FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
};
