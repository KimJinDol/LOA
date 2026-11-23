// LOAStateTreeTask_BattleClock.cpp

#include "AI/StateTree/LOAStateTreeTask_BattleClock.h"
#include "Game/LOARaidSubsystem.h"
#include "Game/LOAGameState.h"
#include "Core/LOALog.h"

#include "StateTreeExecutionContext.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeTask_BattleClock)

EStateTreeRunStatus FLOAStateTreeTask_BattleClock::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Elapsed = 0.f;
	InstanceData.TimeAwayFromCenter = 0.f;
	InstanceData.bWarnedNoCenter = false;

	LOA_LOG(InstanceData.Actor, Log, TEXT("[AI] Battle clock started."));

	// 전투 시작을 레이드 상태에 알린다 → 전투 중 + 공용 부활 목숨 시계 시작 (전투 전일 때만 받음)
	const UWorld* World = Context.GetWorld();
	if (ALOAGameState* GameState = World ? World->GetGameState<ALOAGameState>() : nullptr)
	{
		GameState->StartBattle();
	}
	else
	{
		LOA_LOG(InstanceData.Actor, Warning, TEXT("[AI] GameState is not ALOAGameState. Revive lives will not fill. (Check Game State Class in BP_LOAGameMode)"));
	}

	// 끝내지 않는다. 전투 상태가 끝날 때까지 잰다.
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FLOAStateTreeTask_BattleClock::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// ① 전투 경과
	InstanceData.Elapsed += DeltaTime;

	// ② 중앙 밖 시간
	if (!InstanceData.Actor)
	{
		return EStateTreeRunStatus::Running;
	}

	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(InstanceData.Actor);
	FVector Center;
	if (!Raid || !Raid->FindLocation(InstanceData.CenterLocationTag, Center))
	{
		// 중앙을 모르면 "멀다"고 판단할 수 없다 → 0 그대로 (중앙 복귀가 안 나옴). 경고는 한 번만
		if (!InstanceData.bWarnedNoCenter)
		{
			LOA_LOG(InstanceData.Actor, Warning, TEXT("[AI] Battle Clock: no location marker with tag '%s'. Time away from center stays 0."),
				*InstanceData.CenterLocationTag.ToString());
			InstanceData.bWarnedNoCenter = true;
		}
		InstanceData.TimeAwayFromCenter = 0.f;
		return EStateTreeRunStatus::Running;
	}

	// 바닥 거리 (높이 무시 — 점프 중에 멀어진 것으로 보지 않게). 제곱으로 비교
	const float DistSq = FVector::DistSquared2D(InstanceData.Actor->GetActorLocation(), Center);
	if (DistSq > FMath::Square(InstanceData.CenterRadius))
	{
		InstanceData.TimeAwayFromCenter += DeltaTime;
	}
	else
	{
		InstanceData.TimeAwayFromCenter = 0.f;
	}

	return EStateTreeRunStatus::Running;
}
