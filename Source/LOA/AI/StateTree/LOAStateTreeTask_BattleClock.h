// LOAStateTreeTask_BattleClock.h
// [StateTree Task] "LOA Battle Clock" — 전투 경과 시간과 "보스가 맵 중앙 밖에 연속으로 있던 시간"을 잰다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 보스 트리의 "전투" 상태 (스턴·교전의 부모)
//   이 Task는 끝나지 않는다(Running). 전투 상태가 끝날 때까지 시계를 잰다.
//   자식 상태의 조건(엔진 Float Compare)이 출력 두 개에 연결한다.
//     예) 지연 패턴 상태 : Elapsed ≥ 60            (사용자 결정 2026-10-18: 1페이즈를 1분 안에 못 깨면 포효, 1분마다 반복)
//        중앙 복귀 상태 : TimeAwayFromCenter ≥ 20  (사용자 결정 2026-10-18: 중앙에서 먼 시간이 20초쯤이면 중앙 복귀 패턴)
//
//   왜 "교전"이 아니라 "전투"에 두나:
//     교전은 숨 고르기 뒤마다 다시 들어가서(대상 다시 고르기) 그 안의 Task가 다시 시작된다.
//     시계를 교전에 두면 패턴 하나 끝날 때마다 0이 된다. 전투는 전멸해서 대기로 갈 때까지 계속 머문다.
//
// [흐름]
//   EnterState : Elapsed = 0, TimeAwayFromCenter = 0, ALOAGameState::StartBattle() (전투 중으로 + 공용 부활 목숨 시계 시작. 전투 전일 때만 받음 → 전멸 뒤 다시 다가가면 다시 시작) → Running
//   Tick       : Elapsed += 이번 프레임 시간
//                보스가 중앙 반경(Center Radius, 바닥 거리) 밖이면 TimeAwayFromCenter += 이번 프레임 시간, 안이면 0
//                예) 반경 1200. 보스가 중앙에서 1500cm에 20초 서 있음 → 20 → 중앙 복귀 → 중앙 도착 → 0
//   중앙 = ULOARaidSubsystem::FindLocation(Center Location Tag). 레벨에 위치 표시(ALOALocationMarker)가 있어야 한다.
//   못 찾으면 경고 한 번, TimeAwayFromCenter는 0 그대로 (중앙 복귀가 안 나옴)
//
//   시간은 게임 시간이다. (slomo 0.5면 시계도 반 속도. 패턴 테스터의 LOASlomo로 확인할 때 참고)
//   스턴 중에도 시계는 간다.
//
// [네트워크] 서버만 (StateTree는 서버에만 있다)
//
// [에디터에서]
//   Actor               : 자동 연결 (Context — 보스)
//   Center Location Tag : 중앙 위치 표시의 태그 (예: Location.Arena.Center)
//   Center Radius       : 이 거리(cm, 바닥 거리) 밖이면 "중앙에서 멀다". 기본 1200
//   Elapsed             : 출력. 전투 시작 후 초
//   Time Away From Center : 출력. 중앙 반경 밖에 연속으로 있던 초

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "GameplayTagContainer.h"
#include "LOAStateTreeTask_BattleClock.generated.h"

USTRUCT()
struct LOA_API FLOAStateTreeTask_BattleClockInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 맵 중앙 위치 표시의 태그 (예: Location.Arena.Center) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Location"))
	FGameplayTag CenterLocationTag;

	/** 이 거리(바닥 거리) 밖이면 "중앙에서 멀다" */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float CenterRadius = 1200.f;

	/** 전투 시작 후 초. 자식 상태 조건이 연결한다 (예: ≥ 60 → 지연 패턴) */
	UPROPERTY(EditAnywhere, Category = "Output")
	float Elapsed = 0.f;

	/** 중앙 반경 밖에 연속으로 있던 초. 반경 안에 들어오면 0 (예: ≥ 20 → 중앙 복귀) */
	UPROPERTY(EditAnywhere, Category = "Output")
	float TimeAwayFromCenter = 0.f;

	/** [실행 중] 중앙을 못 찾았다는 경고를 이미 했나 (매 프레임 경고하지 않게) */
	bool bWarnedNoCenter = false;
};

USTRUCT(meta = (DisplayName = "LOA Battle Clock", Category = "LOA"))
struct LOA_API FLOAStateTreeTask_BattleClock : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeTask_BattleClockInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
