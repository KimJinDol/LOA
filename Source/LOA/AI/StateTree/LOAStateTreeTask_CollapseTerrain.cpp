// LOAStateTreeTask_CollapseTerrain.cpp

#include "AI/StateTree/LOAStateTreeTask_CollapseTerrain.h"
#include "Game/LOARaidSubsystem.h"
#include "Core/LOALog.h"

#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeTask_CollapseTerrain)

EStateTreeRunStatus FLOAStateTreeTask_CollapseTerrain::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(InstanceData.Actor);
	if (!Raid)
	{
		LOA_LOG(InstanceData.Actor, Warning, TEXT("[AI] Collapse Terrain: no raid subsystem."));
		return EStateTreeRunStatus::Failed;
	}

	// 바로 끝난다. 예고·무너짐은 구역이 알아서 (이 Task는 기다리지 않음)
	return Raid->RequestTerrainCollapse() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
