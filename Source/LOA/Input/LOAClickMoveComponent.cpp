// LOAClickMoveComponent.cpp

#include "Input/LOAClickMoveComponent.h"
#include "Input/LOAInputConfig.h"
#include "Core/LOALog.h"
#include "Core/LOAGameplayTags.h"
#include "Library/LOACursorLibrary.h"
#include "Core/LOACollisionChannels.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/OverlapResult.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAClickMoveComponent)

namespace
{
	/** IMC 등록/해제 창구 */
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(const APlayerController* PC)
	{
		const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	}

	/** 각도(라디안)를 0 ~ 2π로 */
	double WrapAngle(double Angle)
	{
		Angle = FMath::Fmod(Angle, UE_DOUBLE_TWO_PI);
		return Angle < 0.0 ? Angle + UE_DOUBLE_TWO_PI : Angle;
	}

	/** 바닥 평면에서 점 P에 가장 가까운 직선 A→B 위의 곳. 0 = A, 1 = B */
	double ClosestRatioOnSegment2D(const FVector& A, const FVector& B, const FVector& P)
	{
		const double ABx = B.X - A.X, ABy = B.Y - A.Y;
		const double LengthSq = ABx * ABx + ABy * ABy;
		if (LengthSq < UE_KINDA_SMALL_NUMBER)
		{
			return 0.0;
		}
		return FMath::Clamp(((P.X - A.X) * ABx + (P.Y - A.Y) * ABy) / LengthSq, 0.0, 1.0);
	}
}

ULOAClickMoveComponent::ULOAClickMoveComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 경로를 따라가는 동안에만 Tick을 켠다. (정지 중에는 매 프레임 비용 0)
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// 로컬 전용 부품. 복제하지 않는다.
	SetIsReplicatedByDefault(false);
}

// =============================================================================
// 켜기 / 끄기
// =============================================================================

void ULOAClickMoveComponent::ActivateClickMove(UEnhancedInputComponent* InInputComponent, const ULOAInputConfig* InInputConfig)
{
	if (bActive)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();

	// 마우스가 있는 컴퓨터에서만 동작해야 한다.
	if (!PC || !PC->IsLocalController())
	{
		LOA_LOG(this, Warning, TEXT("ActivateClickMove called on non-local PlayerController. Ignored."));
		return;
	}

	if (!InInputComponent || !InInputConfig)
	{
		LOA_LOG(this, Error, TEXT("ActivateClickMove: InputComponent or InputConfig is null."));
		return;
	}

	// ① IMC 등록 (우클릭 → IA_ClickMove)
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(PC))
	{
		if (ClickMoveIMC)
		{
			Subsystem->AddMappingContext(ClickMoveIMC, ClickMoveIMCPriority);
		}
		else
		{
			LOA_LOG(this, Warning, TEXT("ClickMoveIMC is not set."));
		}
	}

	// ② InputConfig에서 태그로 입력 액션을 찾아 바인딩
	if (const UInputAction* ClickAction = InInputConfig->FindNativeInputActionForTag(ClickMoveInputTag))
	{
		// Started   : 누른 순간 1회
		// Triggered : 누르고 있는 동안 매 프레임 (간격 조절은 Input_ClickMoveHeld에서)
		// 바인딩 핸들을 저장해 두었다가 끌 때 정확히 이것들만 해제한다. (스킬 입력 바인딩은 건드리지 않음)
		BindingHandles.Add(InInputComponent->BindAction(ClickAction, ETriggerEvent::Started, this, &ThisClass::Input_ClickMoveStarted).GetHandle());
		BindingHandles.Add(InInputComponent->BindAction(ClickAction, ETriggerEvent::Triggered, this, &ThisClass::Input_ClickMoveHeld).GetHandle());
	}

	InputComponent = InInputComponent;
	bActive = true;

	LOA_LOG(this, Log, TEXT("Click move activated."));
}

void ULOAClickMoveComponent::DeactivateClickMove()
{
	if (!bActive)
	{
		return;
	}

	StopMove();

	if (UEnhancedInputComponent* IC = InputComponent.Get())
	{
		for (const uint32 Handle : BindingHandles)
		{
			IC->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(GetOwningPlayerController()))
	{
		if (ClickMoveIMC)
		{
			Subsystem->RemoveMappingContext(ClickMoveIMC);
		}
	}

	InputComponent.Reset();
	bActive = false;
}

void ULOAClickMoveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// PlayerController가 사라질 때 입력과 Tick이 남지 않도록 정리
	DeactivateClickMove();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// 입력
// =============================================================================

void ULOAClickMoveComponent::Input_ClickMoveStarted(const FInputActionValue& Value)
{
	LOA_LOG(this, Verbose, TEXT("[ClickMove] 0. Right click received."));

	// 새로 누름 → 지난번에 돈 쪽(왼쪽/오른쪽) 기억을 버린다
	DetourSides.Reset();

	// 누른 순간에는 간격과 상관없이 바로 경로를 만든다. (클릭 반응이 늦으면 조작감이 나빠짐)
	MoveToCursor();
}

void ULOAClickMoveComponent::Input_ClickMoveHeld(const FInputActionValue& Value)
{
	// 누르고 있는 동안 매 프레임 호출되지만, 매 프레임 길찾기를 하면 낭비이므로 간격을 둔다.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimeSeconds() - LastRepathTime >= HoldRepathInterval)
	{
		MoveToCursor();
	}
}

// =============================================================================
// 경로
// =============================================================================

bool ULOAClickMoveComponent::MoveToCursor()
{
	APawn* Pawn = GetControlledPawn();
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		LOA_LOG(this, Verbose, TEXT("[ClickMove] 1. FAIL: No controlled pawn."));
		return false;
	}

	// ⓪ 이동 금지 상태면 입력 무시 (스킬 시전 중, 기절 등)
	//    LastRepathTime을 갱신하지 않으므로, 우클릭을 누르고 있으면 금지가 풀리는 즉시 다시 경로를 만든다.
	if (IsMoveBlocked())
	{
		return false;
	}

	LastRepathTime = World->GetTimeSeconds();

	// ① 커서가 가리키는 위치 (트레이스 → 실패하면 가상 바닥 평면)
	FVector ClickLocation;
	if (!GetCursorGroundLocation(ClickLocation))
	{
		LOA_LOG(this, Verbose, TEXT("[ClickMove] 2. FAIL: Cursor location not found (trace and ground plane both failed)."));
		return false;
	}

	// ②③ 목표 보정 + 길찾기
	if (!BuildPathTo(ClickLocation))
	{
		return false;
	}

	// 따라가는 동안 다시 길찾기할 때 쓸 클릭 지점 (보스가 움직이면 길이 바뀐다)
	FollowClickLocation = ClickLocation;
	LastFollowRepathTime = World->GetTimeSeconds();

	// 따라가는 동안에만 Tick
	SetComponentTickEnabled(true);

	// 이동 시작 알림 (후딜 중인 Ability 등이 듣는다)
	OnMoveStarted.Broadcast();
	return true;
}

bool ULOAClickMoveComponent::BuildPathTo(const FVector& ClickLocation)
{
	APawn* Pawn = GetControlledPawn();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Pawn || !NavSys)
	{
		// 클라이언트에서 이 로그가 뜨면: 프로젝트 설정 > Navigation System > Allow Client Side Navigation 확인
		LOA_LOG(this, Warning, TEXT("NavigationSystem not found. Check 'Allow Client Side Navigation'."));
		return false;
	}

	// ①' 근처 몸 막이를 한 번만 찾아 (중심, 원 반지름) 목록으로. 이 길 만들기 안에서는 이 목록으로 계산만 한다
	//     (한 프레임 안이라 보스는 그대로다. 0.25초 뒤 다음 길 만들기에서 새로 찾는다 → 움직인 위치 반영)
	FBox SearchBox(ForceInit);
	SearchBox += Pawn->GetActorLocation();
	SearchBox += ClickLocation;
	TArray<FBodyBlockCircle> Circles;
	GatherBodyBlockCircles(Pawn, SearchBox, Circles);

	// ② 보스 몸을 클릭했으면 → 클릭한 곳에서 가장 가까운 보스 몸 바깥으로 (2026-10-20 사용자 결정)
	//    예) 보스(반지름 120) 중심에서 40cm 동쪽을 클릭 → 중심에서 동쪽으로 120 + 내 몸 34 + 여유 20 = 174cm 지점
	FVector Target = ClickLocation;
	PushOutOfBodyBlockers(Pawn, Circles, Target);

	// ③ 실제로 갈 수 있는 목표로 보정 (근처 내비메시 → 없으면 그 방향 내비메시 끝)
	FVector Goal;
	if (!FindReachableGoal(NavSys, Pawn, Target, Goal))
	{
		// 근처에도, 그 방향에도 갈 수 있는 곳이 없음 → 이번 클릭은 무시
		LOA_LOG(this, Verbose, TEXT("[ClickMove] 3. FAIL: No reachable NavMesh for click %s. Check NavMeshBoundsVolume (press P)."), *ClickLocation.ToString());
		return false;
	}

	// ④ 내 컴퓨터에서 직접 길찾기. 내비메시는 보스를 모른다 → 보스를 관통하는 직선이 나올 수 있다 (⑤에서 고침)
	UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, Pawn->GetActorLocation(), Goal, Pawn);
	if (!NavPath || !NavPath->IsValid() || NavPath->PathPoints.Num() < 2)
	{
		// 경로 없음 (막힌 곳 등). 이미 가던 길이 있으면 그대로 둔다.
		LOA_LOG(this, Verbose, TEXT("[ClickMove] 4. FAIL: Path not found. Is the character standing on the NavMesh?"));
		return false;
	}

	// 경로 지점 저장. [0]은 현재 위치이므로 [1]부터 따라간다.
	PathPoints = NavPath->PathPoints;
	CurrentPathIndex = 1;

	// 길이 내 위치·클릭 지점 상자를 100cm 넘게 벗어나면(벽을 크게 돌아가는 길 등) 길까지 감싸서 한 번 더 찾는다. 보통은 안 함
	//   100cm 안이면: 찾은 범위(여유 500)가 길에서 400cm 이상 남아 있다 → 길 옆의 보스는 이미 찾았다
	//   (옆으로만 본다. 높이는 찾는 상자에 이미 위아래 여유가 있다)
	const FBox PathBox(PathPoints);
	const FBox Searched = SearchBox.ExpandBy(FVector(100.f, 100.f, 0.f));
	const bool bPathInsideSearched =
		PathBox.Min.X >= Searched.Min.X && PathBox.Min.Y >= Searched.Min.Y &&
		PathBox.Max.X <= Searched.Max.X && PathBox.Max.Y <= Searched.Max.Y;
	if (!bPathInsideSearched)
	{
		GatherBodyBlockCircles(Pawn, SearchBox + PathBox, Circles);
	}

	// ⑤ 보스 몸을 지나가는 직선 → 몸을 감싸는 원을 따라 돌아가는 지점들로 바꾼다
	InsertBodyBlockDetours(Pawn, Circles, PathPoints);

	LOA_LOG(this, Verbose, TEXT("[ClickMove] 5. OK: Path found (%d points). Moving."), PathPoints.Num());
	return true;
}

void ULOAClickMoveComponent::GatherBodyBlockCircles(const APawn* Pawn, const FBox& Area, TArray<FBodyBlockCircle>& OutCircles) const
{
	OutCircles.Reset();

	const UWorld* World = GetWorld();
	if (!World || !Pawn || !Area.IsValid)
	{
		return;
	}

	// 상자 = 내 위치·클릭 지점(·경로)을 감싸고, 옆으로 Detour Search Margin, 위아래로 Body Block Search Half Height 여유
	//   예) 나 (0,0), 클릭 (1000,0), 여유 500 → X -500~1500, Y -500~500
	const FBox Box = Area.ExpandBy(FVector(DetourSearchMargin, DetourSearchMargin, BodyBlockSearchHalfHeight));

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LOAClickMoveBodyBlock), false, Pawn);
	World->OverlapMultiByObjectType(Overlaps, Box.GetCenter(), FQuat::Identity,
		FCollisionObjectQueryParams(LOACollision::BodyBlock), FCollisionShape::MakeBox(Box.GetExtent()), Params);

	const float MyRadius = Pawn->GetSimpleCollisionRadius();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UCapsuleComponent* Blocker = Cast<UCapsuleComponent>(Overlap.GetComponent());
		if (!Blocker)
		{
			continue;
		}

		FBodyBlockCircle& Circle = OutCircles.AddDefaulted_GetRef();
		Circle.Center = Blocker->GetComponentLocation();
		Circle.Radius = Blocker->GetScaledCapsuleRadius() + MyRadius + BodyBlockGoalMargin;
		Circle.HalfHeight = Blocker->GetScaledCapsuleHalfHeight();
		Circle.Blocker = Blocker;
	}
}

bool ULOAClickMoveComponent::IsNearCircleHeight(const FBodyBlockCircle& Circle, const FVector& Location) const
{
	// 보스 몸 위아래로 Body Block Search Half Height 안 (커서가 보스 몸 위쪽에 맞아도 / 다른 층의 보스는 무시)
	return FMath::Abs(Location.Z - Circle.Center.Z) <= Circle.HalfHeight + BodyBlockSearchHalfHeight;
}

void ULOAClickMoveComponent::PushOutOfBodyBlockers(const APawn* Pawn, const TArray<FBodyBlockCircle>& Circles, FVector& InOutLocation) const
{
	if (!Pawn)
	{
		return;
	}

	for (const FBodyBlockCircle& Circle : Circles)
	{
		if (!IsNearCircleHeight(Circle, InOutLocation))
		{
			continue;
		}

		FVector FromCenter = InOutLocation - Circle.Center;
		FromCenter.Z = 0.f;
		if (FromCenter.SizeSquared() >= FMath::Square(Circle.Radius))
		{
			continue;	// 이미 바깥
		}

		// 정확히 한가운데를 클릭했으면 방향이 없다 → 내가 있는 쪽 가장자리
		FVector Direction = FromCenter.GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = (Pawn->GetActorLocation() - Circle.Center).GetSafeNormal2D();
		}

		InOutLocation.X = Circle.Center.X + Direction.X * Circle.Radius;
		InOutLocation.Y = Circle.Center.Y + Direction.Y * Circle.Radius;
	}
}

void ULOAClickMoveComponent::InsertBodyBlockDetours(const APawn* Pawn, const TArray<FBodyBlockCircle>& Circles, TArray<FVector>& InOutPoints)
{
	if (!Pawn || Circles.IsEmpty() || InOutPoints.Num() < 2)
	{
		return;
	}

	// 경로 중간 지점이 몸 안이면 먼저 몸 밖으로 (보스가 벽 모서리 옆에 서 있는 경우 등). [0]은 내 위치라 그대로
	for (int32 Index = 1; Index < InOutPoints.Num(); ++Index)
	{
		PushOutOfBodyBlockers(Pawn, Circles, InOutPoints[Index]);
	}

	int32 Inserted = 0;
	int32 Index = 0;
	bool bAnyBothSidesBlocked = false;	// 디버그 선 색 (빨강 = 양쪽 다 막힘, C안이 필요한 경우)
	while (Index + 1 < InOutPoints.Num() && Inserted < MaxDetoursPerPath)
	{
		const FVector A = InOutPoints[Index];
		const FVector B = InOutPoints[Index + 1];

		const FBodyBlockCircle* Circle = FindFirstCircleOnSegment(Circles, A, B);
		if (!Circle)
		{
			++Index;
			continue;
		}
		const FVector Center = Circle->Center;
		const double Radius = Circle->Radius;
		const TWeakObjectPtr<const UPrimitiveComponent> Blocker = Circle->Blocker;

		// ---------- 원을 따라 도는 지점 만들기 ----------
		// 예) 원 반지름 254, 나는 중심에서 서쪽 600cm (각도 180도), 목표는 동쪽 600cm (각도 0도)
		//     나에서 원에 닿는 점(접점)은 내 각도에서 ±acos(254 / 600) ≈ ±65도 벌어진 곳
		const double DistA = FVector::Dist2D(A, Center);
		const double DistB = FVector::Dist2D(B, Center);
		const double AngleA = FMath::Atan2(A.Y - Center.Y, A.X - Center.X);
		const double AngleB = FMath::Atan2(B.Y - Center.Y, B.X - Center.X);

		// 원 안에 서 있으면(몸에 붙어 있음) 접점이 없다 → 0 (서 있는 곳에서 바로 바깥으로 나가며 돈다)
		const double TangentA = DistA > Radius ? FMath::Acos(Radius / DistA) : 0.0;
		const double TangentB = DistB > Radius ? FMath::Acos(Radius / DistB) : 0.0;

		// 도는 각도. +1 = 각도가 커지는 쪽 (가는 방향의 왼쪽으로 돎), -1 = 작아지는 쪽 (오른쪽으로 돎)
		//   예) 위 예에서 양쪽 다 180 - 65 - 65 = 50도
		const double ArcPlus = WrapAngle((AngleB - TangentB) - (AngleA + TangentA));
		const double ArcMinus = WrapAngle((AngleA - TangentA) - (AngleB + TangentB));

		// 짧은 쪽. 1cm 안으로 같으면 오른쪽
		int8 Side = (FMath::Abs(ArcPlus - ArcMinus) * Radius < 1.0) ? -1 : (ArcPlus < ArcMinus ? 1 : -1);

		// 지난번에 이 몸 막이를 돈 쪽이 있으면, 반대쪽이 Detour Side Switch Margin 이상 짧아질 때만 바꾼다
		FDetourSideMemory* Memory = DetourSides.FindByPredicate([&Blocker](const FDetourSideMemory& Entry) { return Entry.Blocker == Blocker; });
		if (Memory)
		{
			const double KeptArc = Memory->Side > 0 ? ArcPlus : ArcMinus;
			const double OtherArc = Memory->Side > 0 ? ArcMinus : ArcPlus;
			if ((KeptArc - OtherArc) * Radius < DetourSideSwitchMargin)
			{
				Side = Memory->Side;
			}
		}

		// 한쪽으로 도는 지점들 만들기
		auto MakeDetour = [&](int8 InSide)
		{
			const double Arc = InSide > 0 ? ArcPlus : ArcMinus;
			const double StartAngle = InSide > 0 ? AngleA + TangentA : AngleA - TangentA;
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Arc / FMath::DegreesToRadians(static_cast<double>(DetourMaxStepAngle))));
			const double Step = Arc / Steps;

			// 지점은 원보다 조금 바깥에 둔다 → 지점 사이 직선이 원에 딱 닿기만 하고 안으로 들어가지 않는다
			//   예) 원 254, 간격 25도 → 254 ÷ cos(12.5도) ≈ 260cm
			const double VertexRadius = Radius / FMath::Cos(Step * 0.5);

			TArray<FVector> Points;
			Points.Reserve(Steps + 1);
			for (int32 StepIndex = 0; StepIndex <= Steps; ++StepIndex)
			{
				const double Angle = StartAngle + InSide * Step * StepIndex;
				Points.Add(FVector(
					Center.X + VertexRadius * FMath::Cos(Angle),
					Center.Y + VertexRadius * FMath::Sin(Angle),
					FMath::Lerp(A.Z, B.Z, static_cast<double>(StepIndex) / Steps)));
			}
			return Points;
		};

		// ---------- 고른 쪽이 벽·구조물에 막혔으면 반대쪽 (2026-11-12 apply105) ----------
		// 원 계산은 보스만 알고 벽을 모른다 → 보스 옆에 상자가 있으면 그 틈으로 도는 길을 만들어 상자에 붙어 멈췄다.
		// 내비메시로 검사해서 막혔으면 반대쪽. 양쪽 다 막혔으면 지금처럼 고른 쪽 그대로 (C안 = 구조물을 크게 돌아가기는 나중)
		TArray<FVector> Detour = MakeDetour(Side);
		if (!IsDetourWalkable(A, Detour, B))
		{
			TArray<FVector> OtherDetour = MakeDetour(static_cast<int8>(-Side));
			if (IsDetourWalkable(A, OtherDetour, B))
			{
				Side = static_cast<int8>(-Side);
				Detour = MoveTemp(OtherDetour);
			}
			else
			{
				bAnyBothSidesBlocked = true;
				LOA_LOG(this, Verbose, TEXT("[ClickMove] Detour: both sides of %s are blocked by the level. Keeping side %d."),
					*GetNameSafe(Blocker.IsValid() ? Blocker->GetOwner() : nullptr), Side);
			}
		}

		// 이번에 실제로 돈 쪽을 기억 (막혀서 바꿨으면 바꾼 쪽)
		if (Memory)
		{
			Memory->Side = Side;
		}
		else
		{
			DetourSides.Add({ Blocker, Side });
		}

		InOutPoints.Insert(Detour, Index + 1);
		++Inserted;

		// Index는 그대로: A → 첫 지점 직선을 다른 몸 막이에 대해서도 다시 검사한다
	}

	if (bDrawDetourDebug)
	{
		if (const UWorld* World = GetWorld())
		{
			// 노랑 = 보통 / 빨강 = 어느 보스의 양쪽이 다 막혀서 막힌 쪽으로 감 (C안이 필요한 경우)
			const FVector Up(0.f, 0.f, 20.f);
			const FColor LineColor = bAnyBothSidesBlocked ? FColor::Red : FColor::Yellow;
			for (int32 Line = 0; Line + 1 < InOutPoints.Num(); ++Line)
			{
				DrawDebugLine(World, InOutPoints[Line] + Up, InOutPoints[Line + 1] + Up, LineColor, false, FollowRepathInterval + 0.05f, 0, 3.f);
			}
		}
	}
}

bool ULOAClickMoveComponent::IsDetourWalkable(const FVector& A, const TArray<FVector>& Detour, const FVector& B) const
{
	// 직선 A → 지점들 → B를 하나씩 내비메시 위에서 검사. 하나라도 내비메시가 끝나거나 막히면 false
	//   내비메시는 캐릭터 반지름만큼 벽에서 깎여 있다 → 통과 = 캐릭터가 지나갈 수 있음
	//   (FindReachableGoal과 같은 함수: 도중에 막히면 true를 돌려준다)
	UWorld* World = GetWorld();
	if (!World || Detour.IsEmpty())
	{
		return true;	// 검사할 수 없으면 지금까지처럼 그대로 쓴다
	}

	FVector HitLocation;
	FVector From = A;
	for (int32 PointIndex = 0; PointIndex <= Detour.Num(); ++PointIndex)
	{
		const FVector& To = Detour.IsValidIndex(PointIndex) ? Detour[PointIndex] : B;
		if (UNavigationSystemV1::NavigationRaycast(World, From, To, HitLocation, nullptr, GetOwningPlayerController()))
		{
			return false;
		}
		From = To;
	}
	return true;
}

const ULOAClickMoveComponent::FBodyBlockCircle* ULOAClickMoveComponent::FindFirstCircleOnSegment(const TArray<FBodyBlockCircle>& Circles, const FVector& A, const FVector& B) const
{
	if (FVector::DistSquared2D(A, B) < 1.0)
	{
		return nullptr;
	}

	const FBodyBlockCircle* Best = nullptr;
	double BestRatio = TNumericLimits<double>::Max();
	for (const FBodyBlockCircle& Circle : Circles)
	{
		// 가장 가까운 곳이 출발점 = 몸에서 멀어지는 중 (몸에 붙어 서서 바깥으로 감) → 돌 필요 없음
		const double Ratio = ClosestRatioOnSegment2D(A, B, Circle.Center);
		if (Ratio <= 0.0)
		{
			continue;
		}

		// 원을 안 지나감. 1cm 여유: 원에 딱 닿게 만든 돌아가는 직선을 다시 걸리게 하지 않으려고
		const FVector Closest = A + (B - A) * Ratio;
		if (!IsNearCircleHeight(Circle, Closest) || FVector::DistSquared2D(Closest, Circle.Center) >= FMath::Square(Circle.Radius - 1.0))
		{
			continue;
		}

		// 여럿이면 출발점에서 먼저 만나는 것
		if (Ratio < BestRatio)
		{
			BestRatio = Ratio;
			Best = &Circle;
		}
	}
	return Best;
}

bool ULOAClickMoveComponent::GetCursorGroundLocation(FVector& OutLocation) const
{
	// 스킬 조준과 같은 규칙: 트레이스 → 실패하면 캐릭터 발밑 높이의 가상 바닥 평면 (맵 밖 클릭 대응)
	FHitResult Hit;
	if (!ULOACursorLibrary::GetCursorHitResult(GetOwningPlayerController(), CursorTraceChannel, Hit))
	{
		return false;
	}

	OutLocation = Hit.Location;
	return true;
}

bool ULOAClickMoveComponent::FindReachableGoal(UNavigationSystemV1* NavSys, const APawn* Pawn, const FVector& ClickLocation, FVector& OutGoal) const
{
	// (가) 클릭 지점 근처에 내비메시가 있으면 그 위의 가장 가까운 점 (벽 안, 기둥 위를 클릭한 경우 등)
	FNavLocation NavGoal;
	if (NavSys->ProjectPointToNavigation(ClickLocation, NavGoal, NavProjectExtent))
	{
		OutGoal = NavGoal.Location;
		return true;
	}

	// (나) 근처에 없다(맵 밖, 절벽 너머) → 캐릭터에서 클릭 방향으로 직선을 그어 내비메시가 끝나는 곳을 찾는다.
	//      출발점도 내비메시 위여야 직선 검사가 정확하므로 캐릭터 위치를 먼저 내비메시로 내린다.
	FNavLocation NavStart;
	if (!NavSys->ProjectPointToNavigation(Pawn->GetActorLocation(), NavStart, NavProjectExtent))
	{
		return false;   // 캐릭터가 내비메시 밖에 서 있음 (이상 상황)
	}

	FVector RayEnd = ClickLocation;
	RayEnd.Z = NavStart.Location.Z;   // 같은 높이에서 수평으로 검사

	// 내비메시 위 직선 검사: 도중에 내비메시가 끝나거나 막히면 true + 그 지점
	FVector EdgeLocation;
	const bool bHitEdge = UNavigationSystemV1::NavigationRaycast(
		GetWorld(), NavStart.Location, RayEnd, EdgeLocation, nullptr, GetOwningPlayerController());
	if (!bHitEdge)
	{
		return false;   // 막힌 곳이 없는데 (가)가 실패 = 높이 차이가 큰 곳. 무시
	}

	// 가장자리에 딱 붙은 점은 경로 계산이 실패할 수 있어서 캐릭터 쪽으로 조금 당긴 뒤 다시 내비메시 위로 내린다.
	const FVector PullBack = (NavStart.Location - EdgeLocation).GetSafeNormal2D() * EdgePullBackDistance;
	if (!NavSys->ProjectPointToNavigation(EdgeLocation + PullBack, NavGoal, NavProjectExtent))
	{
		return false;
	}

	OutGoal = NavGoal.Location;
	return true;
}

void ULOAClickMoveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* Pawn = GetControlledPawn();

	// 캐릭터가 없어졌거나(사망 등) 경로가 끝났거나, 도중에 이동 금지가 됐으면(기절 등) 정지
	if (!Pawn || !PathPoints.IsValidIndex(CurrentPathIndex) || IsMoveBlocked())
	{
		StopMove();
		return;
	}

	// 따라가는 동안에도 일정 간격으로 길을 다시 찾는다 → 보스가 움직이면 돌아가는 길도 바뀐다
	//   (우클릭을 누르고 있으면 Input_ClickMoveHeld가 커서 기준으로 더 자주 하므로 그쪽에 맡김)
	//   실패하면(잠깐 길이 막힘 등) 가던 길 그대로
	if (const UWorld* World = GetWorld())
	{
		if (FollowRepathInterval > 0.f && World->GetTimeSeconds() - LastFollowRepathTime >= FollowRepathInterval)
		{
			LastFollowRepathTime = World->GetTimeSeconds();
			BuildPathTo(FollowClickLocation);
		}
	}

	if (!PathPoints.IsValidIndex(CurrentPathIndex))
	{
		StopMove();
		return;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();

	// ④ 현재 목표 지점에 충분히 가까우면 다음 지점으로 넘긴다. (높이 차이는 무시: 2D 거리)
	if (FVector::Dist2D(PawnLocation, PathPoints[CurrentPathIndex]) <= AcceptanceRadius)
	{
		++CurrentPathIndex;

		if (!PathPoints.IsValidIndex(CurrentPathIndex))
		{
			// 마지막 지점 도착
			StopMove();
			return;
		}
	}

	// 목표 지점 방향으로 이동 요청 → CharacterMovementComponent가 예측 이동 + 서버 동기화
	const FVector Direction = (PathPoints[CurrentPathIndex] - PawnLocation).GetSafeNormal2D();
	Pawn->AddMovementInput(Direction);
}

void ULOAClickMoveComponent::StopMove()
{
	DetourSides.Reset();
	PathPoints.Reset();
	CurrentPathIndex = 0;
	SetComponentTickEnabled(false);
}

// =============================================================================
// 도우미
// =============================================================================

APlayerController* ULOAClickMoveComponent::GetOwningPlayerController() const
{
	// 이 부품은 PlayerController에만 붙인다.
	return Cast<APlayerController>(GetOwner());
}

bool ULOAClickMoveComponent::IsMoveBlocked() const
{
	// ASC는 PlayerState에 있다. (클릭 이동 부품은 PlayerController에 붙어 있음)
	// 클라이언트에서 PlayerState가 아직 복제되지 않았으면 ASC가 없다 → 막지 않음
	const APlayerController* PC = GetOwningPlayerController();
	const UAbilitySystemComponent* ASC = PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->PlayerState) : nullptr;

	// HasMatchingGameplayTag: 자식 태그도 맞다고 본다. (예: State.Block.Move.Stun이 붙어 있어도 true)
	// 죽어 있어도 막는다. (가던 길도 Tick에서 멈춤 → 부활했을 때 예전 목적지로 걸어가지 않는다)
	// 상태 이상(스턴 등) 중에도 막는다 (2026-11-10). 스턴 GA가 띄움·사망에 끊겨도 스턴 GE가 남아 있는 동안은 못 걷는다
	return ASC && (ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Block_Move)
		|| ASC->HasMatchingGameplayTag(LOAGameplayTags::State_CrowdControl_Status)
		|| ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead));
}

APawn* ULOAClickMoveComponent::GetControlledPawn() const
{
	const APlayerController* PC = GetOwningPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}