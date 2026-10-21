// LOASkillTargeting_Self.h
// [배관] Self Targeting. "나 자신"(기준 액터) 하나를 돌려준다. 범위와 상관없다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [디테일 패널]
//
//   ▼ Targeting  [ Self ▼ ]     ← 설정 칸 없음
//
// [쓰는 곳]
//   자기 버프      : Apply (Trigger = On Skill Start) + Self + 버프 GE
//   광시곡의 나 몫 : Apply [0] Shape / Ally → 뎀감 75%
//                    Apply [1] Self         → 뎀감 50%   (범위 밖이어도 받음, 수치가 달라서 모듈을 나눈다)
//
// [맞은 지점]
//   없음 (bHasHitPoint = false). 나에게 주는 효과라 숫자 위치가 필요 없다.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "LOASkillTargeting_Self.generated.h"

UCLASS(meta = (DisplayName = "Self"))
class LOA_API ULOASkillTargeting_Self : public ULOASkillTargeting
{
	GENERATED_BODY()

public:
	//~ Begin ULOASkillTargeting Interface
	virtual void FindTargets(const FLOASkillModuleContext& Context, TArray<FLOATargetHit>& OutHits) const override;
	//~ End ULOASkillTargeting Interface
};
