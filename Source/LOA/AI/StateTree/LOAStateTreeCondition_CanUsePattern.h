// LOAStateTreeCondition_CanUsePattern.h
// [StateTree 조건] "LOA Can Use Pattern" — 보스가 지금 이 패턴을 쓸 수 있나. (발동은 안 함)
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 상태의 들어가기 조건 (Enter Conditions)
//   예) 지연 패턴 상태 : Elapsed ≥ 60 그리고 LOA Can Use Pattern(Ability.Boss.Roar)
//     전투 61초, 포효 쿨타임 없음 → 참 → 지연 패턴 상태로 → 포효 (쿨타임 60초 시작)
//     전투 75초, 포효 쿨타임 중  → 거짓 → 지연 상태를 건너뛰고 평소(추적 → 패턴)로
//     전투 121초, 쿨타임 끝      → 참 → 다시 포효 (사용자 결정: 1분마다 반복)
//   이 조건이 없으면 75초에도 지연 상태로 들어갔다가 Run Pattern이 실패 → 숨 고르기 → 또 지연 상태 …
//   → 평소 패턴을 영영 못 쓴다.
//
// [판단] ULOAMonsterAbility::CanActivatePattern — 쿨타임 / 다른 패턴 중 / 죽음 / Required인데 대상 없음
//   Run Pattern이 후보를 거를 때 쓰는 것과 같은 함수다.
//
// [네트워크] 서버만
//
// [에디터에서]
//   Actor       : 자동 연결 (Context — 보스)
//   Pattern Tag : 확인할 패턴의 Asset Tag
//   Target      : 입력(선택). Required 패턴이면 부모 "교전"의 Select Random Target 출력에 연결

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "GameplayTagContainer.h"
#include "LOAStateTreeCondition_CanUsePattern.generated.h"

USTRUCT()
struct LOA_API FLOAStateTreeCondition_CanUsePatternInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 확인할 패턴. 패턴 GA의 Asset Tags에 있는 태그 (예: Ability.Boss.Roar) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Ability"))
	FGameplayTag PatternTag;

	/** 노릴 대상. Target Usage가 Required인 패턴만 의미가 있다. 비우면 대상 없음으로 확인 */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<AActor> Target = nullptr;
};

USTRUCT(meta = (DisplayName = "LOA Can Use Pattern", Category = "LOA"))
struct LOA_API FLOAStateTreeCondition_CanUsePattern : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeCondition_CanUsePatternInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
