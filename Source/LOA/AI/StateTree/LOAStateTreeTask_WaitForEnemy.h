// LOAStateTreeTask_WaitForEnemy.h
// [StateTree Task] "LOA Wait For Enemy" — 살아 있는 적(플레이어)이 정해진 거리 안에 들어올 때까지 기다린다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 보스 트리의 "대기" 상태
//   예) Range 1500 → 플레이어가 보스에게서 1500cm 안으로 들어오는 순간 성공 → (Transition) 전투로
//   거리는 높이를 무시한 바닥 거리. 대상이 죽어 있으면(State.Dead) 세지 않는다.
//
// [흐름]
//   EnterState : 바로 한 번 확인 → 있으면 성공, 없으면 Running
//   Tick       : Check Interval(0.25초)마다 다시 확인 → 있으면 성공
//   (실패하지 않는다. 적이 올 때까지 계속 기다린다)
//
// [나중에] 보스방 입구 트리거 + 입장 연출로 시작하게 바꾸면, 이 Task 대신 이벤트(예: Boss.Event.Engage)로 넘어간다.
//
// [네트워크] 서버만 (StateTree는 서버의 AIController에서만 돈다)
//
// [에디터에서]
//   Actor     : 자동 연결 (Context — 보스)
//   Range     : 트리 파라미터 AggroRange에 연결하거나 숫자 직접
//   Check Interval : 기본 0.25초

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "LOAStateTreeTask_WaitForEnemy.generated.h"

/** 보스마다 따로 가지는 값 */
USTRUCT()
struct LOA_API FLOAStateTreeTask_WaitForEnemyInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 이 거리 안에 살아 있는 적이 들어오면 성공 (cm, 높이 무시) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0", Units = "Centimeters"))
	float Range = 1500.f;

	/** 확인 간격 (초). 매 프레임 확인하지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.05", Units = "Seconds"))
	float CheckInterval = 0.25f;

	/** [실행 중] 다음 확인까지 남은 시간 */
	float TimeUntilNextCheck = 0.f;
};

USTRUCT(meta = (DisplayName = "LOA Wait For Enemy", Category = "LOA"))
struct LOA_API FLOAStateTreeTask_WaitForEnemy : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeTask_WaitForEnemyInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	/** Range 안에 살아 있는 적이 있나 */
	static bool IsEnemyInRange(const FInstanceDataType& InstanceData);
};
