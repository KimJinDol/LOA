// LOAGE_Stagger.cpp

#include "AbilitySystem/Effects/LOAGE_Stagger.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Boss.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_Stagger)

ULOAGE_Stagger::ULOAGE_Stagger()
{
	// 한 번 깎고 끝
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// "StaggerDamage에 SetByCaller.Stagger를 더한다". Payload에 값이 없으면 엔진이 0으로 보고 에러 로그를 남긴다 (스킬 데이터 실수를 알 수 있게)
	FSetByCallerFloat StaggerValue;
	StaggerValue.DataTag = LOAGameplayTags::SetByCaller_Stagger;

	FGameplayModifierInfo StaggerModifier;
	StaggerModifier.Attribute = ULOAAttributeSet_Boss::GetStaggerDamageAttribute();
	StaggerModifier.ModifierOp = EGameplayModOp::Additive;
	StaggerModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(StaggerValue);

	Modifiers.Add(StaggerModifier);
}
