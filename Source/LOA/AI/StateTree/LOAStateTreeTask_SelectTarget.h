// LOAStateTreeTask_SelectTarget.h
// [StateTree Task] "LOA Select Random Target" — 살아 있는 적(플레이어) 중 한 명을 무작위로 골라 내보낸다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 보스 트리의 "교전" 상태 (추적·패턴·숨 고르기의 부모. 2026-10-18 "전투" 아래로 한 단계 내림)
//   이 Task는 고른 뒤에도 끝나지 않고(Running) 교전 상태가 끝날 때까지 대상을 들고 있는다.
//   → 자식 상태(추적, 패턴)가 출력 Target에 선을 연결해 쓴다.
//
//   왜 별도 상태가 아니라 부모에 두나:
//     StateTree는 "같은 상태 또는 부모 상태의 Task 출력"만 연결할 수 있다. 옆 상태(형제)의 출력은 못 쓴다.
//     그래서 대상을 고르는 Task를 부모(전투)에 두고, 자식들이 그 출력을 받는다.
//
//   다시 고르기: 자식에서 "교전" 상태로 다시 전환하면 이 Task가 다시 시작(EnterState)되며 새로 고른다.
//     예) 패턴 끝 → 숨 고르기 → 교전(다시 고르기) → 이번엔 플레이어C
//
// [흐름]
//   EnterState : 살아 있는 적 중 무작위 1명 → Target에 넣고 Running. 아무도 없으면 Failed
//   Tick       : 고른 대상이 죽거나 사라지면 살아 있는 다른 적으로 바꿔 Running.
//                아무도 없으면(전멸) Failed → (Transition) 대기로
//                예) B를 노리는 중 B 사망 → A·C 중 무작위로 바꿈. 상태를 떠나지 않으므로 부모 "전투"의 전투 시계도 그대로
//                (2026-10-18 변경: 예전에는 바로 Failed → 대기 → 전투로 다시 들어가서, 한 명만 죽어도 전투 시계가 0이 됐을 것)
//
// [어그로 규칙] 문서 9장 보스 구조: "패턴 고를 때마다 살아 있는 플레이어 중 무작위"
//
// [네트워크] 서버만
//
// [에디터에서]
//   Actor  : 자동 연결 (Context — 보스)
//   Target : 출력. 자식 상태의 Move To(Target Actor), LOA Run Pattern(Target)에 연결

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "LOAStateTreeTask_SelectTarget.generated.h"

USTRUCT()
struct LOA_API FLOAStateTreeTask_SelectTargetInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 고른 대상. 자식 상태의 Task들이 이 값에 연결한다. (죽으면 다른 적으로 바뀐다) */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> Target = nullptr;
};

USTRUCT(meta = (DisplayName = "LOA Select Random Target", Category = "LOA"))
struct LOA_API FLOAStateTreeTask_SelectTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeTask_SelectTargetInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	/** 살아 있는 적 중 무작위 1명을 Target에 넣는다. 아무도 없으면 false */
	static bool PickRandomTarget(FInstanceDataType& InstanceData);
};
