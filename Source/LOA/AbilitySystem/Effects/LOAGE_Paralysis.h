// LOAGE_Paralysis.h
// [배관] 경직 GE의 부모. 보스 공격의 Payload에 넣으면 맞은 플레이어가 경직된다. 숫자·방향 규칙만 가진다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [경직이란] (사용자 결정, 로아 규칙)
//   살짝 밀리면서, 하던 스킬이 끊기고, 경직 시간 동안 아무것도 못 한다 (스킬·회피·이동). 아주 짧은 스턴 느낌.
//   경직 면역(슈퍼아머) 스킬 중이면 아무 반응 없음 (데미지만). 피격 이상(띄움)이 더 강하다.
//
// [역할] "효과는 GE" 규칙: 경직은 맞은 쪽이 판단하지 않는다. 걸리면 경직 (면역만 예외).
//   이 GE : 경직 태그를 붙인다 + 숫자(경직 시간·밀리는 거리)와 방향 규칙을 가진다
//   GA    : ULOAGA_Paralysis가 태그를 보고 발동 → 밀고, 경직 시간이 끝나면 이 GE를 지운다 (서버)
//   GE의 시간(Duration Magnitude, 기본 5초)은 안전용 최대 시간뿐이다. 실제 경직 시간은 GA가 센다 (띄움과 같은 방식)
//
// 예) 보스 휘두르기 Payloads = [ GE_Damage, GE_CC_Paralysis { Duration 0.5, LaunchDistance 100 } ]
//     → 맞은 B에게 State.CrowdControl.Paralysis → 때린 곳에서 B 쪽으로 0.15초 동안 정확히 100cm 밀림 → 0.5초 뒤 풀림
//   밀림은 물리가 아니라 정해진 궤적 (2026-10-21): Push Duration 동안 일정한 속도로, 끝나면 바로 멈춤 (미끄러짐 없음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [숫자] 공격마다 다르면 Payload 값(SetByCaller), 자주 안 바뀌면 이 GE의 기본값
//   SetByCaller.Duration        : 경직 시간 (없으면 Default Duration). 맞은 순간부터 센다
//   SetByCaller.LaunchDistance  : 밀리는 거리 cm (없으면 Default Distance)
//   밀리는 데 걸리는 시간은 고정: Push Duration (이 GE 칸, 사용자 결정)
// [방향] Launch Direction, Yaw Offset → 부모 ULOAGE_CrowdControl (띄움과 같은 규칙)
//
// ─────────────────────────────────────────────────────────────────────────────
// [면역] 코드가 아니라 이 GE 에셋의 조건 칸이 판단한다 (BP에서 설정, 저장할 때 아래를 검사)
//   Grant Tags                  : State.CrowdControl.Paralysis
//   Application Tag Requirements → Ignore Tags :
//       State.Immune.Paralysis     (슈퍼아머 스킬 → 아무 반응 없음)
//       State.Immune.Push          (회피 등 피격 이상 면역은 경직까지 막는다)
//       State.CrowdControl.Push    (띄움·누움 중 → 경직이 안 붙음. 피격 이상이 더 강함)
//   데미지는 다른 GE라 면역과 상관없이 들어간다.
//
// [네트워크] 1 서버만: 적용(면역 검사) / 2 복제: 경직 태그 / 3 각자: 없음
//
// [BP에서 설정할 것] GE_CC_Paralysis (부모: LOAGE_Paralysis)
//   Components: Grant Tags, Target Tag Requirements (위 [면역])
//   LOA|CrowdControl: Launch Direction, (Source Forward면) Yaw Offset, Default Distance / LOA|Paralysis: Default Duration, Push Duration
//   Stacking은 건드리지 않는다 (경직 중 또 맞으면 GA가 옛 GE를 지우고 새 GE로 처음부터)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Effects/LOAGE_CrowdControl.h"
#include "LOAGE_Paralysis.generated.h"

UCLASS()
class LOA_API ULOAGE_Paralysis : public ULOAGE_CrowdControl
{
	GENERATED_BODY()

public:
	ULOAGE_Paralysis();

#if WITH_EDITOR
	/** 저장할 때: 경직 태그를 붙이는가, 면역·피격 이상 조건이 있는가 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** 경직 시간 (초). Payload에 SetByCaller.Duration이 없을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Paralysis", meta = (ClampMin = "0", Units = "Seconds"))
	float DefaultDuration = 0.5f;

	/** 밀리는 데 걸리는 시간 (초, 고정). 이 시간 동안 일정한 속도로 밀리고 바로 멈춘다. 예) 100cm / 0.15초 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Paralysis", meta = (ClampMin = "0.01", Units = "Seconds"))
	float PushDuration = 0.15f;
};
