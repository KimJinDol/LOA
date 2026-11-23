// LOATerrainZone.h
// [배관] 무너지는 지형 구역 하나. 레벨에 놓는다. (2026-11-14, 2페이즈 지형 파괴)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   경기장 = 가운데 원(안 무너짐) + 그것을 둘러싼 고리. 고리를 위·아래 선으로 나눈 왼쪽 반, 오른쪽 반이 구역 하나씩.
//   이 액터는 반쪽 고리 하나가 "멀쩡함 → 예고 → 무너짐"을 거치게 한다.
//   무너지면: 바닥판 충돌 끔 + 떨어지는 연출, 그쪽 바깥 벽(RaidWall) 끔, 낙사 가능 벽(RaidEdge) 켬.
//   위에 서 있던 사람은 바닥이 없어져 떨어진다 → KillZ → 낙사 (ALOACharacter::FellOutOfWorld).
//   내비(클릭 이동 길)는 따로 손대지 않는다: 바닥판 충돌을 끄면 엔진이 그 자리만 다시 만든다 (아래 [내비])
//   언제 무너질지는 모른다. 부르는 쪽(ULOARaidSubsystem::RequestTerrainCollapse ← 보스 StateTree, 치트)이 정한다.
//   누가 듣는지 모른다: 무너지는 순간 OnCollapsed를 울리기만 한다 (상황판이 듣고 시체·보스를 옮김).
//
// [모양 = 반쪽 고리] 액터를 경기장 중심에 놓고, 액터 앞(+X, 빨간 화살표)이 그 반쪽의 한가운데를 보게 돌린다.
//   (위에서 본 모습, 왼쪽 구역)        Inner Radius = 가운데 원 반지름, Outer Radius = 경기장 바깥 반지름
//           ╭───┬───╮                 Arc Angle = 180 (반쪽). 앞 방향 기준 왼쪽 90도 + 오른쪽 90도
//         ╱ 왼쪽 │ 오  ╲               예) 왼쪽 구역: 앞 = 경기장의 왼쪽(−Y), 오른쪽 구역: 반대로
//        │  ←●  ( 원 ) │
//         ╲      │    ╱
//           ╰───┴───╯
//   안인가 (IsInside): 중심에서 거리 ≥ Inner Radius 이고 앞 방향과의 각도 ≤ Arc Angle ÷ 2. 높이는 안 본다 (구역 = 바닥 기둥)
//     바깥 반지름 너머(허공)도 "안"으로 본다 → 허공에 뜬 시체도 옮겨진다
//   남은 땅 자리 (GetRecoverLocation): 사용자 결정 11-14 = 항상 가운데 원 쪽. 중심 → 그 자리 방향으로 Inner Radius − Recover Margin
//     예) 안쪽 800, 여유 100, 죽은 자리가 중심에서 서쪽 1200 → 중심에서 서쪽 700 (가운데 원 안). 높이는 그대로
//     가운데 원은 절대 안 무너지니 다시 옮길 일이 없다
//   예고 = 도넛 부채꼴 (판정 모양 Fan + InnerRadius, 꼭짓점 = 경기장 중심)
//
// [흐름] 예: 예고 3초, 지금 서버 시각 100.0
//   [서버] StartCollapse
//          → 상태 = 예고, 무너질 서버 시각 103.0 (복제)
//          → ULOATelegraphSubsystem::ShowToAll(도넛 부채꼴, 3초) → 모든 화면에 예고가 차오름
//   [각 컴퓨터] 예고 시계 ≥ 103.0 → ApplyCollapse: 충돌·벽을 바꾸고 바닥판이 떨어지기 시작
//               예고 시계 = 예고 매니저와 같은 시계 (서버 시각 + 내 편도 + 여유, 원격 클라만) → 예고가 꽉 차는 순간 = 바닥이 없어지는 순간
//   [서버] 103.0 → 상태 = 무너짐 (복제) → OnCollapsed → 상황판이 구역 안의 시체·보스를 가운데 원으로 옮김
//   [서버] 전멸 → ResetZone → 상태 = 멀쩡함 (복제) → 각 컴퓨터가 되돌림
//
// [왜 예고 시계로 무너뜨리나]
//   내 이동은 편도만큼 늦게 서버에 닿는다. 서버가 103.0에 바닥을 지우면, 그때 서버가 아는 내 위치 = 내 화면의 103.0 − 편도.
//   → 내 화면에서도 편도만큼 일찍 지워야 "내가 본 것 = 서버 판정"이 된다. 예고 장판과 같은 규칙 (ULOATelegraphSubsystem [예고 앞당김])
//   복제(상태 = 무너짐)를 기다리면 편도만큼 늦어서 서버에서는 이미 떨어졌는데 내 화면에서는 서 있다가 끌려 내려간다.
//
// [내비] Project Settings → Navigation Mesh → Runtime Generation = Dynamic (DefaultEngine.ini)
//   Static이면 내비가 실행 중에 안 바뀐다 → 무너진 뒤에도 구멍 위가 "갈 수 있는 곳" → 클릭 이동이 구멍을 가로질러 RaidEdge에 막힘
//   Nav Modifier Volume은 안 쓴다: 상자 모양이라 반쪽 고리를 덮으면 가운데 원까지 덮어 멀쩡한 바닥이 길에서 빠진다
//   엔진 확인 (PrimitiveComponentPhysics.cpp): SetCollisionEnabled → OnComponentCollisionSettingsChanged
//     → 쿼리 충돌이 꺼져 "내비에 영향 있음"이 바뀌면 FNavigationSystem::UpdateComponentData → Dynamic이면 그 자리만 다시 만든다
//   클라도 길찾기를 하므로 각 컴퓨터가 각자 (같은 순간에 같은 바닥판 충돌을 끄므로 결과도 같음)
//
// [바닥판 = 이 액터의 스태틱 메시 전부]
//   BP 또는 레벨에서 이 액터 밑에 스태틱 메시(더미: 기본 큐브)를 여러 개 붙여 반쪽 고리를 덮는다. 코드가 BeginPlay에서 전부 모은다.
//   떨어지는 순서: 바깥 둘레부터 안쪽으로 Piece Delay Spread만큼 늦게. 떨어진 뒤 Fall Duration이 지나면 숨김.
//   Mobility = Movable 이어야 떨어뜨릴 수 있다 (Static이면 경고 로그, 충돌만 꺼지고 그대로 보임).
//   Collision Presets = Floor 여야 장판·판정 모양이 이 판에 붙는다 (Ground 채널, 2026-11-14. 아니면 경고 로그)
//   예고 동안 바닥판을 흔들지 않는다: 충돌도 같이 움직이는데 컴퓨터마다 흔들린 양이 달라 위치 보정이 생긴다 → 흔들림은 이펙트로 (On Warning Started)
//   연출을 바꾸고 싶으면 (Chaos 파괴, 이펙트) BP 이벤트 On Collapsed에서. 판정은 여기 코드가 한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크] 세 규칙
//   1. 서버만   : 언제 시작하나(StartCollapse), 무너짐 확정, 시체·보스 옮기기(상황판), 되돌리기(ResetZone)
//   2. 복제     : RepState (상태 + 무너질 서버 시각). 레벨에 놓인 액터라 항상 관련(bAlwaysRelevant)
//                 예고는 ShowToAll이 따로 보낸다
//   3. 각자     : 바닥 충돌·벽 바꾸기(→ 내비), 떨어짐 연출. 모든 컴퓨터가 같은 시계로 같게 바꾼다 → 위치 보정 없음
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP 설정] BP_TerrainZone (부모 ALOATerrainZone)
//   액터 자리       : 경기장 중심, 높이 = 바닥 윗면 (예고가 액터 높이에 그려짐). 앞(빨간 화살표) = 그 반쪽의 한가운데
//   바닥판          : 스태틱 메시 컴포넌트 (기본 큐브 여러 개로 반쪽 고리 덮기). Mobility Movable, Collision Presets = Floor
//   LOA|Terrain     : Inner Radius, Outer Radius, Arc Angle(180), Telegraph Style, Warning Seconds, Recover Margin, 연출 숫자
//   레벨 인스턴스    : Side(왼/오), Walls To Remove(그쪽 바깥 RaidWall 볼륨),
//                     Edges To Add(가운데 원 둘레 + 위·아래 갈라지는 선을 따라 놓은 얇은 RaidEdge 볼륨들)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LOATerrainZone.generated.h"

class UArrowComponent;
class UStaticMeshComponent;
class ULOATelegraphStyle;
class ALOATerrainZone;

/** 경기장의 어느 쪽 구역인가 (치트·로그용) */
UENUM(BlueprintType)
enum class ELOATerrainSide : uint8
{
	Left	UMETA(DisplayName = "왼쪽"),
	Right	UMETA(DisplayName = "오른쪽"),
};

/** 구역 상태 */
UENUM(BlueprintType)
enum class ELOATerrainZoneState : uint8
{
	Intact		UMETA(DisplayName = "멀쩡함"),
	Warning		UMETA(DisplayName = "예고"),
	Collapsed	UMETA(DisplayName = "무너짐"),
};

/** 복제하는 상태 (상태와 시각이 같이 도착하게 하나로 묶음) */
USTRUCT()
struct FLOATerrainZoneRepState
{
	GENERATED_BODY()

	UPROPERTY()
	ELOATerrainZoneState State = ELOATerrainZoneState::Intact;

	/** 무너지는 서버 시각 (초). 예고·무너짐일 때만 의미 있음 */
	UPROPERTY()
	double CollapseServerTime = 0.0;
};

/** [서버] 구역이 무너졌다. 상황판이 듣는다 (구역 안 시체·보스 옮기기) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOATerrainZoneCollapsedNative, ALOATerrainZone* /*Zone*/);

UCLASS()
class LOA_API ALOATerrainZone : public AActor
{
	GENERATED_BODY()

public:
	ALOATerrainZone(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 서버 ----------

	/** [서버] 멀쩡하면 예고를 시작한다. Warning Seconds 뒤 무너진다. 시작했으면 true */
	bool StartCollapse();

	/** [서버] 멀쩡한 상태로 되돌린다 (전멸, 치트) */
	void ResetZone();

	/** [서버] 무너진 순간 한 번 */
	FLOATerrainZoneCollapsedNative OnCollapsed;

	// ---------- 모든 컴퓨터 ----------

	ELOATerrainZoneState GetZoneState() const { return RepState.State; }
	ELOATerrainSide GetSide() const { return Side; }

	/** 이 자리가 구역(반쪽 고리, 바깥 허공 포함) 안인가. 높이는 안 봄. 헤더 [모양] */
	bool IsInside(const FVector& Location) const;

	/** 구역 안 자리 → 가운데 원 안쪽 자리 (사용자 결정 11-14). 헤더 [모양] */
	FVector GetRecoverLocation(const FVector& Location) const;

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End AActor Interface

	/** [모든 컴퓨터] 예고가 시작됐다. 소리·흔들림 이펙트용 (바닥판은 움직이지 않는다: 헤더 [바닥판]) */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Terrain", meta = (DisplayName = "On Warning Started"))
	void K2_OnWarningStarted();

	/** [모든 컴퓨터] 무너졌다 (충돌이 없어지고 바닥판이 떨어지기 시작). 소리·이펙트·Chaos 파괴용 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Terrain", meta = (DisplayName = "On Collapsed"))
	void K2_OnCollapsed();

	/** [모든 컴퓨터] 멀쩡한 상태로 되돌아왔다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Terrain", meta = (DisplayName = "On Restored"))
	void K2_OnRestored();

#if WITH_EDITORONLY_DATA
	/** 에디터에서만 보이는 화살표 = 이 반쪽의 한가운데 방향 */
	UPROPERTY()
	TObjectPtr<UArrowComponent> EditorArrow;
#endif

	// ---------- 레벨에서 고르는 칸 ----------

	/** 경기장의 어느 쪽인가 (치트 LOATerrainCollapse Left / Right, 로그) */
	UPROPERTY(EditInstanceOnly, Category = "LOA|Terrain")
	ELOATerrainSide Side = ELOATerrainSide::Left;

	/** 무너지면 끌 벽 (그쪽 바깥 RaidWall 볼륨). 충돌만 끈다 */
	UPROPERTY(EditInstanceOnly, Category = "LOA|Terrain")
	TArray<TObjectPtr<AActor>> WallsToRemove;

	/** 무너지면 켤 벽 (가운데 원 둘레 + 위·아래 갈라지는 선의 RaidEdge 볼륨들 = CC로만 넘어가는 낙사 가능 벽). 시작할 때 충돌을 끈다 */
	UPROPERTY(EditInstanceOnly, Category = "LOA|Terrain")
	TArray<TObjectPtr<AActor>> EdgesToAdd;

	// ---------- 모양 (헤더 [모양]) ----------

	/** 가운데 원(안 무너짐) 반지름 (cm) */
	UPROPERTY(EditAnywhere, Category = "LOA|Terrain|Shape", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float InnerRadius = 800.f;

	/** 경기장 바깥 반지름 (cm). 예고 크기, 떨어지는 순서에 쓴다 */
	UPROPERTY(EditAnywhere, Category = "LOA|Terrain|Shape", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float OuterRadius = 2000.f;

	/** 이 구역의 전체 각도 (도). 반쪽 = 180. 앞 방향 기준 좌우로 반씩 */
	UPROPERTY(EditAnywhere, Category = "LOA|Terrain|Shape", meta = (ClampMin = "1.0", ClampMax = "360.0", Units = "Degrees"))
	float ArcAngle = 180.f;

	// ---------- BP 기본값 ----------

	/** 예고 겉모습 (도넛 부채꼴을 그릴 수 있는 Style). 비면 예고 없이 무너진다 (경고 로그) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Terrain")
	TObjectPtr<ULOATelegraphStyle> TelegraphStyle;

	/** 예고 시간 (초). 이만큼 뒤 무너진다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Terrain", meta = (ClampMin = "0.1", Units = "Seconds"))
	float WarningSeconds = 3.f;

	/** 가운데 원으로 옮길 때 원 둘레에서 더 들어가는 거리 (cm). 캡슐 반지름보다 크게 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Terrain", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float RecoverMargin = 100.f;

	/** 가장 바깥 판과 가장 안쪽 판이 떨어지기 시작하는 시간 차 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Terrain|Visual", meta = (ClampMin = "0.0", Units = "Seconds"))
	float PieceDelaySpread = 0.4f;

	/** 판 하나가 떨어지는 시간 (초). 지나면 숨긴다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Terrain|Visual", meta = (ClampMin = "0.1", Units = "Seconds"))
	float FallDuration = 1.5f;

	/** 떨어지는 가속도 (cm/s²) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Terrain|Visual", meta = (ClampMin = "0.0"))
	float FallAcceleration = 1960.f;

	/** 떨어지며 기우는 최대 각도 (도). 판마다 방향이 다르다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Terrain|Visual", meta = (ClampMin = "0.0", Units = "Degrees"))
	float FallTiltDegrees = 25.f;

private:
	/** 바닥판 하나의 원래 모습 (되돌리기용) */
	struct FFloorPiece
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		FTransform RelativeTransform;
		ECollisionEnabled::Type Collision = ECollisionEnabled::QueryAndPhysics;
		/** 떨어지기 시작하는 시간 (무너진 뒤 초) */
		float FallDelay = 0.f;
		/** 기우는 축 (판마다 다름, 이름으로 정해서 모든 컴퓨터가 같음) */
		FVector TiltAxis = FVector::ForwardVector;
	};

	UFUNCTION()
	void OnRep_RepState();

	/** [모든 컴퓨터] 상태가 바뀌었을 때 할 일 (서버는 바꾼 직후, 클라는 복제 도착) */
	void HandleStateChanged();

	/** [모든 컴퓨터] 충돌·벽을 무너진 상태로 + 떨어지기 시작. 한 번만 */
	void ApplyCollapse();

	/** [모든 컴퓨터] 멀쩡한 상태로 되돌린다 */
	void ApplyRestore();

	/** 예고 매니저와 같은 시계 (서버 = 서버 시각, 원격 클라 = 서버 시각 + 편도 + 여유) */
	double GetZoneTimeNow() const;

	/** [모든 컴퓨터] 무너진 뒤 SecondsSinceCollapse초의 바닥판 자리. 다 떨어졌으면 true */
	bool UpdateFallingPieces(double SecondsSinceCollapse);

	/** 중심에서 그 자리까지 바닥 거리 (cm) */
	float GetDistanceFromCenter2D(const FVector& Location) const;

	UPROPERTY(ReplicatedUsing = OnRep_RepState)
	FLOATerrainZoneRepState RepState;

	/** 바닥판들 (BeginPlay에서 모음) */
	TArray<FFloorPiece> FloorPieces;

	/** [서버] 예고 번호 (되돌릴 때 예고를 지움) */
	int32 TelegraphId = 0;

	/** [모든 컴퓨터] 이 컴퓨터에서 무너짐을 이미 적용했나 */
	bool bCollapseApplied = false;

	/** [모든 컴퓨터] 무너짐을 적용한 예고 시계 (떨어짐 연출 기준) */
	double CollapseAppliedTime = 0.0;
};
