// LOACombatLibrary.cpp

#include "Library/LOACombatLibrary.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Combat/LOACombatRPCComponent.h"
#include "GameFramework/PlayerController.h"
#include "UI/Combat/DamageNumber/LOADamageNumberTypes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACombatLibrary)

// =============================================================================
// 부채꼴 계산 도우미 (이 파일 안에서만 씀)
// =============================================================================

namespace LOAFan
{
	/** 디버그 호를 그릴 때 몇 도마다 한 점을 찍을지 */
	constexpr float DebugDegreesPerSegment = 10.f;

	/**
	 * 위에서 본 바닥(XY) 기준: 대상의 몸통 원이 부채꼴에 조금이라도 걸치는가.
	 *
	 * @param Apex          꼭짓점
	 * @param Forward2D     부채꼴 방향 (높이 0, 길이 1)
	 * @param HalfAngleRad  전체 각도의 절반 (라디안). 90도 부채꼴이면 45도
	 * @param Radius        바깥 반지름 (길이)
	 * @param InnerRadius   안쪽 반지름 (0이면 없음)
	 * @param BodyCenter    대상 위치
	 * @param BodyRadius    대상 몸통 반지름 (충돌 캡슐 반지름)
	 *
	 * 판정 순서 (하나라도 확정되면 바로 결과를 돌려준다)
	 *   ① 거리: 몸이 바깥 호보다 멀면 안 맞음 / 몸 전체가 안쪽 빈 곳에 들어가 있으면 안 맞음
	 *   ② 360도면 ①만 통과하면 맞음
	 *   ③ 몸 중심 방향이 각도 안이면 맞음 (①에서 거리는 이미 통과)
	 *   ④ 몸 중심 방향이 각도 밖이면: 양쪽 가장자리 선분(안쪽 반지름 ~ 바깥 반지름)까지 거리 ≤ 몸 반지름이면 맞음
	 *      (각도 밖의 점에서 부채꼴까지 가장 가까운 곳은 항상 가장자리 선분 위에 있으므로 이것으로 충분하다)
	 */
	bool DoesBodyOverlapFan(
		const FVector& Apex, const FVector& Forward2D, float HalfAngleRad,
		float Radius, float InnerRadius,
		const FVector& BodyCenter, float BodyRadius)
	{
		// 꼭짓점 → 몸 중심 (바닥 기준, 높이 무시)
		const FVector ToBody(BodyCenter.X - Apex.X, BodyCenter.Y - Apex.Y, 0.f);
		const float Distance = ToBody.Size();

		// ① 거리
		if (Distance - BodyRadius > Radius)
		{
			return false;	// 바깥 호보다 멀다
		}
		if (Distance + BodyRadius < InnerRadius)
		{
			return false;	// 몸 전체가 안쪽 빈 곳 안
		}

		// ② 원 전체
		if (HalfAngleRad >= PI - KINDA_SMALL_NUMBER)
		{
			return true;
		}

		// ③ 몸 중심 방향이 각도 안
		if (Distance > KINDA_SMALL_NUMBER)
		{
			const float CosToBody = FVector::DotProduct(Forward2D, ToBody / Distance);
			if (CosToBody >= FMath::Cos(HalfAngleRad))
			{
				return true;
			}
		}

		// ④ 몸 중심 방향이 각도 밖 → 양쪽 가장자리 선분까지 거리 (꼭짓점 기준 좌표)
		for (const float Side : { 1.f, -1.f })
		{
			const FVector EdgeDir = Forward2D.RotateAngleAxisRad(Side * HalfAngleRad, FVector::UpVector);
			const FVector EdgeStart = EdgeDir * InnerRadius;
			const FVector EdgeEnd = EdgeDir * Radius;
			if (FMath::PointDistToSegment(ToBody, EdgeStart, EdgeEnd) <= BodyRadius)
			{
				return true;
			}
		}

		return false;
	}

#if ENABLE_DRAW_DEBUG
	/** 부채꼴을 바닥 높이(꼭짓점 높이)에 선으로 그린다. 양쪽 가장자리 + 바깥 호 + (있으면) 안쪽 호 */
	void DrawDebugFan(
		const UWorld* World, const FVector& Apex, const FVector& Forward2D, float HalfAngleRad,
		float Radius, float InnerRadius, const FColor& Color)
	{
		const float FullAngleDeg = FMath::RadiansToDegrees(HalfAngleRad * 2.f);
		const int32 NumSegments = FMath::Max(4, FMath::CeilToInt(FullAngleDeg / DebugDegreesPerSegment));
		const float StepRad = (HalfAngleRad * 2.f) / NumSegments;
		const bool bFullCircle = HalfAngleRad >= PI - KINDA_SMALL_NUMBER;

		FVector PrevOuter, PrevInner;
		for (int32 Index = 0; Index <= NumSegments; ++Index)
		{
			const FVector Dir = Forward2D.RotateAngleAxisRad(-HalfAngleRad + StepRad * Index, FVector::UpVector);
			const FVector Outer = Apex + Dir * Radius;
			const FVector Inner = Apex + Dir * InnerRadius;

			if (Index > 0)
			{
				DrawDebugLine(World, PrevOuter, Outer, Color, false, 1.f, 0, 2.f);
				if (InnerRadius > 0.f)
				{
					DrawDebugLine(World, PrevInner, Inner, Color, false, 1.f, 0, 2.f);
				}
			}

			// 양쪽 가장자리 (원 전체면 가장자리가 없다)
			if (!bFullCircle && (Index == 0 || Index == NumSegments))
			{
				DrawDebugLine(World, Inner, Outer, Color, false, 1.f, 0, 2.f);
			}

			PrevOuter = Outer;
			PrevInner = Inner;
		}
	}
#endif
}

// =============================================================================
// 직사각형 계산 도우미 (이 파일 안에서만 씀)
// =============================================================================

namespace LOABox
{
	/**
	 * 위에서 본 바닥(XY) 기준: 대상의 몸통 원이 직사각형에 조금이라도 걸치는가.
	 *
	 * @param Start       뒷변의 가운데
	 * @param Forward2D   직사각형이 뻗는 방향 (높이 0, 길이 1)
	 * @param Length      앞 길이
	 * @param HalfWidth   좌우 폭의 절반
	 * @param BodyCenter  대상 위치
	 * @param BodyRadius  대상 몸통 반지름 (충돌 캡슐 반지름)
	 *
	 * 판정 순서
	 *   ① 대상 위치를 직사각형 기준 좌표로 바꾼다. (내적: "앞으로 몇 cm, 옆으로 몇 cm")
	 *      → 직사각형이 어느 방향으로 돌아가 있어도 "앞 0~Length, 옆 -HalfWidth~+HalfWidth"인 똑바로 선 상자가 된다.
	 *   ② 앞/옆 좌표를 각각 상자 범위로 자른다. (Clamp) = 상자 안에서 대상 중심과 가장 가까운 점
	 *      (안이면 그대로 → 자기 자신 / 한쪽만 밖이면 그 벽 위 / 두 쪽 다 밖이면 모서리)
	 *   ③ 그 점까지 거리 ≤ 몸통 반지름이면 맞음. (제곱끼리 비교: 둘 다 0 이상이라 결과가 같고 √ 계산을 아낀다)
	 */
	bool DoesBodyOverlapBox(
		const FVector& Start, const FVector& Forward2D, float Length, float HalfWidth,
		const FVector& BodyCenter, float BodyRadius)
	{
		// 옆 방향 = 앞 방향을 오른쪽으로 90도 (위에서 볼 때). 직사각형이 좌우 대칭이라 왼쪽이어도 결과는 같다.
		const FVector Right2D(-Forward2D.Y, Forward2D.X, 0.f);

		// ① 뒷변 가운데 → 대상 (바닥 기준, 높이 무시)을 직사각형 기준 좌표로
		const FVector ToBody(BodyCenter.X - Start.X, BodyCenter.Y - Start.Y, 0.f);
		const float Front = FVector::DotProduct(ToBody, Forward2D);	// 앞으로 몇 cm (뒤면 음수)
		const float Side = FVector::DotProduct(ToBody, Right2D);	// 옆으로 몇 cm (반대쪽이면 음수)

		// ② 상자 안에서 가장 가까운 점
		const float ClosestFront = FMath::Clamp(Front, 0.f, Length);
		const float ClosestSide = FMath::Clamp(Side, -HalfWidth, HalfWidth);

		// ③ 거리 비교
		const float DistSq = FMath::Square(Front - ClosestFront) + FMath::Square(Side - ClosestSide);
		return DistSq <= FMath::Square(BodyRadius);
	}

#if ENABLE_DRAW_DEBUG
	/** 직사각형을 바닥 높이(뒷변 가운데 높이)에 네 변으로 그린다. */
	void DrawDebugBox2D(
		const UWorld* World, const FVector& Start, const FVector& Forward2D, float Length, float HalfWidth, const FColor& Color)
	{
		const FVector Right2D(-Forward2D.Y, Forward2D.X, 0.f);
		const FVector BackLeft = Start - Right2D * HalfWidth;
		const FVector BackRight = Start + Right2D * HalfWidth;
		const FVector FrontLeft = BackLeft + Forward2D * Length;
		const FVector FrontRight = BackRight + Forward2D * Length;

		DrawDebugLine(World, BackLeft, BackRight, Color, false, 1.f, 0, 2.f);
		DrawDebugLine(World, BackRight, FrontRight, Color, false, 1.f, 0, 2.f);
		DrawDebugLine(World, FrontRight, FrontLeft, Color, false, 1.f, 0, 2.f);
		DrawDebugLine(World, FrontLeft, BackLeft, Color, false, 1.f, 0, 2.f);
	}
#endif
}

// =============================================================================
// 대상 찾기 (Filter: 적 / 아군)
// =============================================================================

FVector ULOACombatLibrary::GetShapeCenter(const AActor* Attacker, const FLOAHitShape& Shape)
{
	if (!Attacker)
	{
		return FVector::ZeroVector;
	}

	// 중심 = 공격자 위치 + 앞 방향 × ForwardDistance
	return Attacker->GetActorLocation() + Attacker->GetActorForwardVector() * Shape.ForwardDistance;
}

int32 ULOACombatLibrary::FindTargetsInShape(
	const AActor* Source,
	const FLOAHitShape& Shape,
	ELOATargetFilter Filter,
	TEnumAsByte<ECollisionChannel> ObjectType,
	TArray<AActor*>& OutTargets,
	bool bShowRange,
	const AActor* Caster)
{
	if (!Source)
	{
		return 0;
	}

	// 모든 모양 공통 기준점 (Sphere = 중심, Fan = 꼭짓점, Box = 뒷변 가운데)
	// 이 시점의 기준 액터는 이미 커서 방향으로 돌아 있다. → 앞 방향 = 스킬 방향
	const FVector Center = GetShapeCenter(Source, Shape);

	// 모양에 맞는 함수 고르기. 모양을 추가하면 여기에 case를 추가한다.
	switch (Shape.ShapeType)
	{
	case ELOAHitShapeType::Sphere:
		return FindTargetsInSphere(Source, Center, Shape.Radius, Filter, ObjectType, OutTargets, bShowRange, Caster, Shape.bHitByBody);

	case ELOAHitShapeType::Fan:
		return FindTargetsInFan(Source, Center, Source->GetActorForwardVector(),
			Shape.Radius, Shape.InnerRadius, Shape.Angle, Filter, ObjectType, OutTargets, bShowRange, Caster, Shape.bHitByBody);

	case ELOAHitShapeType::Box:
		// 기준점 = 뒷변 가운데. 기준 액터의 앞 방향으로 뻗는다.
		return FindTargetsInBox(Source, Center, Source->GetActorForwardVector(),
			Shape.Length, Shape.Width, Filter, ObjectType, OutTargets, bShowRange, Caster, Shape.bHitByBody);

	default:
		// 새 모양을 Enum에만 추가하고 여기를 빼먹으면 알 수 있게
		ensureMsgf(false, TEXT("FindTargetsInShape: unknown shape type %d"), static_cast<int32>(Shape.ShapeType));
		return 0;
	}
}

int32 ULOACombatLibrary::FindTargetsInSphere(
	const AActor* Source,
	FVector Center,
	float Radius,
	ELOATargetFilter Filter,
	TEnumAsByte<ECollisionChannel> ObjectType,
	TArray<AActor*>& OutTargets,
	bool bShowRange,
	const AActor* Caster,
	bool bHitByBody)
{
	UWorld* World = Source ? Source->GetWorld() : nullptr;
	if (!World || Radius <= 0.f)
	{
		return 0;
	}

	// 편 판단 기준 ("나"). 시전자를 따로 받았으면 그쪽, 아니면 범위 기준 액터 자신 (캐릭터가 직접 부를 때는 둘이 같다)
	const AActor* FilterSource = Caster ? Caster : Source;

	// ① 구 범위 안의 ObjectType 물체 검색.
	//    나 자신도 검색에 포함한다. (AllyAndSelf가 나를 찾아야 하므로) 뺄지 말지는 ④의 Filter가 정한다.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LOAFindTargetsInSphere), false);
	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ObjectType),
		FCollisionShape::MakeSphere(Radius),
		Params);

	// 목록에 원래 들어 있던 개수. "이번에 새로 찾은 수"를 계산하는 데 쓴다.
	// (범위 여러 개를 한 목록에 이어서 모을 때: 앞쪽 원 + 주변 원 등)
	const int32 NumBefore = OutTargets.Num();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();

		// ② 중복 제외 (한 액터에 콜리전이 여러 개여도 한 번만)
		if (!Target || OutTargets.Contains(Target))
		{
			continue;
		}

		// ③ 맞을 수 있는 대상인가 (ASC 있음 + 죽지 않음). 벽, 소품, 시체 제외
		if (!IsTargetable(Target))
		{
			continue;
		}

		// ④ 편 거르기 (적 / 나를 뺀 아군 / 나를 포함한 아군)
		if (!PassesFilter(FilterSource, Target, Filter))
		{
			continue;
		}

		// ⑤ 발 기준이면: 겹침 검색은 몸통 기준이라 걸치기만 한 대상도 들어온다. → 중심점이 원 안(바닥 거리)인지 다시 본다.
		if (!bHitByBody && FVector::DistSquared2D(Center, Target->GetActorLocation()) > FMath::Square(Radius))
		{
			continue;
		}

		OutTargets.Add(Target);
	}

	const int32 NumFound = OutTargets.Num() - NumBefore;

#if ENABLE_DRAW_DEBUG
	// 이 함수를 부른 월드(보통 서버)에 그린다.
	// - 스탠드얼론 / 리슨 서버 호스트 : 그 화면에 보인다.
	// - PIE 한 프로세스 실행 + 데디케이티드 서버 : 엔진이 같은 프로세스의 클라이언트 창에 대신 그려 준다. (에디터 전용 편의 기능)
	// - 실제 데디케이티드 서버(패키징) : 보이지 않는다.
	if (bShowRange)
	{
		DrawDebugSphere(World, Center, Radius, 16, NumFound > 0 ? FColor::Red : FColor::Green, false, 1.f);
	}
#endif

	return NumFound;
}

int32 ULOACombatLibrary::FindTargetsInFan(
	const AActor* Source,
	FVector Apex,
	FVector Direction,
	float Radius,
	float InnerRadius,
	float Angle,
	ELOATargetFilter Filter,
	TEnumAsByte<ECollisionChannel> ObjectType,
	TArray<AActor*>& OutTargets,
	bool bShowRange,
	const AActor* Caster,
	bool bHitByBody)
{
	UWorld* World = Source ? Source->GetWorld() : nullptr;
	if (!World || Radius <= 0.f || InnerRadius >= Radius)
	{
		return 0;
	}

	// 방향은 바닥 기준 (높이 무시). 방향이 없으면(위/아래만 향함) 판정할 수 없다.
	const FVector Forward2D = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
	if (Forward2D.IsNearlyZero())
	{
		return 0;
	}

	const float HalfAngleRad = FMath::DegreesToRadians(FMath::Clamp(Angle, 1.f, 360.f) * 0.5f);
	const float SafeInnerRadius = FMath::Max(InnerRadius, 0.f);

	// ① 후보: 꼭짓점 중심, 반지름 Radius 구. ASC / Filter / 중복은 구 검색이 이미 거른다.
	//    (구 검색은 실제 충돌체 기준이라 몸이 구에 걸친 대상까지 들어온다)
	//    범위 그리기는 구가 아니라 아래에서 부채꼴로 그린다.
	TArray<AActor*> Candidates;
	FindTargetsInSphere(Source, Apex, Radius, Filter, ObjectType, Candidates, false, Caster);

	const int32 NumBefore = OutTargets.Num();

	for (AActor* Target : Candidates)
	{
		// 이미 목록에 있으면 넣지 않는다. (범위 여러 개를 한 목록에 이어서 모을 때)
		if (!Target || OutTargets.Contains(Target))
		{
			continue;
		}

		// ② 몸통 원이 부채꼴에 걸치는가. 몸통 반지름 = 충돌 캡슐 반지름 (캐릭터/보스)
		//    발 기준이면 몸통 반지름 0 = 중심점이 부채꼴 안인가
		const float BodyRadius = bHitByBody ? Target->GetSimpleCollisionRadius() : 0.f;
		if (LOAFan::DoesBodyOverlapFan(Apex, Forward2D, HalfAngleRad, Radius, SafeInnerRadius,
			Target->GetActorLocation(), BodyRadius))
		{
			OutTargets.Add(Target);
		}
	}

	const int32 NumFound = OutTargets.Num() - NumBefore;

#if ENABLE_DRAW_DEBUG
	// 그려지는 화면은 FindTargetsInSphere와 같다. (스탠드얼론 / 리슨 호스트 / PIE 데디는 보임, 실제 데디는 안 보임)
	if (bShowRange)
	{
		LOAFan::DrawDebugFan(World, Apex, Forward2D, HalfAngleRad, Radius, SafeInnerRadius,
			NumFound > 0 ? FColor::Red : FColor::Green);
	}
#endif

	return NumFound;
}

int32 ULOACombatLibrary::FindTargetsInBox(
	const AActor* Source,
	FVector Start,
	FVector Direction,
	float Length,
	float Width,
	ELOATargetFilter Filter,
	TEnumAsByte<ECollisionChannel> ObjectType,
	TArray<AActor*>& OutTargets,
	bool bShowRange,
	const AActor* Caster,
	bool bHitByBody)
{
	UWorld* World = Source ? Source->GetWorld() : nullptr;
	if (!World || Length <= 0.f || Width <= 0.f)
	{
		return 0;
	}

	// 방향은 바닥 기준 (높이 무시). 방향이 없으면(위/아래만 향함) 판정할 수 없다.
	const FVector Forward2D = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
	if (Forward2D.IsNearlyZero())
	{
		return 0;
	}

	const float HalfWidth = Width * 0.5f;

	// ① 후보: 직사각형을 다 덮는 구. 중심 = 직사각형 중심, 반지름 = 대각선의 절반.
	//    ASC / Filter / 중복은 구 검색이 이미 거른다. 범위 그리기는 구가 아니라 아래에서 직사각형으로 그린다.
	const FVector BoxCenter = Start + Forward2D * (Length * 0.5f);
	const float CoverRadius = FMath::Sqrt(FMath::Square(Length * 0.5f) + FMath::Square(HalfWidth));

	TArray<AActor*> Candidates;
	FindTargetsInSphere(Source, BoxCenter, CoverRadius, Filter, ObjectType, Candidates, false, Caster);

	const int32 NumBefore = OutTargets.Num();

	for (AActor* Target : Candidates)
	{
		// 이미 목록에 있으면 넣지 않는다. (범위 여러 개를 한 목록에 이어서 모을 때)
		if (!Target || OutTargets.Contains(Target))
		{
			continue;
		}

		// ② 몸통 원이 직사각형에 걸치는가. 몸통 반지름 = 충돌 캡슐 반지름 (캐릭터/보스)
		//    발 기준이면 몸통 반지름 0 = 중심점이 직사각형 안인가
		const float BodyRadius = bHitByBody ? Target->GetSimpleCollisionRadius() : 0.f;
		if (LOABox::DoesBodyOverlapBox(Start, Forward2D, Length, HalfWidth, Target->GetActorLocation(), BodyRadius))
		{
			OutTargets.Add(Target);
		}
	}

	const int32 NumFound = OutTargets.Num() - NumBefore;

#if ENABLE_DRAW_DEBUG
	// 그려지는 화면은 FindTargetsInSphere와 같다. (스탠드얼론 / 리슨 호스트 / PIE 데디는 보임, 실제 데디는 안 보임)
	if (bShowRange)
	{
		LOABox::DrawDebugBox2D(World, Start, Forward2D, Length, HalfWidth, NumFound > 0 ? FColor::Red : FColor::Green);
	}
#endif

	return NumFound;
}

bool ULOACombatLibrary::IsBodyInShape(const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw,
	const FVector& BodyLocation, float BodyRadius)
{
	// 방향 (바닥 기준, 길이 1)
	const FVector Forward2D = FRotator(0.f, Yaw, 0.f).Vector().GetSafeNormal2D();

	// 몸통 기준이면 몸통 반지름만큼 넓게, 발 기준이면 중심점만
	const float HitRadius = Shape.bHitByBody ? FMath::Max(BodyRadius, 0.f) : 0.f;

	switch (Shape.ShapeType)
	{
	case ELOAHitShapeType::Sphere:
		// 바닥 거리 ≤ 반지름 + 몸통 반지름
		return Shape.Radius > 0.f
			&& FVector::DistSquared2D(ShapeCenter, BodyLocation) <= FMath::Square(Shape.Radius + HitRadius);

	case ELOAHitShapeType::Fan:
	{
		if (Shape.Radius <= 0.f || Shape.InnerRadius >= Shape.Radius || Forward2D.IsNearlyZero())
		{
			return false;
		}
		const float HalfAngleRad = FMath::DegreesToRadians(FMath::Clamp(Shape.Angle, 1.f, 360.f) * 0.5f);
		return LOAFan::DoesBodyOverlapFan(ShapeCenter, Forward2D, HalfAngleRad, Shape.Radius, FMath::Max(Shape.InnerRadius, 0.f),
			BodyLocation, HitRadius);
	}

	case ELOAHitShapeType::Box:
		if (Shape.Length <= 0.f || Shape.Width <= 0.f || Forward2D.IsNearlyZero())
		{
			return false;
		}
		return LOABox::DoesBodyOverlapBox(ShapeCenter, Forward2D, Shape.Length, Shape.Width * 0.5f, BodyLocation, HitRadius);

	default:
		ensureMsgf(false, TEXT("IsBodyInShape: unknown shape type %d"), static_cast<int32>(Shape.ShapeType));
		return false;
	}
}

// =============================================================================
// 디버그: 판정 모양 그리기
// =============================================================================

namespace LOAHitShapeDebug
{
	/** LOA.Debug.HitShape 1 = 판정 모양 그리기. 컴퓨터마다 따로 (PIE 한 프로세스면 창 전부 같이) */
	static TAutoConsoleVariable<int32> CVarShowHitShape(
		TEXT("LOA.Debug.HitShape"),
		0,
		TEXT("1 = draw hit shapes when they are judged (server: shape targeting, client: victim judge with my body)."),
		ECVF_Cheat);
}

bool ULOACombatLibrary::IsHitShapeDebugEnabled()
{
#if ENABLE_DRAW_DEBUG
	return LOAHitShapeDebug::CVarShowHitShape.GetValueOnGameThread() != 0;
#else
	return false;
#endif
}

void ULOACombatLibrary::SetHitShapeDebugEnabled(bool bEnabled)
{
	LOAHitShapeDebug::CVarShowHitShape->Set(bEnabled ? 1 : 0, ECVF_SetByConsole);
}

void ULOACombatLibrary::DrawDebugHitShape(const UWorld* World, const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	if (!World)
	{
		return;
	}

	const FVector Forward2D = FRotator(0.f, Yaw, 0.f).Vector().GetSafeNormal2D();
	switch (Shape.ShapeType)
	{
	case ELOAHitShapeType::Sphere:
		// 판정이 바닥 거리라 바닥 원으로 그린다.
		DrawDebugCircle(World, ShapeCenter, Shape.Radius, 48, Color, false, 1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
		break;

	case ELOAHitShapeType::Fan:
		LOAFan::DrawDebugFan(World, ShapeCenter, Forward2D, FMath::DegreesToRadians(FMath::Clamp(Shape.Angle, 1.f, 360.f) * 0.5f),
			Shape.Radius, FMath::Max(Shape.InnerRadius, 0.f), Color);
		break;

	case ELOAHitShapeType::Box:
		LOABox::DrawDebugBox2D(World, ShapeCenter, Forward2D, Shape.Length, Shape.Width * 0.5f, Color);
		break;

	default:
		break;
	}
#endif
}

// =============================================================================
// 편 판단
// =============================================================================

bool ULOACombatLibrary::IsTargetable(const AActor* Target)
{
	// ① 전투 참여자인가. ASC가 없으면 효과를 받을 수 없다. (벽, 소품)
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!ASC)
	{
		return false;
	}

	// ② 살아 있는가. State.Dead는 GA_Death가 붙이고 모든 컴퓨터에 복제된다.
	if (ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Dead))
	{
		return false;
	}

	// ③ 몸이 있는가. State.Hidden = 하늘로 사라진 보스 등 (LOA Grant Tags 구간)
	return !ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Hidden);
}

int32 ULOACombatLibrary::FindLivingEnemyPlayers(const AActor* Source, TArray<AActor*>& OutEnemies)
{
	const UWorld* World = Source ? Source->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return 0;
	}

	const int32 NumBefore = OutEnemies.Num();

	// 파티원 목록(PlayerArray)에서 각자의 캐릭터를 꺼낸다. (레벨 전체를 뒤지지 않는다)
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;

		// 살아 있고(맞을 수 있고) 적인 플레이어만
		if (Pawn && IsTargetable(Pawn) && IsEnemy(Source, Pawn))
		{
			OutEnemies.AddUnique(Pawn);
		}
	}

	return OutEnemies.Num() - NumBefore;
}

bool ULOACombatLibrary::IsEnemy(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}

	// [임시 규칙] PlayerState가 있는 Pawn = 플레이어 편
	auto IsPlayerSide = [](const AActor* Actor)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		return Pawn && Pawn->GetPlayerState() != nullptr;
	};

	// 편이 다르면 적
	return IsPlayerSide(A) != IsPlayerSide(B);
}

bool ULOACombatLibrary::IsHeadAttack(const AActor* Target, const AActor* Attacker, float HalfAngleDegrees)
{
	if (!Target || !Attacker || Target == Attacker)
	{
		return false;
	}

	// 바닥 기준: 맞은 쪽 → 때린 사람 방향과 맞은 쪽이 보는 방향 (높이 무시)
	const FVector ToAttacker = (Attacker->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();

	// 같은 자리에 겹쳐 서 있으면 방향을 알 수 없다 → 헤드 아님
	if (ToAttacker.IsNearlyZero() || Forward.IsNearlyZero())
	{
		return false;
	}

	// 두 방향의 내적 = cos(사이 각). 반각 안이면 cos(반각) 이상
	//   예) 반각 45 → 기준 0.707: 정면 0° → 1.0 (헤드) / 30° → 0.87 (헤드) / 90° 옆 → 0 (아님) / 뒤 180° → -1 (아님)
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
	return FVector::DotProduct(Forward, ToAttacker) >= CosHalfAngle;
}

bool ULOACombatLibrary::IsAlly(const AActor* A, const AActor* B)
{
	// 자기 자신은 아군에 넣지 않는다. (나를 넣을지는 Filter가 정한다)
	if (!A || !B || A == B)
	{
		return false;
	}

	// 전투에 참여하는 액터끼리만 편을 따진다. (벽, 소품은 아군도 적도 아님)
	if (!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(A) ||
		!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(B))
	{
		return false;
	}

	return !IsEnemy(A, B);
}

bool ULOACombatLibrary::PassesFilter(const AActor* Source, const AActor* Target, ELOATargetFilter Filter)
{
	if (!Source || !Target)
	{
		return false;
	}

	switch (Filter)
	{
	case ELOATargetFilter::Enemy:
		return IsEnemy(Source, Target);

	case ELOATargetFilter::Ally:
		return IsAlly(Source, Target);

	case ELOATargetFilter::AllyAndSelf:
		return Target == Source || IsAlly(Source, Target);

	default:
		ensureMsgf(false, TEXT("PassesFilter: unknown filter %d"), static_cast<int32>(Filter));
		return false;
	}
}

// =============================================================================
// 효과 적용 (Payload) - 데미지, 힐, 버프 공용
// =============================================================================

void ULOACombatLibrary::ApplyPayload(
	const FLOASkillModuleContext& Context,
	const FLOATargetHit& Target,
	const FLOAEffectPayload& Payload)
{
	// 효과는 서버만 준다. (보낸 쪽 ASC의 주인이 서버 권한을 가졌는지로 확인)
	UAbilitySystemComponent* SourceASC = Context.SourceASC;
	if (!SourceASC || !Target.Target || !Payload.Effect || !SourceASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// ① 받는 쪽 ASC 찾기
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target.Target);
	if (!TargetASC)
	{
		return;
	}

	// ② GE Spec 만들기
	//    EffectContext = "누가 보냈나" 기록 (Instigator = 보낸 ASC의 주인, EffectCauser = 보낸 쪽 몸)
	//    Spec          = "이 GE를 이 레벨로, 이 EffectContext로" 적용할 준비물
	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();

	//    맞은 지점이 있으면 EffectContext에 기록 → 받는 쪽 AttributeSet이 꺼내서 데미지 숫자 위치로 쓴다.
	//    bReset = true: 혹시 남아 있던 HitResult를 덮어쓴다.
	if (Target.bHasHitPoint)
	{
		EffectContext.AddHitResult(Target.HitResult, true);
	}

	//    때린 것(범위 기준 액터: 장판·투사체, 직접 치면 시전자 몸)을 기록 → 받는 쪽이 "무엇에 맞았나"를 알 수 있다.
	//    예) 띄움(ULOAGA_Airborne)의 Source Forward = 장풍 장판의 앞 방향. 피해자 판정은 보고가 늦게 오므로 그사이 사라졌으면 비어 있다.
	if (IsValid(Context.SourceActor))
	{
		EffectContext.AddSourceObject(Context.SourceActor);
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Payload.Effect, Context.Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// ③ 차지 배율은 모든 GE에 항상 넣는다. (차지 스킬이 아니면 1)
	//    읽는 쪽(LOADamageExecution)만 곱하고, 읽지 않는 GE(버프 등)에는 영향이 없다.
	//    → 스킬 데이터에 "차지 배율을 곱할지" 같은 칸이 필요 없다.
	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ChargeMultiplier, Context.ChargeMultiplier);

	// ④ Payload의 값들을 SetByCaller로 넣기
	//    예: SetByCaller.DamageRate = 2.5 → ULOADamageExecution이 공격력 × 2.5를 계산
	//    값은 양수 그대로 넣는다. (데미지는 Damage(메타)에 "더해서" 받는 쪽이 체력을 깎는다)
	for (const FLOASetByCallerValue& Value : Payload.Values)
	{
		if (!Value.Tag.IsValid())
		{
			continue;	// 디테일 패널에서 태그를 안 고른 칸
		}
		SpecHandle.Data->SetSetByCallerMagnitude(Value.Tag, Value.Value.GetValueAtLevel(Context.Level));
	}

	// ⑤ 보낸 ASC → 받는 ASC로 적용 (자기 자신에게 주는 버프면 둘이 같은 ASC)
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void ULOACombatLibrary::ApplyPayloads(
	const FLOASkillModuleContext& Context,
	const TArray<FLOATargetHit>& Targets,
	const TArray<FLOAEffectPayload>& Payloads)
{
	// 대상마다, Payload마다 한 번씩. (대상 3명 × Payload 2개 = GE 적용 6번)
	for (const FLOATargetHit& Target : Targets)
	{
		for (const FLOAEffectPayload& Payload : Payloads)
		{
			ApplyPayload(Context, Target, Payload);
		}

		// 효과를 다 준 뒤 "맞았다"를 알린다. (이 데미지로 죽었으면 사망이 패턴을 취소해서 카운터 모듈이 받지 않는다)
		SendHitEvent(Context, Target.Target);
	}
}

void ULOACombatLibrary::SendHitEvent(const FLOASkillModuleContext& Context, AActor* Target)
{
	// 서버만. 적을 쳤을 때만 (아군 힐·버프는 "맞음"이 아니다)
	if (!Target || !Context.SourceASC || !Context.SourceASC->IsOwnerActorAuthoritative() || !IsEnemy(Context.Caster, Target))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		return;
	}

	// 무엇을 담나: 누가(시전자), 어떤 속성의 공격으로(예: { Skill.Attribute.Counter }). 그 뜻은 받는 쪽이 정한다.
	FGameplayEventData Payload;
	Payload.EventTag = LOAGameplayTags::Event_Combat_Hit;
	Payload.Instigator = Context.Caster;
	Payload.Target = Target;
	Payload.InstigatorTags = Context.AttackTags;
	TargetASC->HandleGameplayEvent(LOAGameplayTags::Event_Combat_Hit, &Payload);
}

// =============================================================================
// 맞은 지점
// =============================================================================

bool ULOACombatLibrary::GetClosestPointOnTarget(const AActor* Target, FVector FromLocation, FVector& OutPoint)
{
	if (!Target)
	{
		OutPoint = FromLocation;
		return false;
	}

	OutPoint = Target->GetActorLocation();

	// 충돌체 = 루트 컴포넌트 (캐릭터면 캡슐). 지금은 충돌체를 하나만 쓴다.
	const UPrimitiveComponent* Collision = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	if (!Collision)
	{
		return false;
	}

	// 반환값: 표면까지 거리 (안쪽이면 0) / 실패(충돌 꺼짐 등)면 음수
	FVector PointOnBody;
	if (Collision->GetClosestPointOnCollision(FromLocation, PointOnBody) < 0.f)
	{
		return false;
	}

	OutPoint = PointOnBody;
	return true;
}

bool ULOACombatLibrary::MakeClosestHitResult(AActor* Target, FVector FromLocation, FHitResult& OutHit)
{
	FVector HitPoint;
	if (!GetClosestPointOnTarget(Target, FromLocation, HitPoint))
	{
		return false;
	}

	// 맞은 면의 방향 = 기준점 → 맞은 지점의 반대 (기준점이 안쪽이면 방향 없음)
	const FVector Normal = (FromLocation - HitPoint).GetSafeNormal();

	OutHit = FHitResult(Target, Cast<UPrimitiveComponent>(Target->GetRootComponent()), HitPoint, Normal);
	OutHit.TraceStart = FromLocation;
	OutHit.TraceEnd = HitPoint;
	return true;
}

// =============================================================================
// 데미지 알림
// =============================================================================

void ULOACombatLibrary::NotifyDamageDealt(AActor* Instigator, AActor* Target, float Damage, const FHitResult* HitResult)
{
	if (!Instigator || !Target || Damage <= 0.f || !Instigator->HasAuthority())
	{
		return;
	}

	// 보낸 사람 → 그 플레이어의 PlayerController
	//   플레이어 Ability가 만든 데미지: 보낸 사람 = PlayerState (ASC의 주인)
	//   혹시 Pawn이나 Controller가 들어와도 찾을 수 있게 한다.
	APlayerController* PC = nullptr;
	if (const APlayerState* PS = Cast<APlayerState>(Instigator))
	{
		PC = PS->GetPlayerController();
	}
	else if (const APawn* Pawn = Cast<APawn>(Instigator))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	else
	{
		PC = Cast<APlayerController>(Instigator);
	}

	// 보스/몬스터가 때린 경우는 PlayerController가 없다 → 여기서 끝
	// PC 클래스를 직접 알 필요 없이 전투 RPC 컴포넌트만 찾는다.
	ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr;
	if (CombatRPC)
	{
		FLOADamageNumberInfo Info;
		Info.Damage = Damage;

		// 맞은 지점 → 대상 기준 상대 위치 (클라의 대상 위치에 더해서 쓴다. 지연으로 위치가 어긋나도 대상에 붙어 보이게)
		// 맞은 지점이 없으면(도트 등) 비워 둔다. → 받은 클라가 내 캐릭터 기준으로 계산
		if (HitResult)
		{
			Info.bHasHitOffset = true;
			Info.HitOffset = HitResult->ImpactPoint - Target->GetActorLocation();
		}

		// 그 플레이어 컴퓨터로만 간다. (리슨 서버 호스트 본인이면 바로 실행)
		CombatRPC->ClientShowDamageNumber(Target, Info);
	}
}
