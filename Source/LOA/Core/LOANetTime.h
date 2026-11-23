// LOANetTime.h
// 모든 컴퓨터가 같이 쓰는 "지금 서버 시각". 예고(차오름)와 피해자 판정(판정 순간)이 같은 시계를 보도록 한 곳에 둔다.
//
// [왜 필요한가]
// 엔진의 GetServerWorldTimeSeconds()는 클라에서 "서버가 보낸 시각을 받은 그대로"라, 오는 데 걸린 시간(한 방향 지연)만큼 늦다.
//   예) 핑 200ms → 클라가 보는 서버 시각이 0.1초 늦음
//       → 예고가 0.1초 늦게 차고, 피해자 판정도 0.1초 늦은 위치로 하게 된다.
// 그래서 클라에서는 핑의 절반(한 방향 지연 추정값)을 더한다. 핑은 왕복 시간이라 절반이 편도.
// 핑은 실제 시간이라 지금 배속을 곱해 게임 시간으로 바꾼다. (배속 1이면 그대로)
//
// [알려진 한계 — 배속] (2026-10-22 측정, 사용자 결정: 그냥 둔다)
// 배속(LOASlomo, 개발용 치트)을 1이 아닌 값으로 바꾸면, 엔진이 알려 주는 서버 시각(GetServerWorldTimeSeconds) 자체가
// 클라에서 서버보다 앞선다. → 클라 예고가 먼저 찬다. 배속 1에서는 정상 (오차 약 0.02초).
//   측정: 배속 0.25에서 클라가 받은 순간 엔진 시각이 서버가 보낸 시각보다 +0.044 ~ +0.164초 (매번 다름)
//         x1로 돌린 직후·몇 초 뒤 모두 −0.024 ~ −0.028 (정상)
//   원인은 확정 못 함. 관련 엔진 이슈: 전체 배속이 복제 주기에도 걸림 (UE-238682, Won't Do)
//   실제 게임에서는 배속이 바뀌지 않으므로 고치지 않는다. 확인하려면 PIE에서 클라의 엔진 시각과 서버 월드 시각을 1초마다 비교.
//
// [사용 예]
//   const double Now = LOANetTime::GetServerTimeNow(GetWorld());
//
// [왜 클래스가 아니라 함수인가]
// 상태가 없는 계산 하나다. (LOALog처럼 namespace 함수로 둔다)

#pragma once

#include "CoreMinimal.h"

class UWorld;
class APawn;

namespace LOANetTime
{
	/**
	 * 지금 서버 시각 (초).
	 *   서버(데디, 리슨 호스트, 스탠드얼론) : 자기 시계 그대로
	 *   클라                               : 엔진 추정값 + 핑 ÷ 2 × 배속
	 * World가 없으면 0.
	 */
	LOA_API double GetServerTimeNow(const UWorld* World);

	/**
	 * 이 컴퓨터 플레이어의 편도 지연 (초, 게임 시간). = 핑 ÷ 2 × 배속. 서버(데디, 리슨 호스트, 스탠드얼론)는 0. (2026-10-28)
	 * 쓰는 곳: 예고 앞당김. 내 이동은 이만큼 늦게 서버에 닿으니, 예고도 이만큼 일찍 꽉 차야 화면 = 서버 판정
	 */
	LOA_API double GetOneWayLatency(const UWorld* World);

	/**
	 * [서버] 그 플레이어의 편도 지연 (초) = 서버가 아는 그 플레이어 핑 ÷ 2. 플레이어가 아니면 0. (2026-11-04)
	 * 쓰는 곳: 판정 방식이 "그 사람 화면에서 공격이 닿은 시각"을 정할 때 (ULOAServerJudgeSubsystem, ULOAVictimJudgeSubsystem)
	 *   전에는 판정 담당(ULOAHitJudgeSubsystem) 안에만 있었는데, 판정 방식이 둘로 나뉘면서 같이 쓰게 여기로 옮김
	 *   배속은 곱하지 않는다 (옮기기 전과 같은 값)
	 */
	LOA_API double GetPlayerOneWayLatency(const APawn* Player);
}
