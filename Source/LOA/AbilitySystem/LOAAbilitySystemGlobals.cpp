// LOAAbilitySystemGlobals.cpp

#include "AbilitySystem/LOAAbilitySystemGlobals.h"
#include "AbilitySystem/LOAGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAbilitySystemGlobals)

FGameplayEffectContext* ULOAAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FLOAGameplayEffectContext();
}
