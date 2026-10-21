// LOAGE_Cooldown.h
// [배관] 공용 쿨타임 GE. 모든 Ability(플레이어 스킬, 보스 패턴)가 이 GE 하나로 쿨타임을 돌린다.
//
//   규칙(이 클래스) : HasDuration / 시간은 SetByCaller.Cooldown / 값은 바꾸지 않음
//   값(Ability)     : ULOAGameplayAbility::ApplyCooldown이 넣음
//                       시간 → SetByCaller.Cooldown = CooldownDuration
//                       태그 → Spec에 CooldownTags를 추가로 붙임 (붙어 있는 동안 ASC가 이 태그를 가짐)
//
// C++ 클래스로 만든 이유는 ULOAGE_ManaCost와 같다. (조정할 숫자가 없는 배관용 GE)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_Cooldown.generated.h"

UCLASS()
class LOA_API ULOAGE_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_Cooldown();
};
