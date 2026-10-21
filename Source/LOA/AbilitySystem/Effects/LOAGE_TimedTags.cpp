// LOAGE_TimedTags.cpp

#include "AbilitySystem/Effects/LOAGE_TimedTags.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_TimedTags)

ULOAGE_TimedTags::ULOAGE_TimedTags()
{
	// 정해진 시간 동안 붙어 있다가 사라진다.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 시간 = SetByCaller.Duration (쓰는 쪽이 넣는다)
	FSetByCallerFloat DurationValue;
	DurationValue.DataTag = LOAGameplayTags::SetByCaller_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationValue);

	// Modifier 없음: 값을 바꾸지 않는다.
	// 태그도 여기서 정하지 않는다: 쓰는 쪽마다 다르므로 Spec에 붙인다. (DynamicGrantedTags)
}
