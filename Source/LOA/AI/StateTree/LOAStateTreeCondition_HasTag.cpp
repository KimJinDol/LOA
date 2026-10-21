// LOAStateTreeCondition_HasTag.cpp

#include "AI/StateTree/LOAStateTreeCondition_HasTag.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeCondition_HasTag)

bool FLOAStateTreeCondition_HasTag::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 보스에 태그가 있나 (ASC가 없거나 태그 칸이 비면 "없음")
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	const bool bTagFound = ASC && InstanceData.Tag.IsValid() && ASC->HasMatchingGameplayTag(InstanceData.Tag);

	switch (InstanceData.Check)
	{
	case ELOATagCheck::HasTag:
		return bTagFound;	// 있으면 참
	case ELOATagCheck::NoTag:
		return !bTagFound;	// 없으면 참
	default:
		return false;
	}
}
