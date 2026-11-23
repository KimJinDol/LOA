// LOAStateTreeTask_CollapseTerrain.h
// [StateTree Task] "LOA Collapse Terrain" — 아직 멀쩡한 지형 구역 중 하나를 무너뜨리기 시작한다. (2026-11-14)
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 교전의 맨 위 자식 "지형 파괴 1·2" (LOA Health Percent + LOA Terrain Collapse Count 조건으로 들어감)
//   들어가면 바로 요청하고 끝난다 (Succeeded). 보스는 기다리지 않고 다음 패턴을 한다 → 예고 3초 동안에도 보스가 방해
//   예) HP 39% → 교전으로 돌아옴 → 지형 파괴 1 → 상황판이 오른쪽을 고름 → 오른쪽 예고 3초 → 그동안 숨 고르기·패턴
//
// [흐름]
//   EnterState : ULOARaidSubsystem::RequestTerrainCollapse → 고를 구역이 있었으면 Succeeded, 없으면 Failed
//   어느 구역인지, 예고·무너짐은 상황판과 구역(ALOATerrainZone)이 한다. 이 Task는 "하나 무너뜨려"만 말한다
//
// [네트워크] 서버만
//
// [에디터에서]
//   Actor : 자동 연결 (Context — 보스. 월드를 찾는 데만 씀)
//   전환  : On Completed → 교전 (Succeeded·Failed 둘 다. 실패가 부모로 올라가면 전투 시계가 0이 된다: 9장 보스 두뇌 2)

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "LOAStateTreeTask_CollapseTerrain.generated.h"

USTRUCT()
struct LOA_API FLOAStateTreeTask_CollapseTerrainInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;
};

USTRUCT(meta = (DisplayName = "LOA Collapse Terrain", Category = "LOA"))
struct LOA_API FLOAStateTreeTask_CollapseTerrain : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeTask_CollapseTerrainInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
