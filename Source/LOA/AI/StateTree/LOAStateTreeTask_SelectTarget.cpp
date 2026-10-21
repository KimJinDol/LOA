// LOAStateTreeTask_SelectTarget.cpp

#include "AI/StateTree/LOAStateTreeTask_SelectTarget.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"

#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeTask_SelectTarget)

EStateTreeRunStatus FLOAStateTreeTask_SelectTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 살아 있는 적이 없으면 실패 → (Transition) 대기
	// 끝내지 않는다(Running). 교전 상태가 끝날 때까지 대상을 들고 있어야 자식 상태가 쓴다.
	return PickRandomTarget(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FLOAStateTreeTask_SelectTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (ULOACombatLibrary::IsTargetable(InstanceData.Target))
	{
		return EStateTreeRunStatus::Running;
	}

	// 대상이 죽었거나(State.Dead) 사라졌으면 살아 있는 다른 적으로 바꾼다. 아무도 없으면(전멸) 실패 → 대기
	LOA_LOG(InstanceData.Actor, Log, TEXT("[AI] Target lost. Reselect."));
	return PickRandomTarget(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FLOAStateTreeTask_SelectTarget::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// 교전 상태를 벗어나면 대상을 놓는다. (다음에 들어올 때 새로 고른다)
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Target = nullptr;
}

bool FLOAStateTreeTask_SelectTarget::PickRandomTarget(FInstanceDataType& InstanceData)
{
	InstanceData.Target = nullptr;

	TArray<AActor*> Enemies;
	ULOACombatLibrary::FindLivingEnemyPlayers(InstanceData.Actor, Enemies);
	if (Enemies.IsEmpty())
	{
		return false;
	}

	// 무작위 1명. 예) 살아 있는 3명 중 0~2번 하나
	InstanceData.Target = Enemies[FMath::RandRange(0, Enemies.Num() - 1)];

	LOA_LOG(InstanceData.Actor, Log, TEXT("[AI] Target selected: %s (%d candidate(s))"),
		*GetNameSafe(InstanceData.Target), Enemies.Num());
	return true;
}
