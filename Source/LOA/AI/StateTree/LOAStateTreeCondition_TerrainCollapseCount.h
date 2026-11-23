// LOAStateTreeCondition_TerrainCollapseCount.h
// [StateTree 조건] "LOA Terrain Collapse Count" — 무너진(예고 중 포함) 지형 구역이 몇 개보다 적은가 / 이상인가. (2026-11-14)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가] HP 문턱은 한 번 넘으면 계속 참이다. 이 조건이 없으면 체력 39%인 동안 교전으로 돌아올 때마다 또 무너뜨리려 한다.
//   → "이 문턱에서 무너뜨릴 몫을 이미 했나"를 무너진 구역 수로 본다. 따로 기억하는 칸이 없다 (상황판의 구역 상태가 곧 기록)
//   예) 지형 파괴 1 : Count 1, 보다 적으면 참 → 0개일 때만 들어감 → 무너뜨린 뒤(1개)에는 안 들어감
//       지형 파괴 2 : Count 2, 보다 적으면 참 → 1개일 때 들어감
//   전멸하면 구역이 멀쩡해져(0개) 처음부터 다시
//
// [판단] ULOARaidSubsystem::GetCollapsedTerrainCount (예고 중 + 무너짐). 예고를 시작한 순간부터 센다 → 예고 3초 동안 또 들어가지 않는다
//
// [네트워크] 서버만
//
// [에디터에서]
//   Actor : 자동 연결 (Context — 보스. 월드를 찾는 데만 씀)
//   Count : 기준 개수
//   Check : 보다 적으면 참 / 이상이면 참

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "LOAStateTreeCondition_TerrainCollapseCount.generated.h"

/** LOA Terrain Collapse Count 조건이 언제 참인가 */
UENUM()
enum class ELOACountCheck : uint8
{
	Below	UMETA(DisplayName = "보다 적으면 참"),
	AtLeast	UMETA(DisplayName = "이상이면 참"),
};

USTRUCT()
struct LOA_API FLOAStateTreeCondition_TerrainCollapseCountInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 기준 개수 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0"))
	int32 Count = 1;

	/** 언제 참인가: 보다 적으면 / 이상이면 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	ELOACountCheck Check = ELOACountCheck::Below;
};

USTRUCT(meta = (DisplayName = "LOA Terrain Collapse Count", Category = "LOA"))
struct LOA_API FLOAStateTreeCondition_TerrainCollapseCount : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeCondition_TerrainCollapseCountInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
