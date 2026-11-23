// LOAStunDurationMagnitude.h
// [배관] 스턴 시간 계산기 (MMC). ULOAGE_Stun의 Duration Magnitude에 C++ 기본값으로 들어간다. (2026-11-10)
//
// [계산] Payload의 SetByCaller.Duration이 있으면 그 값, 없으면 그 GE의 Default Duration
//   예) GE_CC_Stun (Default 2) + Payload Duration 3 → 3초 / Payload에 없으면 → 2초
//
// [왜 계산기인가] 엔진의 SetByCaller 칸은 "없으면 기본값"이 안 된다 (없으면 오류 + 0). 공격마다 바꾸고 싶은 것과
//   자주 안 바뀌는 기본값을 같이 가지려고 (경직의 Default Duration과 같은 규칙)
//
// [다시 계산되는 때] 엔진이 GE를 붙일 때 시간을 다시 계산한다 → Spec을 만든 뒤 넣은 SetByCaller도 반영된다
//
// [네트워크] 서버 (GE 적용은 서버)

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "LOAStunDurationMagnitude.generated.h"

UCLASS()
class LOA_API ULOAStunDurationMagnitude : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	//~ Begin UGameplayModMagnitudeCalculation Interface
	/** SetByCaller.Duration, 없으면 ULOAGE_Stun::DefaultDuration */
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
	//~ End UGameplayModMagnitudeCalculation Interface
};
