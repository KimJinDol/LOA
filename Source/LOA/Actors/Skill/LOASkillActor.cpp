// LOASkillActor.cpp

#include "Actors/Skill/LOASkillActor.h"
#include "Library/LOACombatLibrary.h"
#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillActor)

ALOASkillActor::ALOASkillActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버가 만든 액터를 모든 클라이언트에 복제한다. (클라는 겉모습만)
	bReplicates = true;

	// 부모는 매 프레임 할 일이 없다. 필요한 자식만 켠다.
	PrimaryActorTick.bCanEverTick = false;
}

void ALOASkillActor::InitSkillActor(const FLOASkillModuleContext& InContext, const TArray<FLOAEffectPayload>& InPayloads)
{
	// 값 복사. 이후 GA가 끝나거나 다음 발동이 시작돼도 이 액터의 값은 바뀌지 않는다.
	SkillContext = InContext;
	SkillPayloads = InPayloads;

	// 범위 기준만 이 액터로. (이 액터가 Targeting을 쓰면 이 액터 주변을 찾는다)
	// Caster(시전자)는 그대로 → 편 판단은 계속 시전자 기준. (헤더 [복사본의 SourceActor와 Caster])
	SkillContext.SourceActor = this;
}

void ALOASkillActor::ApplySkillPayloads(const TArray<FLOATargetHit>& Targets) const
{
	// 판정과 데미지는 서버만. (클라의 이 액터는 Context가 비어 있기도 하다)
	if (!HasAuthority())
	{
		return;
	}

	// 시전자가 게임을 나가서 ASC가 사라졌으면 줄 수 없다. (GE를 보낼 쪽이 없음)
	// IsValid: null뿐 아니라 "파괴 예정" 상태도 거른다.
	if (!IsValid(SkillContext.SourceASC) || SkillPayloads.IsEmpty() || Targets.IsEmpty())
	{
		return;
	}

	ULOACombatLibrary::ApplyPayloads(SkillContext, Targets, SkillPayloads);
}
