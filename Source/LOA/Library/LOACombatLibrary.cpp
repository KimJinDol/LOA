// LOACombatLibrary.cpp

#include "Library/LOACombatLibrary.h"
#include "Library/LOACombatSettings.h"
#include "AbilitySystem/LOAGameplayEffectContext.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "AbilitySystem/Effects/LOAGE_InfiniteTags.h"
#include "AbilitySystem/Effects/LOAGE_TimedTags.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Core/LOANetTime.h"
#include "Core/LOACollisionChannels.h"

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

	// 공격자의 지금 자리·방향으로 (규칙은 GetShapeCenterAt 한 곳)
	return GetShapeCenterAt(Attacker->GetWorld(), Attacker->GetActorLocation(), Attacker->GetActorForwardVector(), Shape, Attacker);
}

FVector ULOACombatLibrary::GetShapeCenterAt(const UWorld* World, const FVector& AttackerLocation, const FVector& AttackerForward,
	const FLOAHitShape& Shape, const AActor* IgnoreActor)
{
	// 중심 = 공격자 위치 + 앞 방향 × ForwardDistance
	const FVector Center = AttackerLocation + AttackerForward * Shape.ForwardDistance;
	if (!Shape.bSnapToGround)
	{
		return Center;
	}

	// 땅에 붙이기: 그 아래 바닥 높이로 (못 찾으면 그대로). 예) 보스 캡슐 중심(바닥 위 300)에서 소환 → 바닥
	FVector Ground;
	FindGroundLocation(World, Center, Ground, IgnoreActor);
	return Ground;
}

namespace LOAGround
{
	/** 바닥 찾기 트레이스: 위로 이만큼에서 시작 (cm). Offset Z가 음수라 바닥보다 살짝 아래에서 시작해도 찾게 */
	constexpr float TraceUp = 300.f;

	/** 바닥 찾기 트레이스: 아래로 이만큼까지 (cm). 큰 보스 캡슐 중심 높이보다 넉넉히 */
	constexpr float TraceDown = 2000.f;
}

bool ULOACombatLibrary::FindGroundLocation(const UWorld* World, const FVector& Location, FVector& OutGround, const AActor* IgnoreActor)
{
	OutGround = Location;
	if (!World)
	{
		return false;
	}

	// 바닥(Ground 채널을 막는 것 = Floor 프로필)만. 캐릭터·보스 캡슐, 장애물 프랍, 벽은 안 본다 (2026-11-14, LOACollisionChannels.h [Ground])
	//   → 보스 캡슐 안에서 시작해도 보스 몸에 걸리지 않고, 프랍 위에서 생긴 장판도 프랍 아래 바닥에 붙는다
	//   (전에는 WorldStatic 물체 트레이스: 프랍 윗면에 붙고, WorldDynamic 바닥(BP에 붙인 메시)은 못 찾았다)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LOAFindGround), false, IgnoreActor);
	FHitResult Hit;
	const bool bFound = World->LineTraceSingleByChannel(
		Hit,
		Location + FVector(0.f, 0.f, LOAGround::TraceUp),
		Location - FVector(0.f, 0.f, LOAGround::TraceDown),
		LOACollision::Ground,
		Params);
	if (bFound)
	{
		OutGround.Z = Hit.ImpactPoint.Z;
	}
	return bFound;
}

// 모양 안의 대상 찾기 (네 함수 공통). 헤더 FindTargetsInShape [판정]
namespace LOAShapeSearch
{
	/**
	 * ① 후보: 모양을 덮는 "위아래로 긴 상자"로 겹침 검색 (높이 = 기준점 ± Shape.HalfHeight)
	 *      Sphere / Fan : 기준점 중심, 가로세로 = 반지름 × 2 (부채꼴도 원을 덮는 상자. 각도는 ②에서)
	 *      Box          : 직사각형 그대로 (중심 = 뒷변 가운데 + 앞으로 길이/2, 보는 방향으로 돌림)
	 *    겹침 검색은 실제 충돌체 기준이라 몸이 상자에 걸친 대상까지 들어온다.
	 * ② 후보마다: 맞을 수 있나(IsTargetable) / 편(PassesFilter) / 모양 안인가(IsBodyInShape = 바닥 모양 + 높이)
	 *    → 서버 검색과 피해자 판정(클라)이 ②의 같은 함수로 같은 결과를 낸다
	 */
	int32 FindTargets(
		const AActor* Source, const FLOAHitShape& Shape, const FVector& Center, float Yaw,
		ELOATargetFilter Filter, TEnumAsByte<ECollisionChannel> ObjectType,
		TArray<AActor*>& OutTargets, bool bShowRange, const AActor* Caster)
	{
		UWorld* World = Source ? Source->GetWorld() : nullptr;
		if (!World)
		{
			return 0;
		}

		// 편 판단 기준 ("나"). 시전자를 따로 받았으면 그쪽, 아니면 범위 기준 액터 자신 (캐릭터가 직접 부를 때는 둘이 같다)
		const AActor* FilterSource = Caster ? Caster : Source;
		const float HalfHeight = FMath::Max(Shape.HalfHeight, 1.f);

		// ① 후보 상자 (중심, 반 크기, 회전)
		FVector BoxCenter = Center;
		FVector BoxHalfExtent;
		FQuat BoxRotation = FQuat::Identity;
		switch (Shape.ShapeType)
		{
		case ELOAHitShapeType::Sphere:
		case ELOAHitShapeType::Fan:
			if (Shape.Radius <= 0.f)
			{
				return 0;
			}
			BoxHalfExtent = FVector(Shape.Radius, Shape.Radius, HalfHeight);
			break;

		case ELOAHitShapeType::Box:
		{
			if (Shape.Length <= 0.f || Shape.Width <= 0.f)
			{
				return 0;
			}
			const FRotator Facing(0.f, Yaw, 0.f);
			BoxCenter = Center + Facing.Vector() * (Shape.Length * 0.5f);
			BoxHalfExtent = FVector(Shape.Length * 0.5f, Shape.Width * 0.5f, HalfHeight);
			BoxRotation = Facing.Quaternion();
			break;
		}

		default:
			// 새 모양을 Enum에만 추가하고 여기를 빼먹으면 알 수 있게
			ensureMsgf(false, TEXT("FindTargetsInShape: unknown shape type %d"), static_cast<int32>(Shape.ShapeType));
			return 0;
		}

		// 나 자신도 검색에 포함한다. (AllyAndSelf가 나를 찾아야 하므로) 뺄지 말지는 ②의 Filter가 정한다.
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LOAFindTargetsInShape), false);
		World->OverlapMultiByObjectType(
			Overlaps,
			BoxCenter,
			BoxRotation,
			FCollisionObjectQueryParams(ObjectType),
			FCollisionShape::MakeBox(BoxHalfExtent),
			Params);

		// 목록에 원래 들어 있던 개수. "이번에 새로 찾은 수"를 계산하는 데 쓴다.
		// (범위 여러 개를 한 목록에 이어서 모을 때: 앞쪽 원 + 주변 원 등)
		const int32 NumBefore = OutTargets.Num();

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Target = Overlap.GetActor();

			// ② 중복 제외 (한 액터에 콜리전이 여러 개여도 한 번만) / 맞을 수 있나 (ASC 있음 + 죽지 않음) / 편
			if (!Target || OutTargets.Contains(Target)
				|| !ULOACombatLibrary::IsTargetable(Target)
				|| !ULOACombatLibrary::PassesFilter(FilterSource, Target, Filter))
			{
				continue;
			}

			// ② 모양 안인가 (바닥 모양 + 높이). 몸통 = 충돌 캡슐 (반지름, 반 높이)
			if (ULOACombatLibrary::IsBodyInShape(Shape, Center, Yaw,
				Target->GetActorLocation(), Target->GetSimpleCollisionRadius(), Target->GetSimpleCollisionHalfHeight()))
			{
				OutTargets.Add(Target);
			}
		}

		const int32 NumFound = OutTargets.Num() - NumBefore;

#if ENABLE_DRAW_DEBUG
		// 이 함수를 부른 월드(보통 서버)에 그린다.
		// - 스탠드얼론 / 리슨 서버 호스트 : 그 화면에 보인다.
		// - PIE 한 프로세스 실행 + 데디케이티드 서버 : 엔진이 같은 프로세스의 클라이언트 창에 대신 그려 준다. (에디터 전용 편의 기능)
		// - 실제 데디케이티드 서버(패키징) : 보이지 않는다.
		if (bShowRange)
		{
			ULOACombatLibrary::DrawDebugHitShape(World, Shape, Center, Yaw, NumFound > 0 ? FColor::Red : FColor::Green);
		}
#endif

		return NumFound;
	}
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
	return LOAShapeSearch::FindTargets(Source, Shape, GetShapeCenter(Source, Shape), Source->GetActorRotation().Yaw,
		Filter, ObjectType, OutTargets, bShowRange, Caster);
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
	// 모양을 만들어 공통 함수로 (높이는 FLOAHitShape 기본값)
	FLOAHitShape Shape;
	Shape.ShapeType = ELOAHitShapeType::Sphere;
	Shape.Radius = Radius;
	Shape.bHitByBody = bHitByBody;
	return LOAShapeSearch::FindTargets(Source, Shape, Center, 0.f, Filter, ObjectType, OutTargets, bShowRange, Caster);
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
	// 방향은 바닥 기준 (높이 무시). 방향이 없으면(위/아래만 향함) 판정할 수 없다.
	if (FVector(Direction.X, Direction.Y, 0.f).IsNearlyZero())
	{
		return 0;
	}

	FLOAHitShape Shape;
	Shape.ShapeType = ELOAHitShapeType::Fan;
	Shape.Radius = Radius;
	Shape.InnerRadius = InnerRadius;
	Shape.Angle = Angle;
	Shape.bHitByBody = bHitByBody;
	return LOAShapeSearch::FindTargets(Source, Shape, Apex, Direction.Rotation().Yaw, Filter, ObjectType, OutTargets, bShowRange, Caster);
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
	// 방향은 바닥 기준 (높이 무시). 방향이 없으면(위/아래만 향함) 판정할 수 없다.
	if (FVector(Direction.X, Direction.Y, 0.f).IsNearlyZero())
	{
		return 0;
	}

	FLOAHitShape Shape;
	Shape.ShapeType = ELOAHitShapeType::Box;
	Shape.Length = Length;
	Shape.Width = Width;
	Shape.bHitByBody = bHitByBody;
	return LOAShapeSearch::FindTargets(Source, Shape, Start, Direction.Rotation().Yaw, Filter, ObjectType, OutTargets, bShowRange, Caster);
}

bool ULOACombatLibrary::IsBodyInShape(const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw,
	const FVector& BodyLocation, float BodyRadius, float BodyHalfHeight)
{
	// 방향 (바닥 기준, 길이 1)
	const FVector Forward2D = FRotator(0.f, Yaw, 0.f).Vector().GetSafeNormal2D();

	// 몸통 기준이면 몸통 반지름·반 높이만큼 넓게, 발 기준이면 중심점만
	const float HitRadius = Shape.bHitByBody ? FMath::Max(BodyRadius, 0.f) : 0.f;
	const float HitHalfHeight = Shape.bHitByBody ? FMath::Max(BodyHalfHeight, 0.f) : 0.f;

	// ① 높이: 몸의 위아래가 "기준점 ± HalfHeight"에 걸치는가 (띄워진 플레이어도 이 안이면 맞음. 다른 층은 안 맞음)
	if (FMath::Abs(BodyLocation.Z - ShapeCenter.Z) > FMath::Max(Shape.HalfHeight, 0.f) + HitHalfHeight)
	{
		return false;
	}

	// ② 바닥 모양 (위에서 본 XY)

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

	// 바닥에서 조금 띄워 그린다. 기준점이 바닥(bSnapToGround)이면 선이 바닥과 겹쳐 지직거린다 (그림만. 판정은 그대로)
	constexpr float DrawLift = 10.f;
	const FVector DrawCenter = ShapeCenter + FVector(0.f, 0.f, DrawLift);

	switch (Shape.ShapeType)
	{
	case ELOAHitShapeType::Sphere:
		// 판정이 바닥 거리라 바닥 원으로 그린다.
		DrawDebugCircle(World, DrawCenter, Shape.Radius, 48, Color, false, 1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
		break;

	case ELOAHitShapeType::Fan:
		LOAFan::DrawDebugFan(World, DrawCenter, Forward2D, FMath::DegreesToRadians(FMath::Clamp(Shape.Angle, 1.f, 360.f) * 0.5f),
			Shape.Radius, FMath::Max(Shape.InnerRadius, 0.f), Color);
		break;

	case ELOAHitShapeType::Box:
		LOABox::DrawDebugBox2D(World, DrawCenter, Forward2D, Shape.Length, Shape.Width * 0.5f, Color);
		break;

	default:
		break;
	}

	// 높이 범위: 기준점에서 위아래 HalfHeight 세로선
	const FVector HeightOffset(0.f, 0.f, FMath::Max(Shape.HalfHeight, 0.f));
	DrawDebugLine(World, ShapeCenter - HeightOffset, ShapeCenter + HeightOffset, Color, false, 1.f, 0, 2.f);
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
	if (ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead))
	{
		return false;
	}

	// ③ 몸이 있는가. State.Body.Hidden = 하늘로 사라진 보스 등 (LOA Grant Tags 구간)
	return !ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Hidden);
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

	// 편이 다르면 적 (편 = IsPlayerSide의 임시 규칙)
	return IsPlayerSide(A) != IsPlayerSide(B);
}

bool ULOACombatLibrary::IsPlayerSide(const AActor* Actor)
{
	// [임시 규칙] PlayerState가 있는 Pawn = 플레이어 편
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->GetPlayerState() != nullptr;
}

float ULOACombatLibrary::GetFacingAngleTo(const FVector& From, const FVector& Forward, const FVector& Point)
{
	// 바닥 기준: From → Point 방향과 보는 방향 (높이 무시)
	const FVector ToPoint = (Point - From).GetSafeNormal2D();
	const FVector Facing = Forward.GetSafeNormal2D();
	if (ToPoint.IsNearlyZero() || Facing.IsNearlyZero())
	{
		return 0.f;	// 같은 자리 → 방향 없음
	}

	// 두 방향의 내적 = cos(사이 각) → 각도
	const float Dot = FMath::Clamp(FVector::DotProduct(Facing, ToPoint), -1.f, 1.f);
	return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

bool ULOACombatLibrary::GetFacingDotToAttacker(const FVector& TargetLocation, const FVector& TargetForward,
	const FVector& AttackerLocation, float& OutDot)
{
	OutDot = 0.f;

	// 바닥 기준: 맞은 쪽 → 때린 사람 방향과 맞은 쪽이 보는 방향 (높이 무시)
	const FVector ToAttacker = (AttackerLocation - TargetLocation).GetSafeNormal2D();
	const FVector Forward = TargetForward.GetSafeNormal2D();

	// 같은 자리에 겹쳐 서 있으면 방향을 알 수 없다 → 앞도 뒤도 아님
	if (ToAttacker.IsNearlyZero() || Forward.IsNearlyZero())
	{
		return false;
	}

	// 두 방향의 내적 = cos(사이 각). 1 = 정면, 0 = 옆, -1 = 등 뒤
	OutDot = FVector::DotProduct(Forward, ToAttacker);
	return true;
}

bool ULOACombatLibrary::IsHeadDot(float Dot, float HalfAngleDegrees)
{
	// 앞 반각 안이면 cos(반각) 이상
	//   예) 반각 45 → 기준 0.707: 정면 0° → 1.0 (헤드) / 30° → 0.87 (헤드) / 90° 옆 → 0 (아님) / 뒤 180° → -1 (아님)
	return Dot >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

bool ULOACombatLibrary::IsBackDot(float Dot, float HalfAngleDegrees)
{
	// 뒤 반각 안이면 -cos(반각) 이하 (뒤 = 앞 방향과 180°)
	//   예) 반각 45 → 기준 -0.707: 등 뒤 180° → -1 (백) / 뒤에서 30° 옆(150°) → -0.87 (백) / 90° 옆 → 0 (아님)
	return Dot <= -FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

bool ULOACombatLibrary::IsHeadAttack(const AActor* Target, const AActor* Attacker, float HalfAngleDegrees)
{
	// 지금 자리로 본다
	float Dot = 0.f;
	return Target && Attacker && Target != Attacker
		&& GetFacingDotToAttacker(Target->GetActorLocation(), Target->GetActorForwardVector(), Attacker->GetActorLocation(), Dot)
		&& IsHeadDot(Dot, HalfAngleDegrees);
}

bool ULOACombatLibrary::IsBackAttack(const AActor* Target, const AActor* Attacker, float HalfAngleDegrees)
{
	float Dot = 0.f;
	return Target && Attacker && Target != Attacker
		&& GetFacingDotToAttacker(Target->GetActorLocation(), Target->GetActorForwardVector(), Attacker->GetActorLocation(), Dot)
		&& IsBackDot(Dot, HalfAngleDegrees);
}

ELOAAttackSide ULOACombatLibrary::GetAttackSide(const FVector& AttackerLocation, const FVector& TargetLocation, float TargetYaw)
{
	// 맞은 쪽이 보는 방향 기준 때린 사람이 어디 서 있나
	float Dot = 0.f;
	if (!GetFacingDotToAttacker(TargetLocation, FRotator(0.f, TargetYaw, 0.f).Vector(), AttackerLocation, Dot))
	{
		return ELOAAttackSide::None;	// 겹쳐 서 있음
	}

	// 각도 (모든 대상 공통. 헤드·백·카운터가 같은 범위)
	const float HalfAngle = GetDefault<ULOACombatSettings>()->DirectionalAttackHalfAngle;
	if (IsHeadDot(Dot, HalfAngle))
	{
		return ELOAAttackSide::Front;
	}
	if (IsBackDot(Dot, HalfAngle))
	{
		return ELOAAttackSide::Back;
	}
	return ELOAAttackSide::Side;
}

ELOADirectionalAttack ULOACombatLibrary::GetDirectionalAttack(const FGameplayTagContainer& AttackTags, ELOAAttackSide AttackSide)
{
	// 이 공격의 속성 (Skill.Attribute.BackAttack / HeadAttack)과 공격 방향이 맞아야 보너스
	if (AttackSide == ELOAAttackSide::Back && AttackTags.HasTagExact(LOAGameplayTags::Skill_Attribute_BackAttack))
	{
		return ELOADirectionalAttack::Back;
	}
	if (AttackSide == ELOAAttackSide::Front && AttackTags.HasTagExact(LOAGameplayTags::Skill_Attribute_HeadAttack))
	{
		return ELOADirectionalAttack::Head;
	}
	return ELOADirectionalAttack::None;
}

ELOADirectionalAttack ULOACombatLibrary::GetDirectionalAttackOnWeakPoint(const FGameplayTagContainer& AttackTags)
{
	// 방향 검사만 통과시킨 GetDirectionalAttack (사용자 11-18: "방향을 검사하는 코드만 무조건 통과")
	if (AttackTags.HasTagExact(LOAGameplayTags::Skill_Attribute_BackAttack))
	{
		return ELOADirectionalAttack::Back;
	}
	if (AttackTags.HasTagExact(LOAGameplayTags::Skill_Attribute_HeadAttack))
	{
		return ELOADirectionalAttack::Head;
	}
	return ELOADirectionalAttack::None;
}

bool ULOACombatLibrary::IsWeakPointExposed(const AActor* Target)
{
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	return TargetASC && TargetASC->HasMatchingGameplayTag(LOAGameplayTags::State_Debuff_WeakPoint);
}

FActiveGameplayEffectHandle ULOACombatLibrary::ApplyStateTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& StateTags, float Duration)
{
	if (!ASC || StateTags.IsEmpty())
	{
		return FActiveGameplayEffectHandle();
	}

	// 시간이 있으면 TimedTags (스스로 사라짐), 없으면 InfiniteTags (지울 때까지). ULOAGameplayAbility::ApplyStateTagsToSelf와 같은 규칙
	const bool bTimed = Duration > 0.f;
	const TSubclassOf<UGameplayEffect> EffectClass = bTimed
		? TSubclassOf<UGameplayEffect>(ULOAGE_TimedTags::StaticClass())
		: TSubclassOf<UGameplayEffect>(ULOAGE_InfiniteTags::StaticClass());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		LOA_LOG(ASC->GetAvatarActor(), Error, TEXT("[StateTags] Failed to make state tag effect spec (%s)."), *StateTags.ToStringSimple());
		return FActiveGameplayEffectHandle();
	}

	if (bTimed)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, Duration);
	}
	SpecHandle.Data->DynamicGrantedTags.AppendTags(StateTags);
	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

ELOADirectionalAttack ULOACombatLibrary::GetDirectionalAttackFromContext(const FGameplayEffectContextHandle& EffectContext)
{
	const FLOAGameplayEffectContext* LOAContext = FLOAGameplayEffectContext::Get(EffectContext);
	return LOAContext ? LOAContext->GetAttackJudgeRecord().DirectionalAttack : ELOADirectionalAttack::None;
}

FLOAAttackJudgeRecord ULOACombatLibrary::GetAttackJudgeRecordFromContext(const FGameplayEffectContextHandle& EffectContext)
{
	const FLOAGameplayEffectContext* LOAContext = FLOAGameplayEffectContext::Get(EffectContext);
	return LOAContext ? LOAContext->GetAttackJudgeRecord() : FLOAAttackJudgeRecord();
}

FLOAAttackJudgeRecord ULOACombatLibrary::DecideAttackJudgeRecord(const FLOASkillModuleContext& Context, const FLOATargetHit& Target)
{
	FLOAAttackJudgeRecord Record;

	// 자리·시각: 판정한 쪽이 정해 줬으면 그것 (공격자 판정: 보고한 내 자리 + 내 화면에서 본 시각으로 되감은 보스 자리·방향·시각)
	if (Target.bHasDirectionBasis)
	{
		Record.AttackSide = GetAttackSide(Target.AttackerLocation, Target.TargetLocation, Target.TargetYaw);
		Record.ServerTime = Target.AttackJudgeServerTime;
	}
	// 아니면 지금 자리·지금 시각. 때린 사람 = 시전자 (장판·투사체로 때려도)
	else
	{
		const AActor* Attacker = Context.Caster;
		const AActor* TargetActor = Target.Target;
		if (Attacker && TargetActor && Attacker != TargetActor)
		{
			Record.AttackSide = GetAttackSide(Attacker->GetActorLocation(), TargetActor->GetActorLocation(), TargetActor->GetActorRotation().Yaw);
		}
		Record.ServerTime = LOANetTime::GetServerTimeNow(TargetActor ? TargetActor->GetWorld() : nullptr);
	}

	// 백 / 헤드 보너스 = 공격 방향 + 스킬 속성
	//   약점 공격(대난투 중 보스, 2026-11-18 apply124)이면 방향 검사를 건너뛴다. 공격 방향(AttackSide)은 실제 자리 그대로 둔다 (카운터가 읽음)
	//   예) 대난투 중 보스 옆에서 백 어택 스킬 → AttackSide = Side, DirectionalAttack = Back, bWeakPoint = true
	//       → 데미지 +백 보너스, 치명타 +백 어택 몫 (아래 RollCritical), 숫자 옆 "약점 공격"
	if (IsWeakPointExposed(Target.Target))
	{
		Record.DirectionalAttack = GetDirectionalAttackOnWeakPoint(Context.AttackTags);
		Record.bWeakPoint = (Record.DirectionalAttack != ELOADirectionalAttack::None);
	}
	else
	{
		Record.DirectionalAttack = GetDirectionalAttack(Context.AttackTags, Record.AttackSide);
	}

	// 치명타 (2026-11-13). 백 어택이면 확률이 오르므로 백 / 헤드 다음에
	Record.bCritical = RollCritical(Context, Target, Record.DirectionalAttack);
	return Record;
}

bool ULOACombatLibrary::RollCritical(const FLOASkillModuleContext& Context, const FLOATargetHit& Target, ELOADirectionalAttack DirectionalAttack)
{
	// 플레이어 편이 적을 때릴 때만 (보스는 치명타 개념 없음 / 아군 회복·보호막·버프는 치명타 없음. 사용자 2026-11-13)
	if (!IsPlayerSide(Context.Caster) || !IsEnemy(Context.Caster, Target.Target))
	{
		return false;
	}

	// 확률 = 기본 + 때린 쪽 추가분(Player 스탯) − 맞은 쪽 치명타 저항(Combat 스탯) + 백 어택 보너스
	//   예) 0.7 + 0 − (약점 노출로 −0.1) + 백 어택 0.1 = 0.9
	//   스탯창이 없으면 GetNumericAttribute가 0 → 그 몫은 없음 (예: 허수아비는 Combat이 없어 저항 0)
	const ULOACombatSettings* Settings = GetDefault<ULOACombatSettings>();
	float Chance = Settings->BaseCritRate;
	if (Context.SourceASC)
	{
		Chance += Context.SourceASC->GetNumericAttribute(ULOAAttributeSet_Player::GetExtraCritRateAttribute());
	}
	if (const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target.Target))
	{
		Chance -= TargetASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetCritResistanceAttribute());
	}
	if (DirectionalAttack == ELOADirectionalAttack::Back)
	{
		Chance += Settings->BackAttackCritRateBonus;
	}
	Chance = FMath::Clamp(Chance, 0.f, 1.f);

	// [0, 1) 무작위가 확률보다 작으면 치명타. 확률 1이면 언제나, 0이면 절대 안 남
	return FMath::FRand() < Chance;
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
	// 혼자 불렸으면 이 타격의 공격 판정 기록을 여기서 정한다 (ApplyPayloads는 대상마다 한 번 정해서 몸통을 바로 부른다)
	if (!Context.SourceASC || !Context.SourceASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	ApplyPayloadWithRecord(Context, Target, Payload, DecideAttackJudgeRecord(Context, Target));
}

void ULOACombatLibrary::ApplyPayloadWithRecord(
	const FLOASkillModuleContext& Context,
	const FLOATargetHit& Target,
	const FLOAEffectPayload& Payload,
	const FLOAAttackJudgeRecord& AttackJudgeRecord)
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

	//    보낸 스킬도 적는다 (2026-11-07). 받는 쪽이 "어느 스킬이 건 효과인가"를 알 수 있게. 예) 같은 스킬 보호막 덮어쓰기
	if (Context.SourceAbility)
	{
		EffectContext.SetAbility(Context.SourceAbility);
	}

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

	//    공격 판정 기록(공격 방향, 백 / 헤드, 시각, 치명타)을 우리 쪽지(FLOAGameplayEffectContext)에 적는다. 정하는 곳은 DecideAttackJudgeRecord 하나
	//    → 데미지 계산(보너스·치명타 배율)과 데미지 숫자(글자·노란 숫자)는 이 칸을 읽기만 한다.
	//    자리·시각 = Target의 "공격 판정 기록을 정할 자리·시각"(공격자 판정이 채움 = 내 화면에서 본 보스) / 없으면 지금 자리·지금 시각
	//    (2026-11-13) 기록은 받아서 적는다. 치명타가 무작위라 GE마다 다시 정하면 데미지 GE끼리 결과가 갈린다
	if (FLOAGameplayEffectContext* LOAContext = FLOAGameplayEffectContext::Get(EffectContext))
	{
		LOAContext->SetAttackJudgeRecord(AttackJudgeRecord);
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Payload.Effect, Context.Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	//    공격 속성(Skill.Attribute.*, 예: BackAttack)을 Spec 태그로도 붙인다 → GE 조건 칸(Source Tags 등)이 "백 어택 스킬인가"를 읽을 수 있다.
	//    (엔진 코드 확인: AppendDynamicAssetTags는 Spec의 Asset 태그 + 때린 쪽 캡처 태그 둘 다에 넣는다. GameplayEffect.cpp)
	if (!Context.AttackTags.IsEmpty())
	{
		SpecHandle.Data->AppendDynamicAssetTags(Context.AttackTags);
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

TSubclassOf<UGameplayEffect> ULOACombatLibrary::FindPayloadEffect(const TArray<FLOAEffectPayload>& Payloads, TSubclassOf<UGameplayEffect> EffectClass)
{
	if (!EffectClass)
	{
		return nullptr;
	}
	for (const FLOAEffectPayload& Payload : Payloads)
	{
		if (Payload.Effect && Payload.Effect->IsChildOf(EffectClass))
		{
			return Payload.Effect;
		}
	}
	return nullptr;
}

void ULOACombatLibrary::ApplyPayloads(
	const FLOASkillModuleContext& Context,
	const TArray<FLOATargetHit>& Targets,
	const TArray<FLOAEffectPayload>& Payloads)
{
	// 이 판정에 효과가 들어간 대상이 있었나 / 그중 적이 있었나 (아이덴티티 게이지 수급용, 2026-11-09)
	bool bAppliedAny = false;
	bool bHitEnemy = false;

	// 대상마다, Payload마다 한 번씩. (대상 3명 × Payload 2개 = GE 적용 6번)
	for (const FLOATargetHit& Target : Targets)
	{
		// 무적(State.Body.Invulnerable, 부활 직후 등)인 적에게는 아무것도 주지 않는다. 데미지·피격 이상·"맞았다" 신호 모두.
		// 모든 공격(스킬·장판·투사체·피해자 판정)이 여기를 지나가므로 한 곳에서 막는다. 아군의 힐·버프는 그대로 준다.
		if (IsInvulnerableTo(Context.Caster, Target.Target))
		{
			continue;
		}

		// 이 대상에 대한 공격 판정 기록은 한 번만 정한다 (치명타가 무작위라. 그 대상의 모든 GE와 "맞았다" 신호가 같은 기록, 2026-11-13)
		//   서버가 아니면 ApplyPayloadWithRecord·SendHitEvent가 아무것도 안 하므로 굴리지 않는다
		const FLOAAttackJudgeRecord AttackJudgeRecord = (Context.SourceASC && Context.SourceASC->IsOwnerActorAuthoritative())
			? DecideAttackJudgeRecord(Context, Target) : FLOAAttackJudgeRecord();

		for (const FLOAEffectPayload& Payload : Payloads)
		{
			ApplyPayloadWithRecord(Context, Target, Payload, AttackJudgeRecord);
		}

		bAppliedAny = true;
		bHitEnemy |= IsEnemy(Context.Caster, Target.Target);

		// 효과를 다 준 뒤 "맞았다"를 알린다. (이 데미지로 죽었으면 사망이 패턴을 취소해서 카운터 모듈이 받지 않는다)
		SendHitEvent(Context, Target, AttackJudgeRecord);
	}

	// 판정 1번 = 이 함수 1번. 보낸 스킬에게 알린다 (적 타격형 아이덴티티 수급: 맞은 적 수와 무관하게 1번)
	if (bAppliedAny)
	{
		if (ULOAGameplayAbility* SourceAbility = Cast<ULOAGameplayAbility>(Context.SourceAbility))
		{
			SourceAbility->NotifyPayloadsApplied(bHitEnemy);
		}
	}
}

bool ULOACombatLibrary::IsInvulnerableTo(const AActor* Attacker, const AActor* Target)
{
	if (!Target || !IsEnemy(Attacker, Target))
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	return TargetASC && TargetASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Invulnerable);
}

void ULOACombatLibrary::SendHitEvent(const FLOASkillModuleContext& Context, const FLOATargetHit& Target, const FLOAAttackJudgeRecord& AttackJudgeRecord)
{
	AActor* TargetActor = Target.Target;

	// 서버만. 적을 쳤을 때만 (아군 힐·버프는 "맞음"이 아니다)
	if (!TargetActor || !Context.SourceASC || !Context.SourceASC->IsOwnerActorAuthoritative() || !IsEnemy(Context.Caster, TargetActor))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		return;
	}

	// 무엇을 담나: 누가(시전자), 어떤 속성의 공격으로(예: { Skill.Attribute.Counter }). 그 뜻은 받는 쪽이 정한다.
	FGameplayEventData Payload;
	Payload.EventTag = LOAGameplayTags::Event_Combat_Hit;
	Payload.Instigator = Context.Caster;
	Payload.Target = TargetActor;
	Payload.InstigatorTags = Context.AttackTags;

	// 공격 판정 기록 (데미지 GE의 쪽지와 같은 기록, ApplyPayloads가 넘겨 줌). 신호의 쪽지 칸(ContextHandle)으로 보낸다 (2026-11-03)
	//   예) 카운터: 공격 방향이 Front인가 + 그 시각에 파란 구간이었나
	FGameplayEffectContextHandle EventContext = Context.SourceASC->MakeEffectContext();
	if (FLOAGameplayEffectContext* LOAContext = FLOAGameplayEffectContext::Get(EventContext))
	{
		LOAContext->SetAttackJudgeRecord(AttackJudgeRecord);
		Payload.ContextHandle = EventContext;
	}

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

bool ULOACombatLibrary::MakeShapeHitResult(AActor* Target, const FVector& ShapeCenter, FHitResult& OutHit)
{
	if (!Target)
	{
		return false;
	}

	// 기준점을 대상 몸 중심 높이로 올려서 가장 가까운 점 → 같은 방향의 몸 가운데 높이
	const FVector Reference(ShapeCenter.X, ShapeCenter.Y, Target->GetActorLocation().Z);
	if (!MakeClosestHitResult(Target, Reference, OutHit))
	{
		return false;
	}
	OutHit.TraceStart = ShapeCenter;
	return true;
}
