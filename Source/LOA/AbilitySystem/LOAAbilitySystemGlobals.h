// LOAAbilitySystemGlobals.h
// [배관] GAS 전체 설정 객체의 우리 버전. 지금 하는 일은 하나: Effect Context를 우리 쪽지(FLOAGameplayEffectContext)로 만든다. (2026-11-02)
//
// [엔진에 알리기]
//   DefaultGame.ini
//     [/Script/GameplayAbilities.GameplayAbilitiesDeveloperSettings]
//     AbilitySystemGlobalsClassName=/Script/LOA.LOAAbilitySystemGlobals
//   (= Project Settings → Game → Gameplay Abilities Settings → Ability System Globals Class Name)
//   엔진 코드 확인: IGameplayAbilitiesModule::GetAbilitySystemGlobals가 이 설정의 클래스로 객체를 하나 만든다 (GameplayAbilitiesModule.cpp)
//   클래스 이름이 틀리면 엔진이 시작할 때 멈춘다 (checkf). 이름을 바꾸면 ini도 같이.
//
// [네트워크] 컴퓨터마다 하나씩 있다 (복제 없음). 쪽지를 만드는 규칙만 정한다.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "LOAAbilitySystemGlobals.generated.h"

UCLASS()
class LOA_API ULOAAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

public:
	/** ASC->MakeEffectContext()가 부른다 → 우리 쪽지 */
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
