// LOAGE_IdentityGain.h
// [배관] 공용 아이덴티티 게이지 변화 GE. (2026-11-09)
//
//   규칙(이 클래스) : Instant / IdentityGauge에 Add / 값은 SetByCaller.IdentityGain
//   쓰는 곳         : 스킬 수급 (+, ULOAPlayerAbility::GrantIdentityGain)
//                     끌 때 환급·죽을 때 비우기 (−, ULOAGA_Identity::EndAbility)
//   범위는 스탯창이 0 ~ 최대로 맞춘다 (ULOAAttributeSet_Player)
//
// [왜 C++ GE인가] 마나 소모 GE(ULOAGE_ManaCost)와 같은 이유. 조정할 숫자가 없고(전부 SetByCaller) 규칙도 바뀌지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_IdentityGain.generated.h"

class UAbilitySystemComponent;

UCLASS()
class LOA_API ULOAGE_IdentityGain : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_IdentityGain();

	/**
	 * [서버] 이 ASC의 아이덴티티 게이지를 Delta만큼 바꾼다. (+ 수급, − 감소)
	 * 클라이언트에서 부르거나 Delta가 0이면 아무것도 안 한다. 결과는 스탯창 복제로 본인에게 간다
	 */
	static void ApplyGaugeDelta(UAbilitySystemComponent* ASC, float Delta);
};
