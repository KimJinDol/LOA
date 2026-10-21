// LOAGE_Airborne.h
// [배관] 띄움 GE의 부모. 보스 공격의 Payload에 넣으면 맞은 플레이어가 뜬다. 숫자·방향 규칙만 가진다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할] "효과는 GE" 규칙 (ARCHITECTURE.md 교훈): 띄움은 맞은 쪽이 판단하지 않는다. 걸리면 뜬다 (면역만 예외).
//   이 GE  : 띄움 태그를 붙인다 + 날아갈 숫자(높이·거리·누워 있을 시간)와 방향 규칙을 가진다
//   GA     : ULOAGA_Airborne이 태그를 보고 발동 → 이 GE의 숫자로 실제로 날리고, 다 일어나면 이 GE를 지운다 (서버)
//   GE의 시간(Duration Magnitude, 기본 10초)은 안전용 최대 시간뿐이다. GA가 끝날 신호를 놓쳐도 태그가 영원히 남지 않게.
//
// 예) 보스 내려찍기 Payloads = [ GE_Damage, GE_CC_Airborne { DownDuration 1.0, LaunchDistance 600 } ]
//     → 맞은 B에게 State.CrowdControl.Push.Airborne → 장판 중심에서 B 쪽으로 600cm, 꼭대기 250cm, 1.0초 → 착지 후 1초 누움 → 일어남
//
// ─────────────────────────────────────────────────────────────────────────────
// [궤적] (2026-10-21 바꿈: 물리 → 정해진 궤적. 사용자 결정) ULOACharacterMovementComponent::StartCrowdControlMove
//   공중 시간  : Air Time (이 GE 칸, 고정). 처음 뜰 때도, 공중에서 또 맞을 때도 언제나 같은 시간
//   꼭대기     : 처음 뜬 땅 + Launch Height (이 GE 칸, 고정). 공중에서 또 맞아도 이보다 높이 안 올라간다 (연속 피격에 하늘로 솟지 않게)
//   거리       : 공격마다 다름 (SetByCaller.LaunchDistance) × (1 + High Hit Distance Bonus × 맞은 높이 ÷ Launch Height)
//                예) 거리 300, 보너스 1 → 땅에서 맞음 300 / 꼭대기에서 또 맞음 600 (높이 맞을수록 멀리 튕겨 나감)
//   착지하면 그 자리에서 멈춘다 (미끄러짐 없음)
//
// [숫자] 공격마다 다르면 Payload 값(SetByCaller), 아니면 이 GE의 칸
//   SetByCaller.DownDuration    : 착지한 뒤 누워 있는 시간 (없으면 Default Down Duration). 착지 순간부터 센다
//   SetByCaller.LaunchDistance  : 땅에서 맞았을 때 가는 거리 cm (없으면 Default Distance)
//   (SetByCaller.LaunchHeight는 더 안 쓴다: 높이는 이 GE의 Launch Height로 고정)
//
// [방향] Launch Direction, Yaw Offset, Default Distance는 부모 ULOAGE_CrowdControl의 칸 (경직과 공용, 2026-10-21)
//   Away From Source : 때린 것의 판정 기준점 → 맞은 사람 (장판 중심, 부채꼴 꼭짓점, 투사체 위치). 기본
//   Source Forward   : 때린 것(장판·투사체, 없으면 보스)의 앞 방향 + Yaw Offset. 장풍처럼 한 방향으로만 날리는 패턴
//   → 둘 다 GE를 따로 만든다: GE_CC_Airborne (기본) / GE_CC_Airborne_Forward
//
// ─────────────────────────────────────────────────────────────────────────────
// [면역] 코드가 아니라 이 GE 에셋의 조건 칸이 판단한다 (BP에서 설정, 저장할 때 아래를 검사)
//   Grant Tags                  : State.CrowdControl.Push.Airborne
//   Application Tag Requirements → Ignore Tags :
//       State.Immune.Push             (회피 등 피격 이상 면역 → 안 뜸)
//       State.CrowdControl.Push.Down  (누워 있음 → 다시 안 뜸. 공중이면 다시 뜬다)
//   데미지는 다른 GE라 면역과 상관없이 들어간다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크] 1 서버만: 적용(면역 검사) / 2 복제: 띄움 태그 (Mixed 모드 → 본인에게 GE, 모두에게 태그) / 3 각자: 없음
//
// [BP에서 설정할 것] GE_CC_Airborne (부모: LOAGE_Airborne)
//   Components: Grant Tags, Target Tag Requirements (위 [면역])
//   LOA|CrowdControl: Launch Direction, (Source Forward면) Yaw Offset, Default Distance / LOA|Airborne: Launch Height, Air Time, High Hit Distance Bonus, Default Down Duration
//   Stacking은 건드리지 않는다 (공중에서 또 맞으면 따로따로 붙는다. 다 일어나면 GA가 모두 지운다)
//   Duration Magnitude = 안전용 최대 시간 (기본 10초). 누워 있는 시간은 Payload의 SetByCaller.DownDuration

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Effects/LOAGE_CrowdControl.h"
#include "LOAGE_Airborne.generated.h"

UCLASS()
class LOA_API ULOAGE_Airborne : public ULOAGE_CrowdControl
{
	GENERATED_BODY()

public:
	ULOAGE_Airborne();

#if WITH_EDITOR
	/** 저장할 때: 띄움 태그를 붙이는가, 면역·누움 조건이 있는가 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** 꼭대기 높이 (처음 뜬 땅 기준, cm). 고정 (옛 이름 Default Height → DefaultEngine.ini 리다이렉트로 값 유지) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne", meta = (ClampMin = "10", Units = "Centimeters"))
	float LaunchHeight = 250.f;

	/** 공중 시간 (초). 처음 뜰 때도, 공중에서 또 맞을 때도 이 시간 동안 날아가 착지한다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne", meta = (ClampMin = "0.1", Units = "Seconds"))
	float AirTime = 1.f;

	/** 높이 맞을수록 멀리: 거리 × (1 + 이 값 × 맞은 높이 ÷ Launch Height). 0이면 끔, 1이면 꼭대기에서 두 배 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne", meta = (ClampMin = "0"))
	float HighHitDistanceBonus = 1.f;

	/** 착지한 뒤 누워 있는 시간 (초). Payload에 SetByCaller.DownDuration이 없을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne", meta = (ClampMin = "0", Units = "Seconds"))
	float DefaultDownDuration = 1.f;
};
