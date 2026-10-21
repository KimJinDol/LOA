// LOAClickMoveComponent.h
// [배관] 우클릭 이동 부품. PlayerController에 붙어서 "클릭한 곳까지 길을 찾아 이동"을 담당한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [동작 흐름]
//
//   우클릭 ──[IMC]──▶ IA_ClickMove  (InputConfig: IA_ClickMove = InputTag.ClickMove)
//                        │
//      누른 순간(Started) : 바로 경로 계산
//      누르고 있는 동안(Triggered) : 커서를 따라가도록 일정 간격마다 경로 다시 계산
//                        │
//                        ▼
//   ① 클릭 지점 (ULOACursorLibrary::GetCursorHitResult: 트레이스 → 실패하면 캐릭터 발밑 높이의 가상 바닥 평면)
//   ② 갈 수 있는 목표로 보정 (FindReachableGoal)
//        (가) 클릭 지점 근처에 내비메시가 있으면 → 가장 가까운 내비메시 위 점 (벽 안을 클릭한 경우 등)
//        (나) 없으면(맵 밖, 절벽 너머) → 캐릭터에서 클릭 방향으로 직선을 그어 내비메시가 끝나는 지점
//            로아처럼 맵 끝 바깥을 우클릭해도 그쪽 끝까지 걸어간다.
//      (다) 보스 몸을 클릭했으면 → 클릭한 곳에서 가장 가까운 몸 바깥 (PushOutOfBodyBlockers, 2026-10-20)
//   ③ 내 컴퓨터에서 직접 길찾기 → 경로 지점 목록 (내비메시는 보스를 모른다 → 보스를 관통하는 직선이 나올 수 있음)
//   ③' 보스 돌아가기 (InsertBodyBlockDetours, 2026-10-20 B안): 경로의 각 직선이 몸 막이를 지나가면 몸을 감싸는 원을 따라 도는 지점을 끼워 넣는다
//      따라가는 동안에도 Follow Repath Interval(0.25초)마다 ②~③'을 다시 한다 → 보스가 움직이면 돌아가는 길도 바뀜
//   ④ 매 프레임: 다음 지점 방향으로 Pawn->AddMovementInput()
//      지점에 가까워지면 다음 지점으로, 마지막 지점이면 정지
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 엔진의 SimpleMoveToLocation을 쓰지 않나]
// 그 함수는 멀티플레이 클라이언트에서 동작하지 않고, 서버에 맡기면 핑만큼 늦게 출발한다.
// 대신 "경로 계산은 내 컴퓨터에서, 이동 요청은 AddMovementInput으로" 처리한다.
// → WASD와 똑같이 CharacterMovementComponent가 "내 화면에서 먼저 움직이고 서버가 확인"하는 예측 이동을 해 준다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [필요한 프로젝트 설정]
// - 프로젝트 설정 > Navigation System > Allow Client Side Navigation 켜기
//   (기본값은 길찾기 시스템이 서버에만 있음. 클라이언트에서 ③을 하려면 필요)
// - 맵에 NavMeshBoundsVolume 배치
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// 이 부품은 "이 캐릭터를 조종하는 사람의 컴퓨터"에서만 만들어지고 동작한다. 복제하지 않는다.
// 서버는 경로를 모른다. 서버가 받는 것은 CharacterMovementComponent가 보내는 이동 정보뿐이다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [이동 금지 - State.Block.Move 태그]
// 내 ASC에 State.Block.Move 태그가 붙어 있는 동안은 클릭 이동을 하지 않는다.
//   - 스킬 시전 중 : ULOAPlayerAbility가 bBlockMove(기본 켜짐)일 때 발동~종료 동안 태그를 붙임
//   - 기절 등     : 상태 이상 GE가 State.Block.Move를 붙임 (예정)
//
// 확인하는 곳 두 군데:
//   ① 경로를 새로 만들 때 (MoveToCursor) → 막혀 있으면 우클릭 입력 무시
//   ② 경로를 따라가는 중 (TickComponent) → 도중에 막히면 바로 정지 (이동 중 기절 등)
// 우클릭을 누르고 있던 경우, 태그가 떨어지면 다음 입력부터 다시 이동한다.
//
// 이벤트(태그가 붙는 순간 알림) 대신 Tick에서 확인하는 이유:
//   경로를 따라가는 동안은 어차피 Tick이 돌고, ASC(PlayerState) 준비 시점을 따로 기다릴 필요가 없어서 단순하다.
//
// [Ability와 연결]
// Ability 발동 시 정지는 ULOAPlayerAbility::PreActivate가 StopMove()를 불러서 처리한다.
// (시전 중 금지는 같은 곳에서 붙이는 State.Block.Move 태그로 처리)
//
// [이동 시작 알림 - OnMoveStarted]
// 새 경로로 이동을 시작할 때마다 알린다. 이 부품은 누가 듣는지 모른다. (Ability를 몰라도 됨)
//   예: 후딜 중인 스킬이 듣고 있다가 "이동하면 나는 끝" → 남은 몽타주를 끊고 정상 종료

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "LOAClickMoveComponent.generated.h"

class APlayerController;
class APawn;
class UEnhancedInputComponent;
class UInputMappingContext;
class ULOAInputConfig;
class UNavigationSystemV1;
class UPrimitiveComponent;
struct FInputActionValue;

/** 새 경로로 이동을 시작했다는 알림 */
DECLARE_MULTICAST_DELEGATE(FLOAMoveStartedSignature);

UCLASS(Blueprintable, ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOAClickMoveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOAClickMoveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 클릭 이동을 켠다. PlayerController가 입력 준비가 끝난 뒤 호출한다. [조종하는 사람의 컴퓨터]
	 *   - IMC 등록 (우클릭 → IA_ClickMove)
	 *   - InputConfig에서 ClickMoveInputTag로 입력 액션을 찾아 바인딩
	 */
	void ActivateClickMove(UEnhancedInputComponent* InInputComponent, const ULOAInputConfig* InInputConfig);

	/** 클릭 이동을 끈다. 이동 정지 + 바인딩 해제 + IMC 해제 */
	void DeactivateClickMove();

	/**
	 * 진행 중인 경로 이동을 멈춘다.
	 * 도착, 부품 비활성화 때 내부에서 호출되고, 나중에 Ability 발동/기절 시 외부에서도 호출한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|ClickMove")
	void StopMove();

	UFUNCTION(BlueprintPure, Category = "LOA|ClickMove")
	bool IsMoving() const { return PathPoints.Num() > 0; }

	/**
	 * 새 경로로 이동을 시작할 때마다 호출된다. [조종하는 사람의 컴퓨터]
	 * 우클릭을 누르고 있으면 경로를 다시 계산할 때마다 호출된다.
	 */
	FLOAMoveStartedSignature OnMoveStarted;

protected:
	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	// ---------- 입력 ----------
	/** 우클릭 누른 순간: 바로 경로 계산 */
	void Input_ClickMoveStarted(const FInputActionValue& Value);

	/** 우클릭 누르고 있는 동안: 일정 간격마다 경로 다시 계산 (커서 따라가기) */
	void Input_ClickMoveHeld(const FInputActionValue& Value);

	// ---------- 경로 ----------
	/** 커서 아래 지점을 구해서 그곳까지 경로를 새로 만든다. 성공하면 true */
	bool MoveToCursor();

	/**
	 * 클릭 지점까지 경로를 만든다 (목표 보정 + 길찾기). 성공하면 PathPoints를 바꾸고 true, 실패하면 가던 길 그대로.
	 * 클릭한 순간(MoveToCursor)과 따라가는 중 다시 길찾기(TickComponent) 둘 다 쓴다.
	 */
	bool BuildPathTo(const FVector& ClickLocation);

	/** 몸 막이 하나를 길 계산용으로 적어 둔 것. 길 만들기 한 번 동안만 쓴다 (한 프레임 안이라 보스는 그대로) */
	struct FBodyBlockCircle
	{
		FVector Center = FVector::ZeroVector;
		/** 돌아가는 원 반지름 = 몸 막이 반지름 + 내 몸 반지름 + Body Block Goal Margin */
		float Radius = 0.f;
		/** 몸 막이 반 높이 (다른 층인지 볼 때) */
		float HalfHeight = 0.f;
		TWeakObjectPtr<const UPrimitiveComponent> Blocker;
	};

	/**
	 * 영역(+여유) 안의 몸 막이를 한 번에 찾아 원 목록으로. 길 만들기마다 한 번 (경로가 영역을 크게 벗어나면 한 번 더)
	 * 그 뒤 몸 밖으로 밀기·돌아가기는 이 목록으로 계산만 한다 (2026-10-20 사용자 지적: 같은 보스를 여러 번 찾지 말 것)
	 */
	void GatherBodyBlockCircles(const APawn* Pawn, const FBox& Area, TArray<FBodyBlockCircle>& OutCircles) const;

	/** 이 위치가 그 몸 막이와 같은 층인가 (위아래 Body Block Search Half Height 안) */
	bool IsNearCircleHeight(const FBodyBlockCircle& Circle, const FVector& Location) const;

	/**
	 * 지점이 몸 막이 원 안이면, 그 지점에서 가장 가까운 원 바깥으로 옮긴다. (2026-10-20 사용자 결정)
	 * 클릭 지점(보스 몸을 클릭)과 경로 중간 지점에 쓴다
	 */
	void PushOutOfBodyBlockers(const APawn* Pawn, const TArray<FBodyBlockCircle>& Circles, FVector& InOutLocation) const;

	/**
	 * [B안, 2026-10-20] 경로의 직선이 보스·몬스터 몸 막이를 지나가면, 몸을 감싸는 원을 따라 도는 지점을 끼워 넣는다.
	 *   원 반지름 = 몸 막이 반지름 + 내 몸 반지름 + Body Block Goal Margin
	 *   예) 보스 200 + 나 34 + 여유 20 = 254cm. 나는 보스 서쪽 600cm, 목표는 동쪽 600cm
	 *     → 나에서 원에 닿는 점(접점)으로 → 원을 따라 30도 이하 간격으로 → 목표 쪽 접점 → 목표
	 *   왼쪽·오른쪽 중 짧은 쪽으로 돈다. 거의 같으면 지난번에 고른 쪽을 유지한다 (보스가 조금 움직일 때마다 좌우로 흔들리지 않게)
	 */
	void InsertBodyBlockDetours(const APawn* Pawn, const TArray<FBodyBlockCircle>& Circles, TArray<FVector>& InOutPoints);

	/** 직선 A→B가 지나가는 원 중 A에서 가장 먼저 만나는 것. 없으면 nullptr */
	const FBodyBlockCircle* FindFirstCircleOnSegment(const TArray<FBodyBlockCircle>& Circles, const FVector& A, const FVector& B) const;

	/** 커서가 가리키는 위치. (트레이스 → 실패하면 가상 바닥 평면) 못 찾으면 false */
	bool GetCursorGroundLocation(FVector& OutLocation) const;

	/**
	 * 클릭 지점 → 실제로 갈 수 있는 목표 지점. (위 ②의 (가), (나))
	 * 근처에도, 그 방향에도 갈 수 있는 곳이 없으면 false → 이번 클릭은 무시
	 */
	bool FindReachableGoal(UNavigationSystemV1* NavSys, const APawn* Pawn, const FVector& ClickLocation, FVector& OutGoal) const;

	/** 내 ASC에 State.Block.Move 태그가 붙어 있으면 true (스킬 시전 중, 기절 등) */
	bool IsMoveBlocked() const;

	/** 조종 중인 Pawn. 저장하지 않고 매번 가져온다. (사망 후 새 캐릭터로 바뀌어도 대응) */
	APawn* GetControlledPawn() const;
	APlayerController* GetOwningPlayerController() const;

	// ---------- 데이터 (BP 기본값에서 조정) ----------

	/** 클릭 이동 키 설정 (예: IMC_ClickMove = 마우스 우클릭 → IA_ClickMove) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Input")
	TObjectPtr<UInputMappingContext> ClickMoveIMC;

	/** IMC 우선순위. 같은 키를 여러 IMC가 쓸 때 숫자가 큰 쪽이 먼저 처리된다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Input")
	int32 ClickMoveIMCPriority = 0;

	/** InputConfig에서 클릭 이동 입력 액션을 찾을 때 쓰는 태그 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Input", meta = (Categories = "InputTag"))
	FGameplayTag ClickMoveInputTag;

	/** 커서 아래 바닥을 찾을 때 쓰는 트레이스 채널 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Trace")
	TEnumAsByte<ECollisionChannel> CursorTraceChannel = ECC_Visibility;

	/**
	 * 내비메시 밖을 클릭했을 때 "가장 가까운 갈 수 있는 점"을 찾는 탐색 범위(cm).
	 * 너무 작으면 벽 근처 클릭이 무시되고, 너무 크면 엉뚱한 층으로 보정될 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Path")
	FVector NavProjectExtent = FVector(100.f, 100.f, 200.f);

	/**
	 * 맵 밖을 클릭해서 내비메시 가장자리로 갈 때, 가장자리에서 캐릭터 쪽으로 당기는 거리(cm).
	 * 가장자리에 딱 붙은 점은 경로 계산이 실패할 수 있어서 조금 안쪽을 목표로 잡는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Path", meta = (ClampMin = "0.0"))
	float EdgePullBackDistance = 10.f;

	/** 따라가는 동안 길을 다시 찾는 간격(초). 보스가 움직이면 돌아가는 길도 따라 바뀐다. 0이면 다시 찾지 않음 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Path", meta = (ClampMin = "0.0"))
	float FollowRepathInterval = 0.25f;

	/** 보스 몸과 띄우는 여유 (cm). 몸을 클릭했을 때 옮기는 목표, 돌아가는 원 둘 다 (몸 막이 반지름 + 내 반지름 + 이 값) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Path", meta = (ClampMin = "0.0"))
	float BodyBlockGoalMargin = 20.f;

	/** 몸 막이를 찾을 때 위아래 여유 (cm). 커서가 보스 몸 위쪽에 맞아도 찾게, 다른 층의 보스는 빼게 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Path", meta = (ClampMin = "1.0"))
	float BodyBlockSearchHalfHeight = 500.f;

	/** 원을 따라 돌 때 지점 사이 최대 각도 (도). 작을수록 둥글게 돈다. 30도면 반원에 지점 7개 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Detour", meta = (ClampMin = "5.0", ClampMax = "90.0", Units = "Degrees"))
	float DetourMaxStepAngle = 30.f;

	/** 몸 막이를 찾는 상자의 옆 여유 (cm). 내 위치·클릭 지점을 감싼 상자를 이만큼 넓혀서 한 번에 찾는다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Detour", meta = (ClampMin = "0.0", Units = "cm"))
	float DetourSearchMargin = 500.f;

	/**
	 * 지난번에 고른 쪽(왼쪽/오른쪽)을 유지하는 여유 (cm). 반대쪽이 이만큼 이상 짧아져야 바꾼다.
	 * 예) 보스 정반대편을 클릭 → 양쪽 거리가 거의 같음 → 보스가 조금만 움직여도 짧은 쪽이 바뀜 → 이 값이 없으면 좌우로 흔들림
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Detour", meta = (ClampMin = "0.0", Units = "cm"))
	float DetourSideSwitchMargin = 100.f;

	/** 한 번 길을 만들 때 돌아가기를 끼워 넣는 최대 횟수 (몸 막이가 여럿일 때 끝없이 넣지 않게) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Detour", meta = (ClampMin = "1"))
	int32 MaxDetoursPerPath = 4;

	/** 켜면 돌아가는 길을 화면에 그린다 (노란 선, 다시 길찾기 간격만큼 남음). 테스트용 */
	UPROPERTY(EditAnywhere, Category = "LOA|ClickMove|Detour")
	bool bDrawDetourDebug = false;

	/** 경로 지점에 이 거리(cm) 안으로 들어오면 도착으로 본다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Path", meta = (ClampMin = "1.0"))
	float AcceptanceRadius = 30.f;

	/** 우클릭을 누르고 있을 때 경로를 다시 계산하는 간격(초). 작을수록 반응이 빠르지만 계산이 늘어난다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|ClickMove|Path", meta = (ClampMin = "0.0"))
	float HoldRepathInterval = 0.1f;

	// ---------- 런타임 상태 ----------
	TWeakObjectPtr<UEnhancedInputComponent> InputComponent;
	TArray<uint32> BindingHandles;
	bool bActive = false;

	/** 따라갈 경로 지점 목록 (비어 있으면 정지 상태) */
	TArray<FVector> PathPoints;

	/** 지금 향하고 있는 지점 번호 */
	int32 CurrentPathIndex = 0;

	/** 마지막으로 경로를 계산한 시각 (누르고 있을 때 간격 조절용) */
	double LastRepathTime = 0.0;

	/** 지금 따라가는 클릭 지점 (보정 전). 따라가는 중 다시 길찾기에 쓴다 */
	FVector FollowClickLocation = FVector::ZeroVector;

	/** 따라가는 중 마지막으로 다시 길을 찾은 시각 */
	double LastFollowRepathTime = 0.0;

	/** 몸 막이마다 지난번에 돈 쪽 (+1 / -1). 새로 우클릭하거나 멈추면 비운다 */
	struct FDetourSideMemory
	{
		TWeakObjectPtr<const UPrimitiveComponent> Blocker;
		int8 Side = 0;
	};
	TArray<FDetourSideMemory> DetourSides;
};