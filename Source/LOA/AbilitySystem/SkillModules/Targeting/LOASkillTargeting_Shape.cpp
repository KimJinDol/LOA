// LOASkillTargeting_Shape.cpp

#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting_Shape.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillTargeting_Shape)

void ULOASkillTargeting_Shape::FindTargets(const FLOASkillModuleContext& Context, TArray<FLOATargetHit>& OutHits) const
{
	AActor* Source = Context.SourceActor;
	if (!Source)
	{
		return;
	}

	// ① 모양 안에서 Filter에 맞는 대상 (ASC 없는 대상, Filter에 안 맞는 대상은 라이브러리가 뺀다)
	//    모양을 놓는 곳 = SourceActor (캐릭터, 또는 낫/장판 같은 소환 액터)
	//    편 판단 기준   = Caster (항상 시전자. 소환 액터가 부를 때도 "내 편/적"이 맞게)
	TArray<AActor*> Found;
	//    범위 그리기: 이 Targeting의 Show Range 칸, 또는 콘솔 LOA.Debug.HitShape (패턴 테스터 창의 "판정 표시")
	const bool bDrawRange = bShowRange || ULOACombatLibrary::IsHitShapeDebugEnabled();
	ULOACombatLibrary::FindTargetsInShape(Source, Shape, Filter, ObjectType, Found, bDrawRange, Context.Caster);

	// ② 맞은 지점의 기준 = 모양의 기준점 (Sphere = 중심, Fan = 꼭짓점, Box = 뒷변 가운데).
	//    대상 캡슐 표면에서 이 점과 가장 가까운 곳이 맞은 지점이 된다.
	//    (보스 왼쪽에서 때리면 보스 캡슐의 왼쪽 면 → 데미지 숫자도 그 자리에 뜬다)
	const FVector ShapeCenter = ULOACombatLibrary::GetShapeCenter(Source, Shape);

	for (AActor* Target : Found)
	{
		// 이미 목록에 있는 대상은 넣지 않는다. (OutHits에 다른 Targeting 결과가 들어 있을 수 있음)
		const bool bAlreadyIn = OutHits.ContainsByPredicate(
			[Target](const FLOATargetHit& Existing) { return Existing.Target == Target; });
		if (!Target || bAlreadyIn)
		{
			continue;
		}

		// ③ 추가. 충돌체가 없는 대상(드묾)은 맞은 지점 없이 들어간다.
		FLOATargetHit& NewHit = OutHits.AddDefaulted_GetRef();
		NewHit.Target = Target;
		NewHit.bHasHitPoint = ULOACombatLibrary::MakeClosestHitResult(Target, ShapeCenter, NewHit.HitResult);
	}
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOASkillTargeting_Shape::ValidateTargeting(FDataValidationContext& Context, const FString& Prefix) const
{
	EDataValidationResult Result = Super::ValidateTargeting(Context, Prefix);

	// Fan: 안쪽 빈 곳이 길이 이상이면 맞을 곳이 없다. (실행 중에는 아무도 안 맞음)
	if (Shape.ShapeType == ELOAHitShapeType::Fan && Shape.InnerRadius >= Shape.Radius)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Fan Inner Radius (%.0f) must be less than Radius (%.0f). Nothing can be hit."),
			*Prefix, Shape.InnerRadius, Shape.Radius)));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
