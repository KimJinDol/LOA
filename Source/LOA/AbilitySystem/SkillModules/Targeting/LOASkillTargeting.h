// LOASkillTargeting.h
// [배관] Targeting의 부모. Targeting = "누구에게"를 정하는 부품.
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
// "이 정보(Context)로 대상을 찾아 줘" → 대상 목록을 돌려준다. 그게 전부다.
// 데미지를 줄지 힐을 줄지는 모른다. (그건 Apply 모듈의 "무엇을"이 정한다)
//
// 종류 (드롭다운)
//   Shape : 모양 안에서 Filter(적 / 아군 / 나 포함 아군)에 맞는 대상
//   Self  : 나 자신 (범위와 상관없이. 자기 버프, 광시곡의 나 몫)
//   나중에: 파티 전원, 가장 가까운 적 1명 …  → 클래스 하나 추가하면 드롭다운에 생긴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙]
// - 상태를 갖지 않는다. (FindTargets가 const) 같은 Context를 주면 항상 같은 결과.
//   → 발동이 겹치거나 순서가 꼬여도 이전 값이 남을 일이 없다.
// - 서버에서만 부른다. (부르는 쪽이 HasSkillModuleAuthority로 확인한 뒤 부른다)
// - 찾은 대상을 OutHits에 "추가"한다. (이미 들어 있는 대상과 겹치면 넣지 않는다)
//
// [왜 구조체가 아니라 객체인가]
// 종류마다 필요한 값이 다르다. (Shape는 반지름이 필요, Self는 아무 값도 필요 없음)
// 객체로 만들면 드롭다운에서 고른 종류의 값만 디테일 패널에 보인다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"
#include "LOASkillTargeting.generated.h"

UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class LOA_API ULOASkillTargeting : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * [서버] 대상을 찾아 OutHits에 추가한다.
	 * @param Context  누가, 몇 레벨로 (기준 액터의 위치/방향을 쓴다)
	 * @param OutHits  찾은 대상 + 맞은 지점
	 */
	virtual void FindTargets(const FLOASkillModuleContext& Context, TArray<FLOATargetHit>& OutHits) const
		PURE_VIRTUAL(ULOASkillTargeting::FindTargets, );

#if WITH_EDITOR
	/**
	 * [에디터 전용] 저장 시 검사. 이 Targeting을 가진 모듈(Apply)의 ValidateSkillModule이 부른다.
	 * 기본: 검사할 것 없음. 값 사이에 규칙이 있는 종류(Shape 등)만 덮어쓴다.
	 * @param Prefix  메시지 앞에 붙일 이름 (예: "GA_Whirlwind_C : Modules[0] Apply Targeting")
	 */
	virtual EDataValidationResult ValidateTargeting(class FDataValidationContext& Context, const FString& Prefix) const
	{
		return EDataValidationResult::Valid;
	}
#endif
};
