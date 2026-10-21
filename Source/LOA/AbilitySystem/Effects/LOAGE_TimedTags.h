// LOAGE_TimedTags.h
// [배관] 공용 "정해진 시간 동안 태그 부여" GE. 값을 바꾸지 않고 태그만 붙였다가 시간이 지나면 사라진다.
//
//   규칙(이 클래스) : HasDuration / 시간은 SetByCaller.Duration / Modifier 없음
//   값(쓰는 쪽)     : 시간 → SetByCaller.Duration
//                     태그 → Spec의 DynamicGrantedTags에 추가 (붙어 있는 동안 ASC가 이 태그를 가짐)
//
// 쓰는 곳: ULOAAnimNotifyState_GrantTags (몽타주 구간 동안 면역/슈퍼아머/카운터 가능 태그)
//
// 쿨타임 GE(ULOAGE_Cooldown)와 구조는 같지만 클래스를 나눈다.
// HUD가 "쿨타임 GE인지"를 클래스로 구분하기 때문. (같은 클래스를 쓰면 면역 태그가 쿨타임으로 보임)
// C++ 클래스로 만든 이유는 ULOAGE_ManaCost와 같다. (조정할 숫자가 없는 배관용 GE)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_TimedTags.generated.h"

UCLASS()
class LOA_API ULOAGE_TimedTags : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_TimedTags();
};
