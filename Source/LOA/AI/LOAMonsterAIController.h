// LOAMonsterAIController.h
// [배관] 보스·몬스터의 두뇌를 돌리는 컨트롤러. StateTree를 실행하는 부품을 가진다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할 분담 - 보스 AI]
//   이 컨트롤러          : StateTree를 언제 켜고 끄나 (빙의하면 시작, 몸이 죽으면 정지)
//   StateTree 에셋       : 다음에 뭘 할지 (대기 → 대상 고르기 → 추적 → 패턴 …)   ← 에디터에서 그림
//   StateTree Task (C++) : 상태 안에서 실제로 하는 일 (AI/StateTree/)
//   패턴 GA              : 실제 공격 (ULOAMonsterAbility)
//   이 컨트롤러는 "어떤 보스인지", "어떤 패턴이 있는지"를 모른다. → 일반 몬스터도 같은 클래스, 트리 에셋만 다르게.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (서버만. AIController는 원래 서버에만 있다)
//
//   보스 스폰 (레벨 배치) → 엔진이 이 컨트롤러를 만들어 빙의 (보스 BP의 AI Controller Class)
//     OnPossess
//       ├─ 몸의 사망 반응 부품 OnDied 구독
//       ├─ 이미 죽은 몸이면 시작하지 않음
//       └─ StateTree 시작                ← 빙의 "후"에 시작해야 트리가 몸(Actor)을 안다
//   몸이 죽음 → OnDied → StateTree 정지 + 이동 정지 (서버에서 한 번)
//   OnUnPossess → 구독 해제 + 정지
//
//   StateTree를 자동 시작하지 않는 이유: 컨트롤러가 먼저 생기고 빙의는 나중에 된다.
//   자동 시작하면 "몸 없음" 상태로 트리가 시작될 수 있다. → 빙의 시점에 직접 시작한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만 : 이 컨트롤러, StateTree 전체 (대기·대상 고르기·추적 판단·패턴 발동)
//   2. 복제   : 결과(보스 이동·회전·몽타주)는 보스 액터가 복제한다
//   3. 각자   : 없음
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것]
//   BP_AIC_<보스> (부모: LOAMonsterAIController)
//     StateTreeComponent → State Tree : ST_<보스>  (Schema = StateTree AI Component)
//   BP_Boss_<보스>
//     Pawn → AI Controller Class : BP_AIC_<보스>, Auto Possess AI : Placed in World or Spawned
//   레벨: Nav Mesh Bounds Volume (추적 이동에 필요)

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "LOAMonsterAIController.generated.h"

class UStateTreeAIComponent;

UCLASS()
class LOA_API ALOAMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	ALOAMonsterAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** StateTree 실행 부품. (나중에 그로기 등 이벤트를 트리에 보낼 때 여기로 SendStateTreeEvent) */
	UStateTreeAIComponent* GetStateTreeComponent() const { return StateTreeComponent; }

	/**
	 * [서버] 테스트용 멈춤 / 재개.
	 *   멈춤 : 트리와 이동을 멈춘다. (하던 패턴도 Run Pattern이 떠나면서 취소된다)
	 *   재개 : 트리를 처음(대기)부터 다시 시작한다. 죽은 몸이면 재개하지 않는다.
	 * 쓰는 곳: 콘솔 LOA.Boss.AI Pause / Resume (나중에 패턴 테스터 창)
	 */
	void SetBrainPaused(bool bPaused);

protected:
	//~ Begin AController Interface
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	//~ End AController Interface

private:
	/** 몸의 사망 반응 부품이 "죽었다"고 알릴 때 (AddDynamic이라 UFUNCTION) */
	UFUNCTION()
	void HandlePawnDied(AActor* DeadActor);

	/** 트리와 이동을 멈춘다. (사망, 빙의 해제) */
	void StopBrain(const FString& Reason);

	/** StateTree를 실행하는 부품. 어떤 트리를 쓸지는 BP의 이 부품 칸에서 정한다. */
	UPROPERTY(VisibleAnywhere, Category = "LOA|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;
};
