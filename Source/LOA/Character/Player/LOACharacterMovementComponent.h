// LOACharacterMovementComponent.h
// [배관] 플레이어 이동 컴포넌트. 엔진 캐릭터 이동에 세 가지를 더한다: 보스 몸 막이에서 빠져나오기 / CC로 밀리기·날아가기(정해진 궤적) / 벽을 따라 미끄러지는 건 클릭 이동일 때만
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가] 로아: 보스가 밀고 오면 플레이어가 슥 밀려나고, 겹쳐도 빠져나가는 방향으로 움직이면 잘 나가진다.
//   몸 막이(ULOABodyBlockerComponent)는 보스를 따라 순간이동하듯 움직여서 플레이어 몸 안으로 파고든다.
//   엔진의 "겹침 빼내기"는 한 번에 빼는 거리에 한도가 있어서 (Pawn 기준 100cm)
//     예) 보스(반지름 120)가 내(반지름 40) 한가운데로 착지 → 160cm 겹침 → 엔진이 포기
//   그런데 몸 막이는 계속 나를 막는다 → 안에 갇혀 클릭 이동이 전부 무시됐다.
//
// [규칙]
//   밖에 있음   : 몸 막이에 막힘 (지금까지와 같음. 걸어가면 보스 옆으로 미끄러짐)
//   겹쳐 있음   : 막지 않는다 (내가 걸어서 나갈 수 있음) + 보스 중심 반대쪽으로 Push Out Speed만큼 밀어낸다 (0이면 안 밈)
//   빠져나옴    : 다시 막힘
//   관통 중     : 막지도 밀지도 않는다 (내 ASC에 State.Body.PassThroughBodyBlock 태그가 있을 때. 예: 베스티지)
//   예) Push Out Speed 600, 160cm 겹침 → 약 0.27초 동안 슥 밀려 나감. 그 사이 옆으로 걸으면 그 방향도 더해짐.
//   (2026-11-13) 로아는 보스가 플레이어를 통과하고 플레이어는 밀리지 않는다 → BP_LOACharacter에서 Push Out Speed를 줄여 씀 (사용자).
//     밀어내기는 늘 같은 속도라 보스 걸음과 안 맞으면 "밀림 ↔ 막힘"이 번갈아 덜컥였다 (ARCHITECTURE 17장 "보스 몸 막이 경계").
//     겹친 동안 막지 않는 것(위 둘째 줄)은 그대로 둔다: 보스가 내 한가운데로 착지해도 걸어서 나갈 수 있게 (10-15 갇힘)
//
//   "겹쳐 있음"은 2cm(Overlap Tolerance)보다 깊게 겹쳤을 때. 걸어가 막혀서 맞닿아 있는 건 겹침이 아니다.
//
// [어디서 하나] 이동 계산 안 (UpdateCharacterStateBeforeMovement: 매 이동 계산 직전에 엔진이 부름)
//   플레이어 이동은 내 컴퓨터가 먼저 계산(예측)하고 서버가 같은 입력으로 다시 계산해 확인한다.
//   밀어내기도 그 계산 안에 있어야 두 쪽 결과가 같아진다. 캐릭터 Tick 등 바깥에서 위치를 바꾸면 서버가 되돌린다(덜컥).
//
// [레이드 가장자리 벽] (2026-11-01) CC 궤적이 걸려 있는 동안만 내 캡슐의 RaidEdge 응답을 무시로 → CC로 밀리면 가장자리를 넘어 낙사
//   걷기·회피·Move 모듈은 그대로 막힌다. 일반 벽(RaidWall)은 CC도 막는다. (LOACollisionChannels.h [RaidWall / RaidEdge])
//   몸 막이와 같은 자리(UpdateCharacterStateBeforeMovement)에서 바꾼다 → 서버와 내 컴퓨터가 같은 이동 계산 안에서 같은 응답
//   예) 경직 300cm, 가장자리 벽까지 100cm → 벽을 지나 밀려나고, 궤적이 끝나면 다시 막힘 (이미 벽 밖이라 낙하 → 낙사)
//
// [충돌 설정은 여기 한 곳만] 내 캡슐의 BodyBlock·RaidEdge 응답(막힘/무시)은 이 컴포넌트만 바꾼다.
//   관통은 태그로만 안다: Move 모듈(Pass Through Body Blockers)이 이동하는 동안 State.PassThroughBodyBlock을 붙인다.
//   → Move 모듈은 이 클래스를 모른다. 버프(GE)로 태그를 줘도 관통된다.
//   ※ 알려진 한계: "관통 중"이 이동 기록(Saved Move)에 들어가지 않는다. 핑·패킷 손실이 있을 때 보스에 붙어 대시하면
//     서버가 첫 몇 프레임을 "막힘"으로 계산해 위치를 되돌릴 수 있다. 관통 스킬을 만들 때 고친다 (ARCHITECTURE.md 13장).
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만 : 없음
//   2. 복제   : 결과 위치 (원래의 캐릭터 이동 복제)
//   3. 각자   : 겹침 확인과 밀어내기를 내 컴퓨터(예측)와 서버(확인)가 같은 규칙으로.
//               다른 플레이어 화면의 내 캐릭터는 복제된 위치를 볼 뿐 이 계산을 하지 않는다.
//   한계: 클라가 보는 보스 위치는 핑만큼 늦다 → 밀리는 방향·시점이 서버와 아주 조금 다를 수 있다. 엔진이 작게 맞춘다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [CC로 밀리기·날아가기] (2026-10-21 바꿈: 물리(LaunchCharacter + 미끄러짐) → 정해진 궤적)
//   띄움(ULOAGA_Airborne)·경직(ULOAGA_Paralysis)이 서버에서 StartCrowdControlMove를 부른다
//     → 루트 모션 소스 FLOARootMotionSource_Arc: "T초 동안 정확히 이 길로" (옆은 일정한 속도, 높이는 포물선)
//     → 끝나면 속도 0 (미끄러짐 없음). 땅에 닿거나 시간이 다 되면 끝. 낭떠러지 밖이면 거기서부터 보통 낙하
//   예) 경직 100cm / 0.15초 → 0.15초 동안 일정하게 100cm, 바로 멈춤
//       띄움 300cm / 높이 250 / 1.0초 → 0.5초에 꼭대기, 1.0초에 착지
//   때린 쪽 바라보기: 궤적을 거는 순간 밀리는 방향의 반대로 돈다 (서버 + 내 컴퓨터 각자)
//
//   [왜 숫자를 복제하나] 공격마다 다른 거리는 Payload의 SetByCaller에 있는데, SetByCaller 값은 맞은 사람 컴퓨터로 복제되지 않는다 (GAS 한계).
//     그래서 서버가 숫자를 정해 CrowdControlMove(복제 칸, 맞은 사람에게만)에 넣고, 서버와 맞은 사람 컴퓨터가 같은 숫자로 각자 궤적을 건다.
//     대가: 내 컴퓨터는 편도만큼 늦게 시작한다 (나중에: 피해자 판정·예고 장판은 클라가 먼저 반응)
//
//   [내 자리에서 출발, 서버와 같은 자리에 도착] (2026-10-31)
//     문제: 편도 50만 돼도 경직이 툭 끊겨 보였다. 숫자가 편도 늦게 오는 동안 클라는 계속 걸었고(서버는 이미 멈춤),
//       받는 순간 "서버가 건 시작 위치 + 늦은 만큼 진행된 지점"으로 순간이동했다 (전 방식, 2026-10-23 [늦게 받은 만큼 중간부터]).
//     고침: 서버와 클라의 위치 차이를 "밀리는 동안" 조금씩 메운다
//       서버            : 궤적을 서버 위치에서 건다 (서버가 끝까지 정답). 시작 위치(StartLocation)를 같이 복제
//       맞은 사람 컴퓨터 : 받은 순간 지금 내 자리에서 출발하고, 도착 지점은 서버 도착 지점(서버 시작 위치 + 방향 × 거리)으로 궤적을 건다
//                         → 밀리는 동안 차이가 메워지고, 둘 다 같은 자리에 착지 (높이·시간은 서버 숫자 그대로)
//       서버는 유예 시간 동안 보정을 보내지 않는다 (클라 궤적이 편도 늦고 도착점으로 휘어 있어 중간 위치가 다르다)
//         유예 시간 = 궤적 시간 + 그 사람 핑 + Crowd Control Trust Extra Time
//         유예가 끝나면 엔진 기본 검사로 돌아간다. 이미 같은 자리라 걸릴 게 없다 (끝 차이를 로그로 남긴다)
//       시작 차이 > Crowd Control Flight Limit (핑 사이에 회피·순간이동): 메우기엔 너무 크다
//         → 클라는 서버 시작 위치로 한 번 옮긴 뒤 같은 궤적 / 서버도 유예 중 차이가 이만큼 넘으면 바로 보정
//     예) 편도 0.1 걷다 경직 100cm/0.15초: 클라는 서버보다 약 120cm 앞에서 받음
//         → 내 화면: 0.15초 동안 "밀림 100 + 되돌아감 120"을 한 번에 (조금 세게 밀리는 느낌) / 남의 화면: 서버 궤적 그대로
//     전 시도 (같은 날): ① 클라 위치를 매 이동마다 받아들임 → 띄움 위아래 차이로 한도 계산이 꼬임
//       ② 끝에서 클라 위치를 한 번 받아들임 → 남의 화면에서 CC 끝에 그만큼 툭 옮겨짐
//       → 차이를 "누가, 언제" 떠안나의 문제. 원래 휙 밀리는 맞은 사람 화면이, 밀리는 동안 떠안는 게 가장 티가 안 난다
//     서버가 클라 위치를 믿지 않으므로 보안 한도가 필요 없다 (서버 궤적이 정답)
//     쓰는 곳: 모든 CC. 나중에 피해자 판정·예고 장판의 예측 CC(클라가 먼저 시작)도 같은 방식으로 도착점을 맞춘다
//
// ─────────────────────────────────────────────────────────────────────────────
// [벽을 따라 미끄러지는 건 클릭 이동일 때만] (2026-10-20 사용자: 로아엔 비스듬히 빠져나가는 개념이 없다)
//   엔진은 벽·보스 몸에 비스듬히 닿으면 남은 이동을 표면을 따라 돌려서 마저 간다 (SlideAlongSurface).
//     예) 보스 중심에서 30cm 옆을 겨냥해 500cm 회피 → 몸 막이(둥근 캡슐)를 타고 돌아 보스 옆으로 빠져나감
//   규칙:
//     클릭 이동 중 (입력 가속도 있음, 루트 모션 아님)  → 엔진 그대로 (벽을 따라 걸어감)
//     그 외 (회피·기상기·돌진·띄움·경직 = 루트 모션) + 벽처럼 선 면 → 옆으로 빠지지 않음.
//         땅 위면 닿은 자리에서 멈춤, 공중이면 벽에 붙은 채 떨어짐
//     닿은 면이 눕혀져 있으면(수직에서 45도 넘게: 바닥 경사, 보스 머리 위, 천장) → 엔진 그대로 (머리 위에 걸리지 않게)
//   판단 재료(루트 모션, 가속도)는 엔진이 이동 기록(Saved Move)에 넣는다 → 서버와 내 컴퓨터가 같은 판단.
//   몸 막이 밀어내기는 엔진 원래 함수를 직접 부른다 (언제나 미끄러짐)
//
// ─────────────────────────────────────────────────────────────────────────────
// [이동속도 증가] (2026-11-13)
//   최대 속도 = 엔진 값(BP 걷기 속도) × (1 + 내 MoveSpeedBonus) (ULOAAttributeSet_Player, 버프 GE가 올림, 기본 0)
//   엔진 확인: 걷기 속도 계산(CalcVelocity)이 GetMaxSpeed를 부르고, 내 컴퓨터 예측과 서버 확인 둘 다 같은 함수를 지난다
//     → 덮어써서 곱하면 두 쪽이 같은 식. 예) 걷기 600, 버서커 시너지 +0.1 → 660
//   한계: 버프는 서버가 걸고 스탯은 편도만큼 늦게 내 컴퓨터에 온다 → 그 사이 서버만 빠르게 계산해 작은 보정이 생길 수 있다
//     예) 600 × 0.1 × 편도 0.1초 ≈ 6cm. Net PktLag로 확인 (티 나면 그때 대책)
//   루트 모션(회피·CC·Move 모듈)은 최대 속도를 쓰지 않아서 영향 없음
//
// [설정] BP_LOACharacter → Character Movement (LOA|BodyBlock): Push Out Speed, Overlap Tolerance
//                                              (LOA|Movement): Wall Slide Normal Z

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/NetSerialization.h"
#include "LOACharacterMovementComponent.generated.h"

/** 서버가 정한 CC 궤적 숫자. 맞은 사람 컴퓨터로 복제되어 같은 궤적을 건다 */
USTRUCT()
struct FLOACrowdControlMove
{
	GENERATED_BODY()

	/** 바뀔 때마다 +1. 같은 숫자로 또 맞아도 복제가 다시 오게 */
	UPROPERTY()
	uint8 Sequence = 0;

	/** true = 궤적을 건다, false = 지운다 */
	UPROPERTY()
	bool bActive = false;

	/** 밀리는 수평 방향 (길이 1) */
	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	/** 수평 거리 (cm) */
	UPROPERTY()
	float Distance = 0.f;

	/** 출발할 때 땅 기준 높이 (cm) */
	UPROPERTY()
	float StartHeight = 0.f;

	/** 꼭대기 높이 (땅 기준, cm). 0 = 높이 변화 없음 (경직) */
	UPROPERTY()
	float ApexHeight = 0.f;

	/** 걸리는 시간 (초) */
	UPROPERTY()
	float Duration = 0.f;

	/** 서버가 궤적을 건 위치. 맞은 사람 컴퓨터는 도착 지점 = 이 위치 + 방향 × 거리로 맞춘다 (헤더 [내 자리에서 출발, 서버와 같은 자리에 도착]) */
	UPROPERTY()
	FVector_NetQuantize10 StartLocation = FVector::ZeroVector;
};

class UAbilitySystemComponent;

UCLASS()
class LOA_API ULOACharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	ULOACharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	/**
	 * CC 궤적을 건다. [서버] 맞은 사람 컴퓨터에도 같은 숫자로 걸린다. 이미 걸려 있으면 버리고 새로.
	 * @param Direction   밀리는 수평 방향 (때린 쪽 반대). 캐릭터는 이 반대 방향(때린 쪽)을 바라본다
	 * @param Distance    수평 거리 (cm)
	 * @param StartHeight 지금 땅 기준 높이 (cm). 땅이면 0
	 * @param ApexHeight  꼭대기 (땅 기준, cm). 0이면 옆으로만 (경직)
	 * @param Duration    걸리는 시간 (초)
	 * 예) 경직: (방향, 100, 0, 0, 0.15) / 띄움: (방향, 300, 0, 250, 1.0)
	 */
	void StartCrowdControlMove(const FVector& Direction, float Distance, float StartHeight, float ApexHeight, float Duration);

	/** 걸려 있는 CC 궤적을 지운다. [서버] 맞은 사람 컴퓨터에서도 지워진다 (CC가 끊겼을 때: 기상기·사망 등) */
	void StopCrowdControlMove();

	/** 지금 몸 막이에 겹쳐 밀려나는 중인가 (디버그용) */
	bool IsPushedOutOfBodyBlocker() const { return bInsideBodyBlocker; }

	//~ Begin UMovementComponent Interface
	/** 최대 속도 = 엔진 값 × (1 + 내 MoveSpeedBonus). 예측·서버 둘 다 (헤더 [이동속도 증가], 2026-11-13). 엔진처럼 public */
	virtual float GetMaxSpeed() const override;
	//~ End UMovementComponent Interface

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	//~ End UActorComponent Interface

	//~ Begin UCharacterMovementComponent Interface
	/** 매 이동 계산 직전 (내 컴퓨터 예측, 서버 확인 둘 다). 겹침 확인 → 막힘/무시 → 밀어내기 */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	/** 클릭 이동이 아니고 벽처럼 선 면이면 옆으로 빠지지 않는다 (땅 위 = 멈춤, 공중 = 그대로 떨어짐). 아니면 엔진 그대로 */
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;

	/**
	 * 이동 입력(가속도)을 거른다. State.Block.Move가 있으면 0 → 걷지 않는다. (2026-10-23 사용자 결정)
	 * 엔진이 내 컴퓨터 예측(ControlledCharacterMove)과 서버 확인(MoveAutonomous) 둘 다에서 부른다.
	 *   → 클라를 조작해서 막힘 검사를 건너뛰고 걷는 기록을 보내도, 서버가 가속도를 0으로 바꿔 따라 하지 않는다 → 보정으로 되돌림.
	 * 클릭 이동 부품(ULOAClickMoveComponent::IsMoveBlocked)의 막기는 그대로 둔다: 입력을 아예 안 보내고 가던 길도 지움 (정상 클라용).
	 * 루트 모션(회피·궤적·돌진)은 가속도가 아니라서 영향 없다.
	 */
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;

	/**
	 * [서버] 클라 위치가 서버 위치와 너무 다른가. CC 유예 중이면 Flight Limit까지 오차 아님 (보정 안 보냄, 서버 궤적 그대로).
	 * 그 밖에는 엔진 그대로. 헤더 [내 자리에서 출발, 서버와 같은 자리에 도착]
	 */
	virtual bool ServerExceedsAllowablePositionError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation,
		const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;
	//~ End UCharacterMovementComponent Interface

	/** 겹쳤을 때 밀려나는 속도. 클수록 빨리 빠져나간다. (예: 600 → 160cm 겹침이 약 0.27초) */
	UPROPERTY(EditAnywhere, Category = "LOA|BodyBlock", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float PushOutSpeed = 600.f;

	/** 이만큼보다 깊게 겹쳐야 "겹침"으로 본다. 막혀서 맞닿아 있는 상태를 겹침으로 잘못 보지 않게. */
	UPROPERTY(EditAnywhere, Category = "LOA|BodyBlock", meta = (ClampMin = "0", Units = "cm"))
	float OverlapTolerance = 2.f;

	/** "벽처럼 선 면"의 기준. 면 방향의 Z 절댓값이 이 값 이하면 벽 (0.707 = 수직에서 45도). 클릭 이동이 아닐 때 이런 면에서는 옆으로 빠지지 않는다 */
	UPROPERTY(EditAnywhere, Category = "LOA|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float WallSlideNormalZ = 0.707f;

	/**
	 * 시작 차이 한도 (cm). [클라] 받은 순간 서버 시작 위치와 이만큼 넘게 다르면 메우지 않고 서버 시작 위치로 옮긴다.
	 * [서버] 유예 중 차이가 이만큼 넘으면 바로 보정한다. (핑 사이에 회피·순간이동)
	 * 300 = 정상 클라의 최대 차이 (핑 225 띄움: 처음 걸어간 거리 약 135 + 궤적이 늦게 시작한 만큼 약 130) 위, 회피 500 아래
	 */
	UPROPERTY(EditAnywhere, Category = "LOA|CrowdControl", meta = (ClampMin = "0", Units = "Centimeters"))
	float CrowdControlFlightLimit = 300.f;

	/** [서버] 유예 시간 = 궤적 시간 + 그 사람 핑 + 이 값 (초). 핑 숫자 오차·프레임 여유 */
	UPROPERTY(EditAnywhere, Category = "LOA|CrowdControl", meta = (ClampMin = "0", Units = "Seconds"))
	float CrowdControlTrustExtraTime = 0.2f;

private:
	/** 내 ASC (PlayerState에 있음. 캐릭터가 IAbilitySystemInterface로 돌려준다). PlayerState 도착 전이면 nullptr */
	const UAbilitySystemComponent* GetOwnerAbilitySystem() const;

	/** 서버가 정한 CC 궤적 숫자 (맞은 사람 컴퓨터에만 복제) */
	UPROPERTY(ReplicatedUsing = OnRep_CrowdControlMove)
	FLOACrowdControlMove CrowdControlMove;

	/** [맞은 사람 컴퓨터] 서버가 궤적을 걸었다/지웠다 → 같은 숫자로 따라 한다 */
	UFUNCTION()
	void OnRep_CrowdControlMove();

	/** 이 컴퓨터에서 궤적을 건다 (루트 모션 소스) + 때린 쪽 바라보기 */
	void ApplyCrowdControlMoveLocally(const FLOACrowdControlMove& Move);

	/** 이 컴퓨터에서 궤적을 지운다 */
	void RemoveCrowdControlMoveLocally();

	/** 이 컴퓨터에 걸린 궤적의 번호 (엔진이 준 ID). 0 = 없음 */
	uint16 ActiveCrowdControlSourceId = 0;

	/** [서버] CC 유예가 끝나는 월드 시각 (그때까지 보정 안 보냄). 이 뒤 첫 이동에서 끝 차이를 로그로. 0 = 없음 */
	double CrowdControlTrustEndTime = 0.0;

	/** [서버] 이번 CC 유예 동안 가장 컸던 차이 (cm, 로그용) */
	float CrowdControlMaxDiff = 0.f;

	/**
	 * 내 캡슐과 깊게 겹친 몸 막이들을 찾아, 빠져나갈 방향(바닥 기준, 길이 1)을 구한다.
	 * @return 겹친 몸 막이가 있으면 true
	 */
	bool FindPushOutDirection(FVector& OutDirection) const;

	/** 내 ASC에 State.PassThroughBodyBlock이 있나 (관통 중). ASC가 아직 없으면 false */
	bool IsPassingThroughBodyBlockers() const;

	/** 내 캡슐이 몸 막이를 무시할지(true) 막힐지(false). 바뀔 때만 실제로 설정한다. */
	void SetIgnoreBodyBlockers(bool bIgnore);

	/** 지금 겹쳐서 밀려나는 중인가 */
	bool bInsideBodyBlocker = false;

	/** 지금 내 캡슐이 몸 막이를 무시하고 있나 */
	bool bIgnoringBodyBlockers = false;

	/** 처음 BodyBlock 응답 (ALOACharacter 생성자에서 Block). 무시를 끝낼 때 이 값으로 */
	TEnumAsByte<ECollisionResponse> DefaultBodyBlockResponse = ECR_Block;

	// ---------- 레이드 가장자리 벽 (2026-11-01) ----------

	/** CC 궤적이 지금 걸려 있나 (루트 모션 소스가 아직 살아 있음). 다 끝났거나(시간·착지) 지워졌으면 false */
	bool IsCrowdControlMoving();	// const 아님: 엔진 GetRootMotionSourceByID가 const가 아니다

	/** 내 캡슐이 가장자리 벽(RaidEdge)을 무시할지(true = CC로 밀려 낙사 가능) 막힐지(false). 바뀔 때만 실제로 설정한다 */
	void SetIgnoreRaidEdges(bool bIgnore);

	/** 지금 내 캡슐이 가장자리 벽을 무시하고 있나 */
	bool bIgnoringRaidEdges = false;

	/** 처음 RaidEdge 응답 (ALOACharacter 생성자에서 Block). 무시를 끝낼 때 이 값으로 */
	TEnumAsByte<ECollisionResponse> DefaultRaidEdgeResponse = ECR_Block;
};
