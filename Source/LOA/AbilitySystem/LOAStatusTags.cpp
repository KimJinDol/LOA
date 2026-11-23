// LOAStatusTags.cpp

#include "AbilitySystem/LOAStatusTags.h"
#include "Core/LOAGameplayTags.h"

const TArray<FGameplayTag>& LOAStatusTags::GetParentTags()
{
	static const TArray<FGameplayTag> ParentTags = {
		LOAGameplayTags::State_Buff,
		LOAGameplayTags::State_Debuff,
		LOAGameplayTags::State_CrowdControl_Status,
	};
	return ParentTags;
}

bool LOAStatusTags::IsStatusTag(const FGameplayTag& Tag)
{
	for (const FGameplayTag& ParentTag : GetParentTags())
	{
		if (Tag != ParentTag && Tag.MatchesTag(ParentTag))
		{
			return true;
		}
	}
	return false;
}
