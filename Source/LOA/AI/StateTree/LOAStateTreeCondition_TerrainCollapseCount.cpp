// LOAStateTreeCondition_TerrainCollapseCount.cpp

#include "AI/StateTree/LOAStateTreeCondition_TerrainCollapseCount.h"
#include "Game/LOARaidSubsystem.h"

#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeCondition_TerrainCollapseCount)

bool FLOAStateTreeCondition_TerrainCollapseCount::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 상황판이 없으면 (게임 월드가 아님) 0개로 본다
	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(InstanceData.Actor);
	const int32 Collapsed = Raid ? Raid->GetCollapsedTerrainCount() : 0;

	switch (InstanceData.Check)
	{
	case ELOACountCheck::Below:
		return Collapsed < InstanceData.Count;
	case ELOACountCheck::AtLeast:
		return Collapsed >= InstanceData.Count;
	default:
		return false;
	}
}
