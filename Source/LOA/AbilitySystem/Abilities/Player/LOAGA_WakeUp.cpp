// LOAGA_WakeUp.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_WakeUp.h"
#include "Core/LOAGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_WakeUp)

ULOAGA_WakeUp::ULOAGA_WakeUp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 누워 있을 때만
	ActivationRequiredTags.AddTag(LOAGameplayTags::State_CrowdControl_Push_Down);

	// 플레이어 Ability는 피격 이상 중 발동 불가 (ULOAPlayerAbility 생성자) → 기상기만 뺀다
	ActivationBlockedTags.RemoveTag(LOAGameplayTags::State_CrowdControl_Push);

	// 무적 없음 (사용자 결정). 회피가 넣어 둔 면역을 뺀다
	ActivationOwnedTags.RemoveTag(LOAGameplayTags::State_Immune_Push);
	ActivationOwnedTags.RemoveTag(LOAGameplayTags::State_Immune_Debuff);

	// 발동하는 순간 띄움 GA를 끊는다 → 띄움 GA의 EndAbility가 띄움·누움·이동 금지 GE를 지운다 (서버)
	CancelAbilitiesWithTag.AddTag(LOAGameplayTags::Ability_CrowdControl_Airborne);

	// 회피보다 짧게 구른다 (BP에서 조정)
	DodgeDistance = 300.f;
}
