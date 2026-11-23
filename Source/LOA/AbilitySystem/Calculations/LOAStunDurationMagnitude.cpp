// LOAStunDurationMagnitude.cpp

#include "AbilitySystem/Calculations/LOAStunDurationMagnitude.h"
#include "AbilitySystem/Effects/LOAGE_Stun.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStunDurationMagnitude)

float ULOAStunDurationMagnitude::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// 기본값: 이 GE 에셋의 Default Duration (스턴 GE가 아니면 2초)
	const ULOAGE_Stun* StunEffect = Cast<ULOAGE_Stun>(Spec.Def);
	const float DefaultDuration = StunEffect ? StunEffect->DefaultDuration : 2.f;

	// 없어도 경고하지 않는다 (없으면 기본값을 쓰는 게 정상)
	const float Duration = Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, false, DefaultDuration);

	// 0 이하면 순간 효과처럼 되어 태그가 안 남는다 → 기본값
	return Duration > 0.f ? Duration : DefaultDuration;
}
