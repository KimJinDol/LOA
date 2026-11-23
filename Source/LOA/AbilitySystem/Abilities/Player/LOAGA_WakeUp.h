// LOAGA_WakeUp.h
// [배관] 기상기. 누워 있을 때(띄움 후 착지) 스페이스 → 커서 방향으로 구르며 일어난다. 모든 직업 공용.
//
// ─────────────────────────────────────────────────────────────────────────────
// [회피와의 관계] (2026-10-19 사용자 결정: 키만 스페이스, 쿨타임은 따로, 무적 없음)
//   움직임(커서 쪽으로 돌기 → 정해진 거리를 밀기 → 몽타주)은 회피와 같다 → ULOAGA_Dodge를 그대로 물려받고 조건만 바꾼다.
//                   회피 (ULOAGA_Dodge)                    기상기 (이 클래스)
//   발동 조건       피격 이상 중이면 안 됨                  누워 있을 때만 (State.CrowdControl.Push.Down)
//   면역            피격 이상·상태 이상 면역                없음 (사용자: 무적 없음)
//   끊는 것         BP의 Cancel Abilities with Tag          띄움 GA (Ability.Common.Airborne) → 띄움·누움 GE가 지워짐
//   쿨타임          Cooldown.Common.Dodge                          따로 (예: Cooldown.Common.WakeUp)
//
// [키] GA는 자기 키를 모른다 (6장). 꾸러미가 InputTag.WakeUp으로 묶고, IMC에서 스페이스를 IA_Dodge와 IA_WakeUp 둘 다에 연결한다.
//   서 있을 때 스페이스 → 회피만 발동 (기상기는 Down이 없어서 거절)
//   누워 있을 때 스페이스 → 기상기만 발동 (회피는 피격 이상 중이라 거절)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] 예) 띄움 → 착지해서 미끄러지는 중, 낭떠러지 1m 앞
//   스페이스 → (B 컴퓨터에서 먼저, 서버가 확인)
//     GAS: Cancel Abilities with Tag → [서버] ULOAGA_Airborne 취소 → 띄움·누움·이동 금지 GE 삭제 (미끄러짐 제동도 풀림)
//     부모(회피) 흐름: 쿨타임 → 커서 쪽으로 회전 → 몽타주 + 밀기 300cm (끝나면 속도 0 → 미끄러짐이 멈춤)
//
// [네트워크] 회피와 같다 (LocalPredicted). 띄움 GA 취소는 서버에서만 일어난다 (띄움 GA가 서버에만 있으므로)
//   1. 서버만 : 띄움 GA 취소, GE 삭제
//   2. 복제   : 태그가 빠짐, 몽타주
//   3. 각자   : 구르기 이동 (B 컴퓨터 예측 + 서버 확인, 회피와 같은 루트 모션 Task)
//   한계: B 컴퓨터는 누움 태그를 서버에서 복제로 받는다 → 착지하고 핑의 절반 정도 동안은 스페이스가 거절될 수 있다 (핑 100ms → 0.05초)
//
// [BP에서 설정할 것] GA_WakeUp (부모: LOAGA_WakeUp)
//   Montage                   = 기상 구르기 몽타주 (끝 부분에 Event.Montage.Unlock, 회피와 같은 규칙)
//   Cooldown Duration / Tags  = 10 / Cooldown.Common.WakeUp
//   Asset Tags                = Ability.Common.WakeUp
//   Dodge Distance / Duration = 300 / 0.3 (기본값)
//   꾸러미: 플레이어 꾸러미에 InputTag.WakeUp으로. 아이콘(SkillDisplayInfo)은 비워 두면 스킬 슬롯에 안 나온다

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/LOAGA_Dodge.h"
#include "LOAGA_WakeUp.generated.h"

UCLASS(Abstract)
class LOA_API ULOAGA_WakeUp : public ULOAGA_Dodge
{
	GENERATED_BODY()

public:
	ULOAGA_WakeUp(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
