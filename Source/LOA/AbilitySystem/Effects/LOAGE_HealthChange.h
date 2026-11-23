// LOAGE_HealthChange.h
// [배관] 체력 바꾸기 GE. 체력에 SetByCaller.HealthChange를 더한다 (Instant, Health Add). 깎을 때는 음수로 넣는다. (2026-11-18 apply125)
//
// 쓰는 곳: 대난투가 끝나 원래 차원에 돌아온 뒤, 대난투 누적 피해를 보스 체력에 한 번에 (ALOABrawlArena, 값 = −누적)
//
// [왜 데미지 GE가 아닌가]
//   데미지 입구(Common의 Damage)로 넣으면 데미지 숫자·레이드 기록이 한 번 더 나간다 (대난투 중에 이미 나감)
//   → 체력(Health)을 직접 바꾼다. Common 스탯창의 체력 줄이 범위를 맞추고, 처음 0이면 사망 이벤트를 보낸다
//     (ULOAAttributeSet_Common::PostGameplayEffectExecute의 Health 줄 = 낙사 ULOAGE_Kill과 같은 길)
// [왜 ApplyModToAttribute가 아닌가] 값만 바꾸고 실행 경로를 안 거쳐 사망 이벤트가 안 나간다 (ULOAGE_Kill 헤더, 엔진 확인)
//
// 막타(Instigator)는 적용하는 쪽이 Effect Context에 넣는다 (대난투: 마지막으로 친 사람)
// C++ 클래스로 만든 이유는 ULOAGE_Kill과 같다 (조정할 숫자가 없는 배관용 GE)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_HealthChange.generated.h"

UCLASS()
class LOA_API ULOAGE_HealthChange : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_HealthChange();
};
