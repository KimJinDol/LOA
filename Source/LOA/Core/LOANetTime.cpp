// LOANetTime.cpp

#include "Core/LOANetTime.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

double LOANetTime::GetServerTimeNow(const UWorld* World)
{
	if (!World)
	{
		return 0.0;
	}

	const AGameStateBase* GameState = World->GetGameState();
	const double EngineServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();

	// 서버(데디, 리슨 호스트, 스탠드얼론)는 자기 시계가 곧 서버 시계
	if (World->GetNetMode() != NM_Client)
	{
		return EngineServerTime;
	}

	// 클라: 엔진 추정값 + 한 방향 지연 (= 이 컴퓨터 플레이어 핑의 절반)
	//   핑은 엔진이 PlayerState에 복제해 준다. (왕복, ms. 클라 쪽 값은 4ms 단위, 최대 약 1초)
	//   예) 핑 200ms → +0.1초. 서버 100.0초에 띄운 1.2초 예고가 서버 판정(101.2)과 같은 순간 꽉 찬다.
	const APlayerController* LocalPC = World->GetFirstPlayerController();
	const APlayerState* LocalPS = LocalPC ? LocalPC->PlayerState : nullptr;
	const double OneWayLatency = LocalPS ? LocalPS->GetPingInMilliseconds() * 0.001 * 0.5 : 0.0;

	return EngineServerTime + OneWayLatency;
}
