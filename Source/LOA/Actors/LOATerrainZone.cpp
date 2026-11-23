// LOATerrainZone.cpp

#include "Actors/LOATerrainZone.h"
#include "Game/LOARaidSubsystem.h"
#include "Combat/Telegraph/LOATelegraphSubsystem.h"
#include "Combat/Telegraph/LOATelegraphTypes.h"
#include "Core/LOANetTime.h"
#include "Core/LOALog.h"
#include "Core/LOACollisionChannels.h"

#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOATerrainZone)

ALOATerrainZone::ALOATerrainZone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 예고 중(무너질 시각 확인)과 떨어지는 동안만 켠다
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 상태 하나만 복제. 레벨에 놓인 액터라 모든 컴퓨터에 있고, 거리와 상관없이 항상 받게 한다
	bReplicates = true;
	bAlwaysRelevant = true;

	// 루트 = 경기장 중심. 바닥판(스태틱 메시)은 BP·레벨에서 이 밑에 붙인다
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	EditorArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EditorArrow"));
	if (EditorArrow)
	{
		EditorArrow->SetupAttachment(RootComponent);
		EditorArrow->ArrowSize = 5.f;
	}
#endif
}

void ALOATerrainZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALOATerrainZone, RepState);
}

#if WITH_EDITOR
EDataValidationResult ALOATerrainZone::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (InnerRadius >= OuterRadius)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("Inner Radius (%.0f) must be smaller than Outer Radius (%.0f)."), InnerRadius, OuterRadius)));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

void ALOATerrainZone::BeginPlay()
{
	Super::BeginPlay();

	// ① 바닥판 = 이 액터의 스태틱 메시 전부. 원래 모습과 떨어지는 순서를 기억한다
	//    순서: 바깥 둘레가 먼저. 예) 안쪽 800, 바깥 2000, Spread 0.4 → 거리 2000 판 0초, 1400 판 0.2초, 800 판 0.4초
	const float RingWidth = OuterRadius - InnerRadius;
	TInlineComponentArray<UStaticMeshComponent*> Meshes(this);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		FFloorPiece& Piece = FloorPieces.AddDefaulted_GetRef();
		Piece.Mesh = Mesh;
		Piece.RelativeTransform = Mesh->GetRelativeTransform();
		Piece.Collision = Mesh->GetCollisionEnabled();

		const float Distance = GetDistanceFromCenter2D(Mesh->GetComponentLocation());
		const float OuterToInner = RingWidth > 0.f ? FMath::Clamp((OuterRadius - Distance) / RingWidth, 0.f, 1.f) : 0.f;
		Piece.FallDelay = OuterToInner * PieceDelaySpread;

		// 기우는 축: 판 이름으로 정한다 → 모든 컴퓨터에서 같은 방향으로 기운다 (레벨에 놓인 이름은 모든 컴퓨터가 같음)
		FRandomStream Stream(static_cast<int32>(GetTypeHash(Mesh->GetFName())));
		Piece.TiltAxis = FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f).Vector();

		if (Mesh->Mobility != EComponentMobility::Movable)
		{
			LOA_LOG(this, Warning, TEXT("[Terrain] %s : floor piece %s is not Movable. It will lose collision but will not fall."),
				*GetNameSafe(this), *GetNameSafe(Mesh));
		}

		// 장판·판정 모양은 Ground 채널을 막는 바닥에만 붙는다 (Floor 프로필, LOACollisionChannels.h [Ground]).
		//   BP에 붙인 메시는 기본 BlockAllDynamic이라 안 막는다 → 이 판 위의 장판이 공중에 남는다 (11-14 더미 바닥판에서 겪음)
		if (Mesh->GetCollisionResponseToChannel(LOACollision::Ground) != ECR_Block)
		{
			LOA_LOG(this, Warning, TEXT("[Terrain] %s : floor piece %s does not block the Ground channel. Telegraphs above it will not snap down. Use the Floor collision preset."),
				*GetNameSafe(this), *GetNameSafe(Mesh));
		}
	}
	if (FloorPieces.IsEmpty())
	{
		LOA_LOG(this, Warning, TEXT("[Terrain] %s has no floor pieces (Static Mesh components)."), *GetNameSafe(this));
	}

	// ② 낙사 가능 벽은 무너지기 전까지 꺼 둔다 (각 컴퓨터. 레벨에 놓인 볼륨이라 모든 컴퓨터에 있다)
	for (AActor* Edge : EdgesToAdd)
	{
		if (IsValid(Edge))
		{
			Edge->SetActorEnableCollision(false);
		}
	}

	// ③ 상황판에 등록 (위치 표시와 같은 방식: 각 컴퓨터가 각자)
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->RegisterTerrainZone(this);
	}
}

void ALOATerrainZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->UnregisterTerrainZone(this);
	}

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// 서버
// =============================================================================

bool ALOATerrainZone::StartCollapse()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || RepState.State != ELOATerrainZoneState::Intact)
	{
		return false;
	}

	// ① 상태 = 예고 + 무너질 서버 시각 (복제 → 각 컴퓨터가 같은 시각에 무너뜨림)
	const double Now = LOANetTime::GetServerTimeNow(World);
	RepState.State = ELOATerrainZoneState::Warning;
	RepState.CollapseServerTime = Now + WarningSeconds;

	// ② 예고: 도넛 부채꼴 (꼭짓점 = 경기장 중심, 앞 방향으로 좌우 반씩)
	//    예) 안쪽 800, 바깥 2000, 각도 180, 앞 = 서쪽 → 서쪽 반쪽 고리
	if (TelegraphStyle)
	{
		FLOATelegraphData Data;
		Data.Style = TelegraphStyle;
		Data.Shape.ShapeType = ELOAHitShapeType::Fan;
		Data.Shape.Radius = OuterRadius;
		Data.Shape.InnerRadius = InnerRadius;
		Data.Shape.Angle = ArcAngle;
		Data.Location = GetActorLocation();	// 액터 = 경기장 중심, 바닥 윗면 높이 (헤더 [BP 설정])
		Data.Yaw = GetActorRotation().Yaw;
		TelegraphId = ULOATelegraphSubsystem::ShowToAll(this, Data, WarningSeconds);
	}
	else
	{
		LOA_LOG(this, Warning, TEXT("[Terrain] %s has no Telegraph Style. Collapsing without warning."), *GetNameSafe(this));
	}

	LOA_LOG(this, Log, TEXT("[Terrain] %s (%s) warning. Collapse at server time %.2f (in %.1fs)"),
		*GetNameSafe(this), *UEnum::GetValueAsString(Side), RepState.CollapseServerTime, WarningSeconds);

	HandleStateChanged();
	ForceNetUpdate();
	return true;
}

void ALOATerrainZone::ResetZone()
{
	if (!HasAuthority() || RepState.State == ELOATerrainZoneState::Intact)
	{
		return;
	}

	// 예고 중이었으면 예고도 지운다
	ULOATelegraphSubsystem::HideToAll(this, TelegraphId);
	TelegraphId = 0;

	RepState = FLOATerrainZoneRepState();
	LOA_LOG(this, Log, TEXT("[Terrain] %s (%s) restored."), *GetNameSafe(this), *UEnum::GetValueAsString(Side));

	HandleStateChanged();
	ForceNetUpdate();
}

// =============================================================================
// 모든 컴퓨터
// =============================================================================

float ALOATerrainZone::GetDistanceFromCenter2D(const FVector& Location) const
{
	return FVector::Dist2D(GetActorLocation(), Location);
}

bool ALOATerrainZone::IsInside(const FVector& Location) const
{
	// ① 가운데 원 밖인가 (바깥 반지름 너머 허공도 "안": 허공에 뜬 시체도 옮겨지게)
	if (GetDistanceFromCenter2D(Location) < InnerRadius)
	{
		return false;
	}

	// ② 이 반쪽 방향인가: 앞 방향과의 각도 ≤ 각도 ÷ 2
	//    예) 각도 180, 앞 = 서쪽 → 서쪽에서 좌우 90도 안 = 서쪽 반
	FVector ToLocation = Location - GetActorLocation();
	ToLocation.Z = 0.f;
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	if (!ToLocation.Normalize())
	{
		return false;
	}
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(ArcAngle * 0.5f));
	return FVector::DotProduct(Forward, ToLocation) >= CosHalfAngle;
}

FVector ALOATerrainZone::GetRecoverLocation(const FVector& Location) const
{
	// 중심 → 그 자리 방향으로 (가운데 원 반지름 − 여유). 높이는 그대로
	//   예) 안쪽 800, 여유 100, 중심에서 서쪽 1200 → 서쪽 700
	FVector Direction = Location - GetActorLocation();
	Direction.Z = 0.f;
	if (!Direction.Normalize())
	{
		Direction = GetActorForwardVector().GetSafeNormal2D();
	}

	FVector Result = GetActorLocation() + Direction * FMath::Max(InnerRadius - RecoverMargin, 0.f);
	Result.Z = Location.Z;
	return Result;
}

void ALOATerrainZone::OnRep_RepState()
{
	HandleStateChanged();
}

void ALOATerrainZone::HandleStateChanged()
{
	switch (RepState.State)
	{
	case ELOATerrainZoneState::Intact:
		ApplyRestore();
		SetActorTickEnabled(false);
		K2_OnRestored();
		break;

	case ELOATerrainZoneState::Warning:
		// 무너질 시각을 Tick에서 본다 (이미 지났으면 다음 Tick에 바로 무너짐)
		SetActorTickEnabled(true);
		K2_OnWarningStarted();
		break;

	case ELOATerrainZoneState::Collapsed:
		// 보통은 예고 시계로 이미 무너뜨렸다. 예고를 못 받고 바로 이 상태를 받았으면 지금 무너뜨린다
		ApplyCollapse();
		break;

	default:
		break;
	}
}

void ALOATerrainZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Now = GetZoneTimeNow();

	// ① 무너질 시각이 됐나 (예고 시계 기준. 서버는 서버 시각 그대로)
	if (!bCollapseApplied && RepState.State != ELOATerrainZoneState::Intact && Now >= RepState.CollapseServerTime)
	{
		ApplyCollapse();
	}

	// ② [서버] 무너짐 확정 → 복제 + 알림 (상황판이 구역 안 시체·보스를 옮김)
	if (HasAuthority() && bCollapseApplied && RepState.State == ELOATerrainZoneState::Warning)
	{
		RepState.State = ELOATerrainZoneState::Collapsed;
		TelegraphId = 0;	// 예고는 꽉 차서 스스로 사라졌다
		ForceNetUpdate();
		LOA_LOG(this, Log, TEXT("[Terrain] %s (%s) collapsed."), *GetNameSafe(this), *UEnum::GetValueAsString(Side));
		OnCollapsed.Broadcast(this);
	}

	// ③ 떨어지는 연출. 다 떨어지면 Tick을 끈다
	if (bCollapseApplied && UpdateFallingPieces(Now - CollapseAppliedTime))
	{
		SetActorTickEnabled(false);
	}
}

void ALOATerrainZone::ApplyCollapse()
{
	if (bCollapseApplied)
	{
		return;
	}
	bCollapseApplied = true;
	CollapseAppliedTime = GetZoneTimeNow();

	// ① 바닥이 없어진다 → 위에 있던 사람은 떨어진다 (판정. 연출보다 먼저)
	//    충돌을 끄면 엔진이 그 자리 내비도 다시 만든다 (Runtime Generation = Dynamic, 헤더 [내비])
	for (const FFloorPiece& Piece : FloorPieces)
	{
		if (UStaticMeshComponent* Mesh = Piece.Mesh.Get())
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// ② 그쪽 바깥 벽은 끄고, 낙사 가능 벽을 켠다 → 걷기·회피로는 못 들어가고 CC로 밀리면 떨어진다
	for (AActor* Wall : WallsToRemove)
	{
		if (IsValid(Wall))
		{
			Wall->SetActorEnableCollision(false);
		}
	}
	for (AActor* Edge : EdgesToAdd)
	{
		if (IsValid(Edge))
		{
			Edge->SetActorEnableCollision(true);
		}
	}

	SetActorTickEnabled(true);
	K2_OnCollapsed();
}

void ALOATerrainZone::ApplyRestore()
{
	bCollapseApplied = false;
	CollapseAppliedTime = 0.0;

	// 충돌이 돌아오면 엔진이 내비도 다시 만든다
	for (const FFloorPiece& Piece : FloorPieces)
	{
		if (UStaticMeshComponent* Mesh = Piece.Mesh.Get())
		{
			Mesh->SetRelativeTransform(Piece.RelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
			Mesh->SetVisibility(true);
			Mesh->SetCollisionEnabled(Piece.Collision);
		}
	}

	for (AActor* Wall : WallsToRemove)
	{
		if (IsValid(Wall))
		{
			Wall->SetActorEnableCollision(true);
		}
	}
	for (AActor* Edge : EdgesToAdd)
	{
		if (IsValid(Edge))
		{
			Edge->SetActorEnableCollision(false);
		}
	}
}

bool ALOATerrainZone::UpdateFallingPieces(double SecondsSinceCollapse)
{
	bool bAllDone = true;
	for (const FFloorPiece& Piece : FloorPieces)
	{
		UStaticMeshComponent* Mesh = Piece.Mesh.Get();
		if (!Mesh || !Mesh->IsVisible() || Mesh->Mobility != EComponentMobility::Movable)
		{
			continue;
		}

		const float FallTime = static_cast<float>(SecondsSinceCollapse) - Piece.FallDelay;
		if (FallTime <= 0.f)
		{
			bAllDone = false;	// 아직 차례가 안 옴
			continue;
		}
		if (FallTime >= FallDuration)
		{
			Mesh->SetVisibility(false);
			continue;
		}
		bAllDone = false;

		// 원래 자리에서 아래로 ½at², 판마다 다른 축으로 기울어짐
		//   예) 가속도 1960, 1초 → 980cm 아래, 기울기 25 × (1 / 1.5) ≈ 17도
		const USceneComponent* Parent = Mesh->GetAttachParent();
		const FTransform BaseWorld = Parent ? Piece.RelativeTransform * Parent->GetComponentTransform() : Piece.RelativeTransform;
		const float Drop = 0.5f * FallAcceleration * FallTime * FallTime;
		const float TiltRadians = FMath::DegreesToRadians(FallTiltDegrees * (FallTime / FallDuration));

		const FVector Location = BaseWorld.GetLocation() - FVector::UpVector * Drop;
		const FQuat Rotation = FQuat(Piece.TiltAxis, TiltRadians) * BaseWorld.GetRotation();
		Mesh->SetWorldLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	return bAllDone;
}

double ALOATerrainZone::GetZoneTimeNow() const
{
	if (const ULOATelegraphSubsystem* Telegraphs = ULOATelegraphSubsystem::Get(this))
	{
		return Telegraphs->GetTelegraphTimeNow();
	}
	return LOANetTime::GetServerTimeNow(GetWorld());
}
