// LOAStateTreeCondition_HasTag.h
// [StateTree 조건] "LOA Has Tag" — 보스(ASC)에 이 태그가 있나 / 없나.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 전환(Transition) 조건, 상태 들어가기 조건
//   예) 스턴 (카운터 작업에서 남은 문제: AI를 켜면 스턴 중에 Move To로 걸어옴)
//     교전 → 스턴    : On Tick, LOA Has Tag(Tag = State.CrowdControl.StatusEffect.Stunned, Check = 태그가 있으면 참)
//     스턴 → 교전    : On Tick, LOA Has Tag(Tag = State.CrowdControl.StatusEffect.Stunned, Check = 태그가 없으면 참)
//     카운터 성공 → ULOAGA_Stun이 State.CrowdControl.StatusEffect.Stunned 3초 → 트리는 스턴 상태에서 아무것도 안 함
//     → 3초 뒤 스턴 GA가 끝나며 태그가 빠짐 → 교전으로 (대상 다시 고르기)
//   나중에 페이즈·기믹 태그도 같은 조건으로.
//
// [판단] ASC->HasMatchingGameplayTag. GE로 붙은 태그와 로컬 태그를 다 본다.
//   부모 태그로 물으면 자식 태그도 맞음 (예: State.Immune → State.Immune.StatusEffect도 "있음")
//
// [네트워크] 서버만. 서버 ASC의 태그를 본다 (State.CrowdControl.StatusEffect.Stunned는 서버 GE)
//
// [에디터에서]
//   Actor : 자동 연결 (Context — 보스)
//   Tag   : 확인할 태그
//   Check : 태그가 있으면 참 / 태그가 없으면 참

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "GameplayTagContainer.h"
#include "LOAStateTreeCondition_HasTag.generated.h"

/** LOA Has Tag 조건이 언제 참인가 */
UENUM()
enum class ELOATagCheck : uint8
{
	HasTag	UMETA(DisplayName = "태그가 있으면 참"),
	NoTag	UMETA(DisplayName = "태그가 없으면 참"),
};

USTRUCT()
struct LOA_API FLOAStateTreeCondition_HasTagInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 확인할 태그 (예: State.CrowdControl.StatusEffect.Stunned) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Tag;

	/** 언제 참인가: 태그가 있으면 / 없으면 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	ELOATagCheck Check = ELOATagCheck::HasTag;
};

USTRUCT(meta = (DisplayName = "LOA Has Tag", Category = "LOA"))
struct LOA_API FLOAStateTreeCondition_HasTag : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeCondition_HasTagInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
