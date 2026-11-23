// LOAGE_Stun.h
// [배관] 스턴 GE의 부모. Payload에 넣으면 맞은 쪽이 스턴된다. 시간 규칙만 가진다. (2026-11-10)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할] "효과는 GE" 규칙 (경직·띄움과 같음)
//   이 GE : 스턴 태그(State.CrowdControl.Status.Stunned)를 시간 동안 붙인다 + 면역 조건
//   GA    : ULOAGA_Stun이 태그가 있는 동안 켜져 있다 (끊기, 멈춤, 모션). 태그가 사라지면 꺼진다
//   경직과 달리 GE 시간 = 진짜 스턴 시간이다 (밀기도 "또 맞으면 처음부터"도 없어서 GA가 시간을 셀 이유가 없다)
//
// [시간] Payload의 SetByCaller.Duration, 없으면 이 GE의 Default Duration (ULOAStunDurationMagnitude가 고른다)
//   예) Roar 작은 장판: [ 데미지, GE_CC_Stun { SetByCaller.Duration 2 } ] → 2초
//   예) 카운터 성공: 카운터 모듈이 Stun Duration을 SetByCaller.Duration으로 넣는다
//
// ─────────────────────────────────────────────────────────────────────────────
// [면역] 코드가 아니라 이 GE 에셋의 조건 칸이 판단한다 (BP에서 설정, 저장할 때 아래를 검사)
//   Grant Tags                  : State.CrowdControl.Status.Stunned
//   Application Tag Requirements → Ignore Tags :
//       State.Immune.Status        (상태 이상 면역: 아이덴티티 켜는 동안 등 → 아무 반응 없음)
//       State.CrowdControl.Push    (띄움·누움 중 → 안 걸림. 피격 이상이 더 강함, 경직과 같은 규칙)
//   보스: 플레이어는 CC가 없으므로(사용자 전제) 보스에게 걸리는 건 카운터 스턴뿐 → 같은 GE를 쓴다
//
// [네트워크] 1 서버만: 적용(면역 검사) / 2 복제: 스턴 태그 / 3 각자: 없음
//
// [BP에서 설정할 것] GE_CC_Stun (부모: LOAGE_Stun)
//   Components: Grant Tags, Target Tag Requirements (위 [면역])
//   LOA|Stun: Default Duration
//   Duration Magnitude는 건드리지 않는다 (C++ 기본값 = ULOAStunDurationMagnitude)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_Stun.generated.h"

UCLASS()
class LOA_API ULOAGE_Stun : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_Stun();

#if WITH_EDITOR
	/** 저장할 때: 스턴 태그를 붙이는가, 면역·피격 이상 조건이 있는가, 시간 계산기가 그대로인가 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** 스턴 시간 (초). Payload에 SetByCaller.Duration이 없을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Stun", meta = (ClampMin = "0.1", Units = "Seconds"))
	float DefaultDuration = 2.f;
};
