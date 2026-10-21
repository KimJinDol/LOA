// LOANetTime.h
// 모든 컴퓨터가 같이 쓰는 "지금 서버 시각". 예고(차오름)와 피해자 판정(판정 순간)이 같은 시계를 보도록 한 곳에 둔다.
//
// [왜 필요한가]
// 엔진의 GetServerWorldTimeSeconds()는 클라에서 "서버가 보낸 시각을 받은 그대로"라, 오는 데 걸린 시간(한 방향 지연)만큼 늦다.
//   예) 핑 200ms → 클라가 보는 서버 시각이 0.1초 늦음
//       → 예고가 0.1초 늦게 차고, 피해자 판정도 0.1초 늦은 위치로 하게 된다.
// 그래서 클라에서는 핑의 절반(한 방향 지연 추정값)을 더한다. 핑은 왕복 시간이라 절반이 편도.
//
// [사용 예]
//   const double Now = LOANetTime::GetServerTimeNow(GetWorld());
//
// [왜 클래스가 아니라 함수인가]
// 상태가 없는 계산 하나다. (LOALog처럼 namespace 함수로 둔다)

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace LOANetTime
{
	/**
	 * 지금 서버 시각 (초).
	 *   서버(데디, 리슨 호스트, 스탠드얼론) : 자기 시계 그대로
	 *   클라                               : 엔진 추정값 + 핑 ÷ 2
	 * World가 없으면 0.
	 */
	LOA_API double GetServerTimeNow(const UWorld* World);
}
