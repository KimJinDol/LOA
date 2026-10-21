// LOAStateTreeCondition_CanUsePattern.cpp

#include "AI/StateTree/LOAStateTreeCondition_CanUsePattern.h"
#include "AbilitySystem/Abilities/Monster/LOAMonsterAbility.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystemGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeCondition_CanUsePattern)

bool FLOAStateTreeCondition_CanUsePattern::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);

	// 패턴이 없으면 CanActivatePattern이 경고를 남기고 false
	return ULOAMonsterAbility::CanActivatePattern(ASC, InstanceData.PatternTag, InstanceData.Target);
}
