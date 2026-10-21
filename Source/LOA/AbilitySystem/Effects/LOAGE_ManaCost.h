// LOAGE_ManaCost.h
// [배관] 공용 마나 소모 GE. 모든 플레이어 스킬이 이 GE 하나로 마나를 깎는다.
//
//   규칙(이 클래스) : Instant / Mana에 Add / 값은 SetByCaller.ManaCost
//   값(Ability)     : ULOAPlayerAbility::ApplyCost가 -ManaCost를 Spec에 넣어 적용
//
// [왜 BP 에셋이 아니라 C++ 클래스인가]
// GE는 보통 기획자가 에디터에서 만지는 데이터(BP)다. 하지만 이 GE에는 조정할 숫자가 하나도 없고(전부 SetByCaller),
// 규칙도 바뀌지 않는다. 코드에 고정해 두면 스킬 BP마다 GE를 지정할 필요가 없고, 에셋 경로를 코드에 쓸 일도 없다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_ManaCost.generated.h"

UCLASS()
class LOA_API ULOAGE_ManaCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_ManaCost();
};
