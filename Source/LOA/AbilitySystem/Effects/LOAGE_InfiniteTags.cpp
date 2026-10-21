// LOAGE_InfiniteTags.cpp

#include "AbilitySystem/Effects/LOAGE_InfiniteTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_InfiniteTags)

ULOAGE_InfiniteTags::ULOAGE_InfiniteTags()
{
	// 떼어 낼 때까지 붙어 있다. (시간으로 사라지지 않음)
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Modifier 없음: 값을 바꾸지 않는다.
	// 태그도 여기서 정하지 않는다: 쓰는 쪽마다 다르므로 Spec에 붙인다. (DynamicGrantedTags)
}
