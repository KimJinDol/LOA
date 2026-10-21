// LOAGE_Cooldown.cpp

#include "AbilitySystem/Effects/LOAGE_Cooldown.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_Cooldown)

ULOAGE_Cooldown::ULOAGE_Cooldown()
{
	// 정해진 시간 동안 붙어 있다가 사라진다.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 시간 = SetByCaller.Cooldown (ULOAGameplayAbility::ApplyCooldown이 CooldownDuration을 넣는다)
	FSetByCallerFloat CooldownValue;
	CooldownValue.DataTag = LOAGameplayTags::SetByCaller_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownValue);

	// Modifier 없음: 값을 바꾸지 않는다.
	// 태그도 여기서 정하지 않는다: Ability마다 다르므로 ApplyCooldown이 Spec에 붙인다. (DynamicGrantedTags)
}
