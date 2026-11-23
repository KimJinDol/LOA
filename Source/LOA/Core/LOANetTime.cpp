// LOANetTime.cpp

#include "Core/LOANetTime.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/Pawn.h"

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

	// 클라: 엔진 추정값 + 한 방향 지연. 예) 핑 200ms → +0.1초
	return EngineServerTime + GetOneWayLatency(World);
}

double LOANetTime::GetOneWayLatency(const UWorld* World)
{
	if (!World || World->GetNetMode() != NM_Client)
	{
		return 0.0;	// 서버는 자기 시계가 곧 서버 시계
	}

	// 한 방향 지연 = 이 컴퓨터 플레이어 핑의 절반
	//   핑은 엔진이 PlayerState에 복제해 준다. (왕복, ms. 클라 쪽 값은 4ms 단위, 최대 약 1초)
	const APlayerController* LocalPC = World->GetFirstPlayerController();
	const APlayerState* LocalPS = LocalPC ? LocalPC->PlayerState : nullptr;
	const double OneWayLatency = LocalPS ? LocalPS->GetPingInMilliseconds() * 0.001 * 0.5 : 0.0;

	// 핑은 실제 시간, 서버 시각은 게임 시간(배속이 걸림) → 게임 시간으로 바꾼다
	//   예) 편도 0.1초, 배속 0.25 → 그 사이 게임 시간은 0.025초만 흐름 → 0.025 (배속 1이면 0.1)
	const AWorldSettings* WorldSettings = World->GetWorldSettings();
	const double TimeDilation = WorldSettings ? WorldSettings->GetEffectiveTimeDilation() : 1.0;

	return OneWayLatency * TimeDilation;
}

double LOANetTime::GetPlayerOneWayLatency(const APawn* Player)
{
	// 핑(왕복, ms)의 절반. 서버의 PlayerState 핑 = 서버가 잰 그 플레이어 왕복
	const APlayerState* PlayerState = Player ? Player->GetPlayerState() : nullptr;
	return PlayerState ? PlayerState->GetPingInMilliseconds() * 0.001 * 0.5 : 0.0;
}
