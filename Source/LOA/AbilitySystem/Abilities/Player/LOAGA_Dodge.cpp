// LOAGA_Dodge.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_Dodge.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Actor.h"
#include "GameFramework/RootMotionSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Dodge)

ULOAGA_Dodge::ULOAGA_Dodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 스킬 시전 중에도 발동되도록, 부모가 넣어 둔 "행동 금지 중 발동 불가" 조건을 뺀다.
	ActivationBlockedTags.RemoveTag(LOAGameplayTags::State_Block_Ability);

	// 회피하는 동안 피격 이상·상태 이상 면역. (데미지는 받는다)
	// Activation Owned Tags: Ability가 실행 중인 동안만 ASC에 붙고, 끝나면(몽타주 끝 / 후딜에서 끊김 / 취소) GAS가 뗀다.
	// → 로아처럼 "끊지 않으면 회피 모션 내내 면역, 끊으면 그 순간 해제"가 된다. BP에서 더하거나 뺄 수 있다.
	ActivationOwnedTags.AddTag(LOAGameplayTags::State_Immune_Push);
	ActivationOwnedTags.AddTag(LOAGameplayTags::State_Immune_Debuff);

	// bBlockMove / bBlockAbility는 부모 기본값(true) 그대로:
	// 회피 중에는 우클릭 이동도, 다른 스킬도 받지 않는다. 몽타주의 Unlock 노티파이(대시 끝)부터 풀린다.
}

// =============================================================================
// 회피
// =============================================================================

void ULOAGA_Dodge::StartSkill()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// ① 몽타주 재생 + 끝나면 종료. (다른 스킬과 같은 규칙: Unlock 노티파이부터 후딜, 이동/스킬로 캔슬)
	//    몽타주가 없으면 이동이 끝날 때 종료한다. (OnDodgeMoveFinished)
	PlayMontageAndEnd();

	// ② 이동: 지금 보는 방향(방금 커서 쪽으로 돌았음)으로 DodgeDuration 동안 밀기
	FVector Direction = Avatar->GetActorForwardVector();
	Direction.Z = 0.f;
	Direction.Normalize();

	const float Strength = DodgeDistance / DodgeDuration;   // 초당 이동 거리 (cm/s)

	UAbilityTask_ApplyRootMotionConstantForce* MoveTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		TEXT("DodgeMove"),
		Direction,
		Strength,
		DodgeDuration,
		/*bIsAdditive*/ false,                          // 지금 속도를 무시하고 이 힘만으로 이동
		/*StrengthOverTime*/ nullptr,                   // 처음부터 끝까지 같은 속도
		ERootMotionFinishVelocityMode::SetVelocity,     // 끝나면 아래 속도로 설정
		/*SetVelocityOnFinish*/ FVector::ZeroVector,    // → 멈춤 (미끄러지지 않음)
		/*ClampVelocityOnFinish*/ 0.f,
		/*bEnableGravity*/ true);                        // 바닥에 붙어 있도록

	MoveTask->OnFinish.AddDynamic(this, &ThisClass::OnDodgeMoveFinished);
	MoveTask->ReadyForActivation();
}

void ULOAGA_Dodge::OnDodgeMoveFinished()
{
	// 몽타주가 없으면 끝낼 신호가 없으므로 여기서 정상 종료.
	// 몽타주가 있으면 아무것도 안 한다: 착지 동작은 몽타주가 계속 재생하고, 몽타주가 끝나거나 후딜에서 끊길 때 종료된다.
	if (!Montage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}