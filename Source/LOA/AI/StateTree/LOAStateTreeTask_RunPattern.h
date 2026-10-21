// LOAStateTreeTask_RunPattern.h
// [StateTree Task] "LOA Run Pattern" — 후보 패턴 중 지금 쓸 수 있는 것을 무작위로 하나 골라 대상에게 쓰고, 끝날 때까지 기다린다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 보스 트리의 "패턴" / "지연 패턴" / "중앙 복귀" 상태
//   예) 패턴 상태 : Pattern Tags = { Ability.Boss.Slam, Ability.Boss.TwinSlash, Ability.Boss.Downward }
//      지연 상태 : Pattern Tags = { Ability.Boss.Roar }   ← 하나만 넣으면 그것만 쓴다
//      Target = (부모 "교전"의 Select Random Target 출력)
//
// [흐름]
//   EnterState : 후보마다 ULOAMonsterAbility::CanActivatePattern(보스, 태그, Target)로 "지금 쓸 수 있나" 확인
//                  예) Slam 쿨타임 중, TwinSlash·Downward 가능 → 둘 중 무작위 (각 50%)
//                  쓸 수 있는 것이 없음 → Failed (트리가 숨 고르기 후 다시 고른다)
//                → 고른 패턴을 TryActivatePattern → 번호표(Spec Handle)를 들고 Running
//   Tick       : 그 패턴이 아직 도는지 확인 → 끝났으면 Succeeded
//   ExitState  : 패턴이 아직 도는데 상태를 떠나면(스턴·그로기 끼어들기, 트리 정지) 패턴을 취소한다
//                → 트리는 다른 상태로 갔는데 보스가 계속 내려찍는 일이 없게
//
//   패턴이 끝났는지는 매 틱 "그 번호표의 패턴이 활성인가"로 확인한다. (알림 구독보다 단순하고, 놓칠 일이 없다)
//   무작위는 여기(C++)서 한다. 같은 패턴 연달아 쓰기는 패턴의 쿨타임이 막는다. (사용자 결정 2026-10-18: 완전 무작위)
//
// [실패했을 때] 쓸 수 있는 후보가 없으면 바로 Failed. 트리에서 "숨 고르기"(Delay) 상태를 거쳐 다시 고르게 해야
//   매 프레임 발동 시도를 반복하지 않는다. (BP 만들기 참고)
//
// [네트워크] 서버만. 패턴 GA도 서버만 실행된다.
//
// [에디터에서]
//   Actor        : 자동 연결 (Context — 보스)
//   Pattern Tags : 후보 패턴들의 Asset Tag (예: Ability.Boss.Slam). 하나 이상
//   Target       : 입력. 부모 상태의 LOA Select Random Target → Target에 연결 (비우면 대상 없이 발동)

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "LOAStateTreeTask_RunPattern.generated.h"

USTRUCT()
struct LOA_API FLOAStateTreeTask_RunPatternInstanceData
{
	GENERATED_BODY()

	/** 이 트리를 쓰는 몸 (보스). Schema가 자동으로 채운다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** 후보 패턴들. 패턴 GA의 Asset Tags에 있는 태그 (예: Ability.Boss.Slam). 지금 쓸 수 있는 것 중 무작위로 하나 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Ability"))
	FGameplayTagContainer PatternTags;

	/** 노릴 대상. 보통 부모 상태의 Select Random Target 출력에 연결한다. 비우면 대상 없이 발동 */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<AActor> Target = nullptr;

	/** [실행 중] 발동한 패턴의 번호표. 끝났는지 확인하고, 상태를 떠날 때 취소하는 데 쓴다 */
	FGameplayAbilitySpecHandle ActivatedHandle;

	/** [실행 중] 이번에 고른 패턴 (로그용) */
	FGameplayTag ActivatedTag;
};

USTRUCT(meta = (DisplayName = "LOA Run Pattern", Category = "LOA"))
struct LOA_API FLOAStateTreeTask_RunPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLOAStateTreeTask_RunPatternInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	/** 발동한 패턴이 아직 도는 중인가 */
	static bool IsPatternActive(const FInstanceDataType& InstanceData);
};
