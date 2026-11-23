// LOAGE_Kill.h
// [배관] 즉사 GE. 체력을 0으로 덮어쓴다 (Instant, Health Override 0).
//
// 쓰는 곳: 낙사 (ALOACharacter::FellOutOfWorld). 나중에 전멸기 같은 "무조건 죽음"에도 쓸 수 있다.
//
// [왜 GE로 하나] (2026-10-23)
//   사망 알림(Event.Trigger.Death)은 스탯창의 PostGameplayEffectExecute에서 나간다 (ULOAAttributeSet_Common::SendDeathEventIfJustDied).
//   이 콜백은 GE가 "실행"될 때만 불린다.
//   ASC의 ApplyModToAttribute / ApplyModToAttributeUnsafe는 값만 바꾸고 실행 경로를 안 거친다 (엔진 코드 확인:
//   둘 다 FActiveGameplayEffectsContainer::ApplyModToAttribute → SetAttributeBaseValue만 함)
//   → 체력은 0인데 죽지 않는다. 그래서 GE로 적용한다.
//
// 데미지가 아니라서 데미지 숫자는 없다. 막타(Instigator)는 적용한 쪽 (낙사면 자기 자신).
// C++ 클래스로 만든 이유는 ULOAGE_ManaCost와 같다. (조정할 숫자가 없는 배관용 GE)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_Kill.generated.h"

UCLASS()
class LOA_API ULOAGE_Kill : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_Kill();
};
