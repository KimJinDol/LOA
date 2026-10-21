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
//   겹쳐 있음   : 막지 않는다 + 보스 중심 반대쪽으로 Push Out Speed만큼 밀어낸다. 내 이동도 그대로 된다.
//   빠져나옴    : 다시 막힘
//   관통 중     : 막지도 밀지도 않는다 (내 ASC에 State.PassThroughBodyBlock 태그가 있을 때. 예: 베스티지)
//   예) Push Out Speed 600, 160cm 겹침 → 약 0.27초 동안 슥 밀려 나감. 그 사이 옆으로 걸으면 그 방향도 더해짐.
//
//   "겹쳐 있음"은 2cm(Overlap Tolerance)보다 깊게 겹쳤을 때. 걸어가 막혀서 맞닿아 있는 건 겹침이 아니다.
//
// [어디서 하나] 이동 계산 안 (UpdateCharacterStateBeforeMovement: 매 이동 계산 직전에 엔진이 부름)
//   플레이어 이동은 내 컴퓨터가 먼저 계산(예측)하고 서버가 같은 입력으로 다시 계산해 확인한다.
//   밀어내기도 그 계산 안에 있어야 두 쪽 결과가 같아진다. 캐릭터 Tick 등 바깥에서 위치를 바꾸면 서버가 되돌린다(덜컥).
//
// [충돌 설정은 여기 한 곳만] 내 캡슐의 BodyBlock 응답(막힘/무시)은 이 컴포넌트만 바꾼다.
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
//     엔진이 두 쪽 궤적을 짝지어(Matches) 시간을 맞춘다 → 내 화면에서도 부드럽게 움직인다. 대가: 내 컴퓨터는 핑만큼 늦게 시작
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

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	//~ End UActorComponent Interface

	//~ Begin UCharacterMovementComponent Interface
	/** 매 이동 계산 직전 (내 컴퓨터 예측, 서버 확인 둘 다). 겹침 확인 → 막힘/무시 → 밀어내기 */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	/** 클릭 이동이 아니고 벽처럼 선 면이면 옆으로 빠지지 않는다 (땅 위 = 멈춤, 공중 = 그대로 떨어짐). 아니면 엔진 그대로 */
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;
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
};
