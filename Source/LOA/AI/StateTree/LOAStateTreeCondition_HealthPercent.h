// LOAStateTreeCondition_HealthPercent.h
// [StateTree 조건] "LOA Health Percent" — 보스 체력이 몇 % 이하인가 / 보다 많은가. (2026-11-14)
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 상태 들어가기 조건. HP 문턱을 레벨 블루프린트나 보스 BP 칸이 아니라 트리에 둔다 → 레이드 흐름이 트리 그래프에서 한눈에 보인다
//   예) 2페이즈 지형 파괴 (교전의 맨 위 자식)
//     지형 파괴 1 : LOA Health Percent(40, 이하면 참) 그리고 LOA Terrain Collapse Count(1, 보다 적으면 참) → LOA Collapse Terrain
//     지형 파괴 2 : LOA Health Percent(20, 이하면 참) 그리고 LOA Terrain Collapse Count(2, 보다 적으면 참) → LOA Collapse Terrain
//     체력 41% → 39% → 패턴이 끝나고 교전으로 돌아올 때 지형 파괴 1 → 왼·오 중 하나 무너짐
//   나중에 페이즈 전환 (HP 50% → 2페이즈)도 같은 조건으로.
//
// [판단] 체력 ÷ 최대 체력 × 100. 최대 체력이 0이거나 ASC가 없으면 거짓
//   예) 체력 39,000 / 100,000 = 39% → Percent 40, 이하면 참 → 참
//
// [네트워크] 서버만. 서버 ASC의 스탯을 본다
//
// [에디터에서]
//   Actor   : 자동 연결 (Context — 보스)
//   Percent : 문턱 (0 ~ 100)
//   Check   : 이하면 참 / 보다 많으면 참

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "LOAStateTreeCondition_HealthPercent.generated.h"

/** LOA Health Percent 조건이 언제 참인가 */
UENUM()
enum class ELOAHealthCheck : uint8
{
	AtOrBelow	UMETA(DisplayName = "이하면 참"),
	Above		UMETA(DisplayName = "보다 많으면 참"),
};

USTRUCT()
struct LOA_API FLOAStateTreeCondition_HealthPercentInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 문턱 (%) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "Percent"))
	float Percent = 50.f;

	/** 언제 참인가: 이하면 / 보다 많으면 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	ELOAHealthCheck Check = ELOAHealthCheck::AtOrBelow;
};

USTRUCT(meta = (DisplayName = "LOA Health Percent", Category = "LOA"))
struct LOA_API FLOAStateTreeCondition_HealthPercent : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeCondition_HealthPercentInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
