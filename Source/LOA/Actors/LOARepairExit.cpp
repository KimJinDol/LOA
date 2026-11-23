// LOARepairExit.cpp

#include "Actors/LOARepairExit.h"
#include "Game/LOAGameState.h"
#include "Character/Player/LOACharacter.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARepairExit)

ALOARepairExit::ALOARepairExit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 겹침 알림만 받는다. 매 프레임 할 일 없음
	PrimaryActorTick.bCanEverTick = false;

	// 레벨에 미리 놓는 액터라 모든 컴퓨터에 있다. 복제할 것이 없다.
	bReplicates = false;

	ExitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitBox"));
	ExitBox->InitBoxExtent(FVector(300.f, 300.f, 200.f));
	ExitBox->SetCollisionProfileName(TEXT("Trigger"));	// 모든 것과 겹치기만 (막지 않음)
	ExitBox->SetGenerateOverlapEvents(true);
	ExitBox->SetHiddenInGame(true);
	ExitBox->SetCanEverAffectNavigation(false);
	RootComponent = ExitBox;
}

void ALOARepairExit::BeginPlay()
{
	Super::BeginPlay();

	// 판단은 서버만. 클라에서는 알림을 묶지 않는다
	if (HasAuthority())
	{
		ExitBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
		ExitBox->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
	}
}

bool ALOARepairExit::IsCharacterInside(const AActor* Character) const
{
	return Character && ExitBox && ExitBox->IsOverlappingActor(Character);
}

void ALOARepairExit::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어 몸만 (장판·보스 등은 무시)
	if (Cast<ALOACharacter>(OtherActor))
	{
		RequestCheck();
	}
}

void ALOARepairExit::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Cast<ALOACharacter>(OtherActor))
	{
		RequestCheck();
	}
}

void ALOARepairExit::RequestCheck() const
{
	if (ALOAGameState* LOAGameState = GetWorld() ? GetWorld()->GetGameState<ALOAGameState>() : nullptr)
	{
		LOAGameState->CheckRepairExit();
	}
}
