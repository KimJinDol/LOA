// LOAGE_IdentityDrain.h
// [배관] 공용: 아이덴티티가 켜져 있는 동안 붙는 GE. 게이지를 깎는다. (2026-11-09)
//
//   규칙(이 클래스) : 무한 지속 / 0.1초마다 IdentityGauge에 Add / 값은 SetByCaller.IdentityGain (음수)
//   값·태그(GA)     : ULOAGA_Identity가 "−초당 감소량 × 0.1"을 넣고, State.Identity.Active를 Spec에 붙인다
//   떼는 곳         : ULOAGA_Identity::EndAbility (끔 / 0이 됨 / 죽음)
//   직업과 무관. 켜진 동안 무엇을 하나(오라 회복 등)는 ULOAGA_Identity 자식이 정한다
//
// [0.1초인 이유] 게이지가 화면에서 부드럽게 줄게. 1초면 10씩 뚝뚝 떨어진다
// [태그를 Spec에 붙이는 이유] 쿨타임 GE와 같다. 직업마다 아이덴티티 태그가 달라질 수 있어서 GE에 고정하지 않는다

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_IdentityDrain.generated.h"

UCLASS()
class LOA_API ULOAGE_IdentityDrain : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_IdentityDrain();

	/** 감소 간격 (초) */
	static constexpr float DrainPeriod = 0.1f;
};
