// LOASpectateComponent.cpp

#include "Player/LOASpectateComponent.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASpectateComponent)

ULOASpectateComponent::ULOASpectateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 0.2초마다 확인. 본인 컴퓨터에서만 켠다 (ActivateSpectate)
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.2f;

	// 복제하지 않는다. 카메라는 각자 일
	SetIsReplicatedByDefault(false);
}

void ULOASpectateComponent::ActivateSpectate(UEnhancedInputComponent* EnhancedInput)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// 키 설정 등록 (← / →). 살아 있는 동안 눌러도 Cycle에서 무시한다.
	if (const ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (SpectateIMC)
			{
				Subsystem->AddMappingContext(SpectateIMC, SpectateIMCPriority);
			}
		}
	}

	if (EnhancedInput)
	{
		if (NextTargetAction)
		{
			EnhancedInput->BindAction(NextTargetAction, ETriggerEvent::Started, this, &ThisClass::Input_Next);
		}
		if (PrevTargetAction)
		{
			EnhancedInput->BindAction(PrevTargetAction, ETriggerEvent::Started, this, &ThisClass::Input_Prev);
		}
	}

	if (!SpectateIMC || !NextTargetAction || !PrevTargetAction)
	{
		LOA_LOG(PC, Warning, TEXT("[Spectate] IMC or input actions not set on SpectateComponent. Auto spectate works, arrow keys do not."));
	}

	SetComponentTickEnabled(true);
}

// =============================================================================
// 확인 (0.2초마다)
// =============================================================================

void ULOASpectateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APlayerController* PC = GetOwningPlayerController();
	const UWorld* World = GetWorld();
	APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!MyPawn || !World)
	{
		return;
	}

	// ① 살아 있음: 관전 중이었으면 내 카메라로 돌아온다 (부활)
	if (!IsDead(MyPawn))
	{
		if (DeadSince >= 0.0)
		{
			DeadSince = -1.0;
			SetSpectateTarget(nullptr);
		}
		return;
	}

	// ② 방금 죽음: 시각만 적어 둔다. Start Delay 동안은 내 몸(사망 모션)을 본다.
	if (DeadSince < 0.0)
	{
		DeadSince = World->GetTimeSeconds();
	}

	if (!CanSpectate())
	{
		return;
	}

	// ③ 보던 아군이 없거나 죽었으면 다음 살아 있는 아군으로 (없으면 내 몸)
	const APawn* Current = SpectateTarget.Get();
	if (!Current || IsDead(Current))
	{
		Cycle(+1);
	}
}

// =============================================================================
// 대상 바꾸기
// =============================================================================

void ULOASpectateComponent::Input_Next()
{
	Cycle(+1);
}

void ULOASpectateComponent::Input_Prev()
{
	Cycle(-1);
}

void ULOASpectateComponent::Cycle(int32 Direction)
{
	if (!CanSpectate())
	{
		return;
	}

	TArray<APawn*> Allies;
	GatherLivingAllies(Allies);

	if (Allies.IsEmpty())
	{
		// 살아 있는 아군이 없다 → 내 몸
		SetSpectateTarget(nullptr);
		return;
	}

	const int32 Num = Allies.Num();
	const int32 CurrentIndex = Allies.IndexOfByKey(SpectateTarget.Get());

	// 지금 보는 아군이 목록에 없으면(처음, 또는 방금 죽어서 빠짐) 앞에서부터
	//   예) [B, C]에서 C를 보다 → 다음 = B, 이전 = B
	const int32 NewIndex = (CurrentIndex == INDEX_NONE)
		? 0
		: ((CurrentIndex + Direction) % Num + Num) % Num;

	SetSpectateTarget(Allies[NewIndex]);
}

void ULOASpectateComponent::SetSpectateTarget(APawn* NewTarget)
{
	APlayerController* PC = GetOwningPlayerController();
	APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!PC)
	{
		return;
	}

	SpectateTarget = NewTarget;

	// 보는 대상 = 아군 캐릭터 또는 내 몸. 이미 그걸 보고 있으면 다시 하지 않는다 (블렌드가 다시 시작되지 않게)
	AActor* ViewActor = NewTarget ? NewTarget : MyPawn;
	if (!ViewActor || PC->GetViewTarget() == ViewActor)
	{
		return;
	}

	PC->SetViewTargetWithBlend(ViewActor, BlendTime);

	LOA_LOG(PC, Log, TEXT("[Spectate] View target -> %s"), NewTarget ? *GetNameSafe(NewTarget) : TEXT("my body"));
}

// =============================================================================
// 도우미
// =============================================================================

void ULOASpectateComponent::GatherLivingAllies(TArray<APawn*>& OutAllies) const
{
	const APlayerController* PC = GetOwningPlayerController();
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!PC || !GameState)
	{
		return;
	}

	const APawn* MyPawn = PC->GetPawn();

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (Pawn && Pawn != MyPawn && !IsDead(Pawn))
		{
			OutAllies.Add(Pawn);
		}
	}
}

bool ULOASpectateComponent::IsDead(const APawn* Pawn)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	return ASC && ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead);
}

bool ULOASpectateComponent::CanSpectate() const
{
	const UWorld* World = GetWorld();
	return World && DeadSince >= 0.0 && World->GetTimeSeconds() - DeadSince >= StartDelay;
}

APlayerController* ULOASpectateComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}
