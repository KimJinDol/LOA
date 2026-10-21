// LOASkillTargeting_Self.cpp

#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting_Self.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillTargeting_Self)

void ULOASkillTargeting_Self::FindTargets(const FLOASkillModuleContext& Context, TArray<FLOATargetHit>& OutHits) const
{
	// "나" = 시전자. (범위 기준 SourceActor가 아니다. 장판 액터가 Self를 써도 장판이 아니라 시전자에게 준다)
	AActor* Source = Context.Caster;
	if (!Source)
	{
		return;
	}

	// 이미 목록에 있으면 넣지 않는다. (다른 Targeting 결과와 같은 목록을 쓸 때 대비)
	const bool bAlreadyIn = OutHits.ContainsByPredicate(
		[Source](const FLOATargetHit& Existing) { return Existing.Target == Source; });
	if (bAlreadyIn)
	{
		return;
	}

	// 나 자신. 맞은 지점은 없다.
	FLOATargetHit& NewHit = OutHits.AddDefaulted_GetRef();
	NewHit.Target = Source;
	NewHit.bHasHitPoint = false;
}
