// LOAGE_Stagger.h
// [배관] 공용 무력화 GE. 스킬 Payload에 넣으면 맞은 보스의 무력화 게이지가 깎인다. (2026-11-18 apply120)
//
//   규칙(이 클래스) : Instant / StaggerDamage(무력화 입구)에 Add / 값은 SetByCaller.Stagger
//   쓰는 곳         : 무력화가 있는 플레이어 스킬의 Payload (데미지 GE 옆에 하나 더)
//                     예) Payloads = [ GE_Damage_Basic { DamageRate }, LOAGE_Stagger { SetByCaller.Stagger = CT_SkillValues 행 } ]
//   깎는 일·0 판단은 보스 스탯창이 한다 (ULOAAttributeSet_Boss)
//
// [왜 데미지 계산 안에서 같이 깎지 않나] (대신 정함 2026-11-18)
//   데미지 계산(ULOADamageExecution)이 무력화 칸을 쓰면, 데미지를 받는 모든 대상(플레이어·허수아비)이 보스 스탯창을 가져야 한다
//   ("Execution이 캡처하는 스탯창은 맞는 쪽 모두에" 규칙). GE를 따로 두면 무력화가 없는 대상은 엔진이 건너뛴다:
//   GameplayEffect.cpp InternalExecuteMod → 스탯창이 없으면 "does not have attribute ... Skipping modifier" 로그만 (Log 단계)
//
// [왜 C++ GE인가] 아이덴티티 게이지 GE(ULOAGE_IdentityGain)와 같은 이유. 조정할 숫자가 없고(전부 SetByCaller) 규칙도 바뀌지 않는다.
//   BP 자식 없이 Payload의 Effect 칸에서 LOAGE_Stagger를 바로 고른다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_Stagger.generated.h"

UCLASS()
class LOA_API ULOAGE_Stagger : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_Stagger();
};
