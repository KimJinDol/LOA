// LOAStateTreeTask_RunPattern.cpp

#include "AI/StateTree/LOAStateTreeTask_RunPattern.h"
#include "AbilitySystem/Abilities/Monster/LOAMonsterAbility.h"
#include "Core/LOALog.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStateTreeTask_RunPattern)

namespace LOARunPattern
{
	static UAbilitySystemComponent* GetASC(const AActor* Actor)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	}
}

EStateTreeRunStatus FLOAStateTreeTask_RunPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ActivatedHandle = FGameplayAbilitySpecHandle();
	InstanceData.ActivatedTag = FGameplayTag();

	UAbilitySystemComponent* ASC = LOARunPattern::GetASC(InstanceData.Actor);
	if (!ASC || InstanceData.PatternTags.IsEmpty())
	{
		LOA_LOG(InstanceData.Actor, Warning, TEXT("[AI] Run Pattern: no ASC or Pattern Tags is empty."));
		return EStateTreeRunStatus::Failed;
	}

	// ① 후보 중 지금 쓸 수 있는 것만 모은다. 예) { Slam(쿨타임), TwinSlash, Downward } → { TwinSlash, Downward }
	TArray<FGameplayTag> Usable;
	for (const FGameplayTag& Tag : InstanceData.PatternTags)
	{
		if (ULOAMonsterAbility::CanActivatePattern(ASC, Tag, InstanceData.Target))
		{
			Usable.Add(Tag);
		}
	}

	// 다 쿨타임 중이면 실패 → 트리가 숨 고르기 후 다시 고른다
	if (Usable.IsEmpty())
	{
		LOA_LOG(InstanceData.Actor, Log, TEXT("[AI] Run Pattern: no usable pattern in %s (cooldown / busy / no target)."),
			*InstanceData.PatternTags.ToStringSimple());
		return EStateTreeRunStatus::Failed;
	}

	// ② 무작위 하나. 예) 2개 중 0~1번
	const FGameplayTag Picked = Usable[FMath::RandRange(0, Usable.Num() - 1)];
	LOA_LOG(InstanceData.Actor, Log, TEXT("[AI] Run Pattern: picked %s (%d of %d usable)."),
		*Picked.ToString(), Usable.Num(), InstanceData.PatternTags.Num());

	// ③ 발동. 방금 확인했으니 보통 성공한다. 거부되면 실패 → 숨 고르기
	if (!ULOAMonsterAbility::TryActivatePattern(ASC, Picked, InstanceData.Target, &InstanceData.ActivatedHandle))
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.ActivatedTag = Picked;

	// 몽타주가 없는 패턴은 발동 즉시 끝났을 수 있다
	return IsPatternActive(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FLOAStateTreeTask_RunPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 패턴이 끝났으면 성공 (취소돼서 끝난 경우도 여기서 끝난 것으로 본다. 다음 행동은 트리가 정한다)
	return IsPatternActive(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FLOAStateTreeTask_RunPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 아직 도는 패턴을 두고 상태를 떠나면 취소한다. (정상적으로 끝난 뒤라면 할 일 없음)
	if (IsPatternActive(InstanceData))
	{
		if (UAbilitySystemComponent* ASC = LOARunPattern::GetASC(InstanceData.Actor))
		{
			LOA_LOG(InstanceData.Actor, Log, TEXT("[AI] Left pattern state while %s is running. Cancel it."),
				*InstanceData.ActivatedTag.ToString());
			ASC->CancelAbilityHandle(InstanceData.ActivatedHandle);
		}
	}

	InstanceData.ActivatedHandle = FGameplayAbilitySpecHandle();
	InstanceData.ActivatedTag = FGameplayTag();
}

bool FLOAStateTreeTask_RunPattern::IsPatternActive(const FInstanceDataType& InstanceData)
{
	if (!InstanceData.ActivatedHandle.IsValid())
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = LOARunPattern::GetASC(InstanceData.Actor);
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(InstanceData.ActivatedHandle) : nullptr;
	return Spec && Spec->IsActive();
}
