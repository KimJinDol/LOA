// LOAMonsterAIController.cpp

#include "AI/LOAMonsterAIController.h"
#include "Character/LOADeathReactionComponent.h"
#include "Core/LOALog.h"

#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAMonsterAIController)

ALOAMonsterAIController::ALOAMonsterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));

	// 자동 시작하지 않는다. 빙의한 뒤 OnPossess에서 직접 시작한다. (헤더 [흐름])
	StateTreeComponent->SetStartLogicAutomatically(false);
}

void ALOAMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		return;
	}

	// ① 몸이 죽으면 AI를 멈춘다. 사망 반응 부품이 없는 몸이면 경고만 (멈출 신호가 없음)
	ULOADeathReactionComponent* DeathReaction = InPawn->FindComponentByClass<ULOADeathReactionComponent>();
	if (DeathReaction)
	{
		DeathReaction->OnDied.AddUniqueDynamic(this, &ThisClass::HandlePawnDied);

		// 이미 죽은 몸에 빙의했으면 시작하지 않는다.
		if (DeathReaction->IsDead())
		{
			return;
		}
	}
	else
	{
		LOA_LOG(InPawn, Warning, TEXT("[AI] %s has no DeathReactionComponent. AI will not stop on death."), *GetNameSafe(InPawn));
	}

	// ② 트리 시작. 트리 에셋이 비어 있으면 엔진이 오류 로그를 남기고 아무것도 하지 않는다.
	if (!StateTreeComponent->IsRunning())
	{
		StateTreeComponent->StartLogic();
		LOA_LOG(InPawn, Log, TEXT("[AI] StateTree started on %s"), *GetNameSafe(InPawn));
	}
	
}

void ALOAMonsterAIController::OnUnPossess()
{
	if (APawn* PossessedPawn  = GetPawn())
	{
		if (ULOADeathReactionComponent* DeathReaction = PossessedPawn ->FindComponentByClass<ULOADeathReactionComponent>())
		{
			DeathReaction->OnDied.RemoveDynamic(this, &ThisClass::HandlePawnDied);
		}
	}

	StopBrain(TEXT("Unpossessed"));

	Super::OnUnPossess();
}

void ALOAMonsterAIController::SetBrainPaused(bool bPaused)
{
	if (bPaused)
	{
		StopBrain(TEXT("Paused"));
		return;
	}

	APawn* BossPawn = GetPawn();
	if (!BossPawn || !StateTreeComponent || StateTreeComponent->IsRunning())
	{
		return;
	}

	// 죽은 몸은 다시 깨우지 않는다.
	const ULOADeathReactionComponent* DeathReaction = BossPawn->FindComponentByClass<ULOADeathReactionComponent>();
	if (DeathReaction && DeathReaction->IsDead())
	{
		LOA_LOG(BossPawn, Warning, TEXT("[AI] Cannot resume. %s is dead."), *GetNameSafe(BossPawn));
		return;
	}

	StateTreeComponent->StartLogic();
	LOA_LOG(BossPawn, Log, TEXT("[AI] StateTree resumed on %s"), *GetNameSafe(BossPawn));
}

void ALOAMonsterAIController::HandlePawnDied(AActor* DeadActor)
{
	// 내 몸의 알림만
	if (DeadActor && DeadActor == GetPawn())
	{
		StopBrain(TEXT("Dead"));
	}
}

void ALOAMonsterAIController::StopBrain(const FString& Reason)
{
	// 트리 정지 → 지금 상태의 Task들이 ExitState를 받는다. (Run Pattern은 도는 패턴을 취소. 사망이면 GA_Death가 이미 취소함)
	if (StateTreeComponent && StateTreeComponent->IsRunning())
	{
		StateTreeComponent->StopLogic(Reason);
		LOA_LOG(GetPawn(), Log, TEXT("[AI] StateTree stopped (%s)"), *Reason);
	}

	// 걷던 길 지우기
	StopMovement();
}
