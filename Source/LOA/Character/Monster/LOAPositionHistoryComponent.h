// LOAPositionHistoryComponent.h
// [배관] 몸의 자리 기록. 서버가 최근 약 1초 동안 "그 서버 시각에 이 몸이 어디에, 어느 방향으로 있었나"를 적어 둔다. (2026-11-02)
// 공격자 판정(플레이어 → 보스)에서 서버가 "그 플레이어 화면에 보였던 보스 자리"로 되감아 보고를 검사할 때 쓴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가] 예: 핑 200ms(편도 0.1초), 보스가 초당 500cm로 걷는 중
//   0.4초 내 화면에서 대검이 보스를 맞힘. 내 화면의 보스 = 서버 0.3초 때 자리 A (복제가 편도만큼 늦게 온다)
//   0.5초 보고가 서버에 도착. 서버의 보스는 이미 0.2초 × 500 = 100cm 더 간 B
//   → 서버가 B로 검사하면 반지름 끝에 걸친 공격이 빗나간다. A를 알아야 한다 → 이 기록에서 0.3초를 찾는다.
//
// [내 화면의 보스가 "서버 몇 초 때 자리"인가 — 짐작하지 않는다]
//   엔진 캐릭터는 다른 컴퓨터에 위치를 보낼 때, 그 위치가 마지막으로 바뀐 서버 시각도 같이 보낸다.
//     ACharacter::ReplicatedServerLastTransformUpdateTimeStamp (Character.h)
//     값 = 서버 이동 계산(PerformMovement) 끝에서 위치·회전이 바뀌었으면 World->GetTimeSeconds() (CharacterMovementComponent.cpp)
//   단, 기본으로는 0이 간다: 이동 부드럽게 하기(Network Smoothing Mode)가 Linear이거나
//     bNetworkAlwaysReplicateTransformUpdateTimestamp가 켜져 있을 때만 보낸다 (Character.cpp PreReplication)
//   → 이 컴포넌트가 서버 BeginPlay에서 주인 캐릭터의 그 칸을 켠다. (보스 위치를 보낼 때마다 4바이트)
//   이 기록도 같은 프레임에 같은 시계(World->GetTimeSeconds())로 적으므로, 클라가 보낸 시각으로 그대로 찾을 수 있다.
//     서버의 LOANetTime::GetServerTimeNow도 같은 시계다 (서버에서는 GameState 서버 시각 = 월드 시각)
//
// [찾는 규칙] FindAt(시각)
//   그 시각 이하 중 가장 늦은 기록. 위치는 프레임마다 한 칸이라 사이를 섞지 않는다 (클라가 본 것도 그 프레임의 자리)
//   기록보다 옛날 → 가장 오래된 기록 (보스가 가만히 서 있으면 "마지막으로 움직인 시각"이 아주 옛날일 수 있다.
//                   그 뒤로 안 움직였으니 가장 오래된 기록과 같은 자리다)
//   기록보다 미래 → 가장 최근 기록
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1 서버만 : 기록 (Tick은 서버에서만 켠다), 찾기
//   2 복제   : 엔진이 보스 위치와 그 서버 시각을 보낸다 (이 컴포넌트가 켜는 칸)
//   3 각자   : 없음
//
// [BP 설정] 없음. 붙이기만 하면 된다 (ALOABossCharacter 생성자). ACharacter가 아닌 몸(허수아비)은 시각을 못 보내서 붙이지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LOAPositionHistoryComponent.generated.h"

/** 기록 한 칸 */
struct FLOAPositionHistorySample
{
	/** 서버 월드 시각 (초) */
	double ServerTime = 0.0;
	FVector Location = FVector::ZeroVector;
	/** 보는 방향 (도) */
	float Yaw = 0.f;
};

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOAPositionHistoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOAPositionHistoryComponent();

	/**
	 * [서버] 그 서버 시각의 자리·방향. 헤더 [찾는 규칙]
	 * @return 기록이 하나도 없으면 false (Out은 그대로)
	 */
	bool FindAt(double ServerTime, FVector& OutLocation, float& OutYaw) const;

	/** 몇 초치를 남기나. 되감기 한도(핑 + 여유)보다 길게 */
	static constexpr double HistoryLength = 1.0;

protected:
	//~ Begin UActorComponent Interface
	/** [서버] 주인 캐릭터가 위치의 서버 시각을 보내게 켜고, 기록을 시작한다. 클라는 Tick을 끈다 */
	virtual void BeginPlay() override;
	/** [서버] 이번 프레임 자리를 적고 오래된 칸을 버린다. 이동 계산 뒤(PostPhysics)라 이 프레임의 최종 자리 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

private:
	/** 오래된 것부터. 60fps면 약 60칸 */
	TArray<FLOAPositionHistorySample> Samples;
};
