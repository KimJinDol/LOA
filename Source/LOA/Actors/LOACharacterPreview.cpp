// LOACharacterPreview.cpp

#include "Actors/LOACharacterPreview.h"
#include "Data/LOACharacterClassData.h"
#include "Player/LOALocalPlayer.h"
#include "Core/LOALog.h"

#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACharacterPreview)

ALOACharacterPreview::ALOACharacterPreview()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;	// 로비는 혼자 쓰는 맵

	PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
	SetRootComponent(PreviewRoot);

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(PreviewRoot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALOACharacterPreview::BeginPlay()
{
	Super::BeginPlay();

	ULOALocalPlayer* LocalPlayer = nullptr;
	if (const UWorld* World = GetWorld())
	{
		LocalPlayer = Cast<ULOALocalPlayer>(World->GetFirstLocalPlayerFromController());
	}
	if (!LocalPlayer)
	{
		LOA_LOG(this, Warning, TEXT("[Lobby] No ULOALocalPlayer. Check LocalPlayerClassName in DefaultEngine.ini."));
		return;
	}

	ListenedLocalPlayer = LocalPlayer;
	LocalPlayer->OnSelectedClassChanged.AddUObject(this, &ThisClass::HandleSelectedClassChanged);

	// 위젯이 먼저 골랐을 수도 있다 (BeginPlay 순서는 정해져 있지 않음)
	if (const ULOACharacterClassData* AlreadySelected = LocalPlayer->GetSelectedClassData())
	{
		ShowClass(AlreadySelected);
	}
}

void ALOACharacterPreview::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// LocalPlayer는 맵이 바뀌어도 남는다 → 듣기를 끊지 않으면 사라진 액터를 부른다
	if (ULOALocalPlayer* LocalPlayer = ListenedLocalPlayer.Get())
	{
		LocalPlayer->OnSelectedClassChanged.RemoveAll(this);
	}
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void ALOACharacterPreview::HandleSelectedClassChanged(const ULOACharacterClassData* ClassData)
{
	ShowClass(ClassData);
}

void ALOACharacterPreview::ShowClass(const ULOACharacterClassData* ClassData)
{
	if (!ClassData)
	{
		return;
	}

	ShownClassData = ClassData;

	// 앞에서 불러오던 직업은 취소 (빠르게 여러 번 눌러도 마지막 것만)
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	TArray<FSoftObjectPath> PathsToLoad;
	if (!ClassData->SkeletalMesh.IsNull())
	{
		PathsToLoad.Add(ClassData->SkeletalMesh.ToSoftObjectPath());
	}
	if (!ClassData->LobbyAnimation.IsNull())
	{
		PathsToLoad.Add(ClassData->LobbyAnimation.ToSoftObjectPath());
	}

	if (PathsToLoad.IsEmpty())
	{
		ApplyLoadedClass(ClassData);
		return;
	}

	// 다 오면 ApplyLoadedClass. 이미 메모리에 있으면 엔진이 바로 부른다
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(PathsToLoad,
		FStreamableDelegate::CreateUObject(this, &ThisClass::ApplyLoadedClass, ClassData));
}

void ALOACharacterPreview::ApplyLoadedClass(const ULOACharacterClassData* ClassData)
{
	// 그사이 다른 직업을 골랐으면 버린다
	if (!ClassData || ClassData != ShownClassData || !PreviewMesh)
	{
		return;
	}

	PreviewMesh->SetSkeletalMeshAsset(ClassData->SkeletalMesh.Get());
	PreviewMesh->SetRelativeTransform(ClassData->MeshRelativeTransform);

	if (UAnimSequence* LobbyAnimation = ClassData->LobbyAnimation.Get())
	{
		// 애님 BP 없이 동작 하나만 반복 (Single Node)
		PreviewMesh->PlayAnimation(LobbyAnimation, true);
	}
	else
	{
		PreviewMesh->Stop();
		LOA_LOG(this, Warning, TEXT("[Lobby] [%s] has no LobbyAnimation. Preview shows the default pose."), *GetNameSafe(ClassData));
	}
}
