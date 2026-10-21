// LOAStateTreeTask_WaitForEnemy.cpp

#include "AI/StateTree/LOAStateTreeTask_WaitForEnemy.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"

#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeTask_WaitForEnemy)

EStateTreeRunStatus FLOAStateTreeTask_WaitForEnemy::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 들어오자마자 한 번 확인 (이미 가까이 있으면 바로 전투)
	if (IsEnemyInRange(InstanceData))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.TimeUntilNextCheck = InstanceData.CheckInterval;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FLOAStateTreeTask_WaitForEnemy::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.TimeUntilNextCheck -= DeltaTime;
	if (InstanceData.TimeUntilNextCheck > 0.f)
	{
		return EStateTreeRunStatus::Running;
	}
	InstanceData.TimeUntilNextCheck = InstanceData.CheckInterval;

	if (IsEnemyInRange(InstanceData))
	{
		LOA_LOG(InstanceData.Actor, Log, TEXT("[AI] Enemy within %.0fcm. Engage."), InstanceData.Range);
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

bool FLOAStateTreeTask_WaitForEnemy::IsEnemyInRange(const FInstanceDataType& InstanceData)
{
	const AActor* Self = InstanceData.Actor;
	if (!Self)
	{
		return false;
	}

	TArray<AActor*> Enemies;
	ULOACombatLibrary::FindLivingEnemyPlayers(Self, Enemies);

	// 바닥 거리(높이 무시)로 비교. 루트 없이 제곱끼리 비교한다.
	const FVector SelfLocation = Self->GetActorLocation();
	const float RangeSquared = FMath::Square(InstanceData.Range);

	for (const AActor* Enemy : Enemies)
	{
		if (FVector::DistSquared2D(SelfLocation, Enemy->GetActorLocation()) <= RangeSquared)
		{
			return true;
		}
	}
	return false;
}
