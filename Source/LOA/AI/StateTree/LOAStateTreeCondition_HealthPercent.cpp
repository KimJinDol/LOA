// LOAStateTreeCondition_HealthPercent.cpp

#include "AI/StateTree/LOAStateTreeCondition_HealthPercent.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeCondition_HealthPercent)

bool FLOAStateTreeCondition_HealthPercent::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	if (!ASC)
	{
		return false;
	}

	const float MaxHealth = ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		return false;
	}

	// 예) 39,000 / 100,000 × 100 = 39
	const float HealthPercent = ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute()) / MaxHealth * 100.f;

	switch (InstanceData.Check)
	{
	case ELOAHealthCheck::AtOrBelow:
		return HealthPercent <= InstanceData.Percent;
	case ELOAHealthCheck::Above:
		return HealthPercent > InstanceData.Percent;
	default:
		return false;
	}
}
