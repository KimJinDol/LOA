// LOALocationMarker.cpp

#include "Actors/LOALocationMarker.h"
#include "Game/LOARaidSubsystem.h"
#include "Core/LOALog.h"

#include "Components/SceneComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOALocationMarker)

ALOALocationMarker::ALOALocationMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 할 일은 등록뿐. 매 프레임 할 일 없음
	PrimaryActorTick.bCanEverTick = false;

	// 레벨에 미리 놓는 액터라 모든 컴퓨터에 있다. 복제할 것이 없다.
	bReplicates = false;

	// 게임 중에는 안 보인다
	SetHidden(true);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	EditorIcon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorIcon"));
	if (EditorIcon)
	{
		EditorIcon->SetupAttachment(RootComponent);
	}

	EditorArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EditorArrow"));
	if (EditorArrow)
	{
		EditorArrow->SetupAttachment(RootComponent);
	}
#endif
}

void ALOALocationMarker::BeginPlay()
{
	Super::BeginPlay();

	if (!LocationTag.IsValid())
	{
		LOA_LOG(this, Warning, TEXT("[Location] %s has no Location Tag. GA cannot find it."), *GetNameSafe(this));
		return;
	}

	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->RegisterLocation(this);
	}
}

void ALOALocationMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->UnregisterLocation(this);
	}

	Super::EndPlay(EndPlayReason);
}
