// LOAGameMode.cpp

#include "Game/LOAGameMode.h"
#include "Game/LOAGameState.h"
#include "Character/Player/LOACharacter.h"
#include "Data/LOACharacterClassData.h"
#include "Data/LOACharacterSelection.h"
#include "Core/LOALog.h"

#include "Engine/AssetManager.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGameMode)

ALOAGameMode::ALOAGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Pawn / PlayerController / PlayerState 클래스는 여기서 정하지 않는다.
	// 설정값(IMC, InputConfig 등)은 BP 쪽에 있으므로 BP_LOAGameMode에서 BP 클래스들을 지정한다.

	// GameState는 설정할 값이 없어서 C++ 클래스를 기본값으로 둔다. (공용 부활 목숨)
	GameStateClass = ALOAGameState::StaticClass();
}

APawn* ALOAGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	UWorld* World = GetWorld();
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);

	if (!World || !PawnClass || !StartSpot)
	{
		// 필요한 게 없으면 엔진 기본 동작에 맡긴다.
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	// 스폰 위치: PlayerStart의 위치 + 좌우 방향(Yaw)만 사용 (엔진 기본 동작과 동일)
	const FRotator StartRotation(0.f, StartSpot->GetActorRotation().Yaw, 0.f);
	const FTransform SpawnTransform(StartRotation, StartSpot->GetActorLocation());

	FActorSpawnParameters SpawnParams;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.ObjectFlags |= RF_Transient;     // 레벨에 저장되지 않는 런타임 액터
	SpawnParams.bDeferConstruction = true;       // ② 스폰 "시작"만 한다. (BeginPlay 전 상태로 멈춤
	
	// 액터가 스폰될 위치에 다른 물체(벽, 캐릭터, 장애물 등)가 겹쳐 있을 때, 엔진이 어떻게 충돌을 처리하고 스폰할지 지정하는 옵션
	// 스폰하려는 위치에 충돌이 발생하면, 엔진이 주변을 탐색하여 겹치지 않고 스폰할 수 있는 가장 가까운 빈 공간으로 위치를 약간 조정(Adjust)함.
	// 만약 주변에 마땅한 빈 공간을 찾지 못하더라도, 무조건(Always) 그 자리에 겹친 채로 액터를 스폰시킵니다.
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* NewPawn = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams);
	if (!NewPawn)
	{
		LOA_LOG(this, Error, TEXT("Failed to spawn pawn [%s]."), *GetNameSafe(PawnClass));
		return nullptr;
	}

	// ①③ 직업을 정해서 스폰이 끝나기 전에 넣는다. → 첫 복제 패킷에 직업이 함께 실림
	if (ALOACharacter* LOACharacter = Cast<ALOACharacter>(NewPawn))
	{
		if (const ULOACharacterClassData* ClassData = ChooseClassData(NewPlayer))
		{
			LOACharacter->SetCharacterClassData(ClassData);
		}
		else
		{
			LOA_LOG(this, Warning, TEXT("No class data for [%s]. Check TestClasses on %s."),
				*GetNameSafe(NewPlayer), *GetNameSafe(GetClass()));
		}
	}

	// ④ 스폰 완료 → BeginPlay, 복제 시작. 이후 엔진이 빙의(Possess)를 이어서 한다.
	NewPawn->FinishSpawning(SpawnTransform);

	return NewPawn;
}

FString ALOAGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ErrorMessage.IsEmpty())
	{
		return ErrorMessage;
	}

	// 로비를 거쳐 왔으면 주소에 ?Class=DA_Class_Berserker 가 있다 (ULOALocalPlayer::GetGameLoginOptions)
	const FLOACharacterSelection Selection = FLOACharacterSelection::FromLoginOptions(Options);
	if (!Selection.IsValid())
	{
		// 로비를 거치지 않은 접속 (PIE 다인 등) → 스폰 때 TestClasses
		return ErrorMessage;
	}

	if (const ULOACharacterClassData* ClassData = FindSelectedClassData(Selection))
	{
		// 스폰(ChooseClassData)이 이 표에서 꺼낸다
		AssignedClasses.Add(NewPlayerController, ClassData);
		LOA_LOG(this, Log, TEXT("Assigned class [%s] to [%s] from login options."), *GetNameSafe(ClassData), *GetNameSafe(NewPlayerController));
	}
	else
	{
		// 접속은 막지 않는다. 직업만 TestClasses로
		LOA_LOG(this, Warning, TEXT("Unknown class [%s] in login options from [%s]. Falling back to TestClasses."),
			*Selection.ClassId.ToString(), *GetNameSafe(NewPlayerController));
	}
	return ErrorMessage;
}

const ULOACharacterClassData* ALOAGameMode::FindSelectedClassData(const FLOACharacterSelection& Selection) const
{
	// 클라가 보낸 글자라 믿지 않는다: 에셋 매니저 목록에 있는 CharacterClass이고, 정말 직업 데이터인 것만
	if (!Selection.IsValid() || Selection.ClassId.PrimaryAssetType != ULOACharacterClassData::GetPrimaryAssetTypeName() || !UAssetManager::IsInitialized())
	{
		return nullptr;
	}

	// 목록에 없는 이름이면 빈 경로 (DefaultGame.ini에 CharacterClass 등록이 필요)
	const FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(Selection.ClassId);
	if (AssetPath.IsNull())
	{
		return nullptr;
	}

	// 직업 데이터는 작고 TestClasses가 이미 들고 있어 대개 메모리에 있다 → 바로 불러온다
	return Cast<ULOACharacterClassData>(AssetPath.TryLoad());
}

const ULOACharacterClassData* ALOAGameMode::ChooseClassData(AController* Player)
{
	// 부활 또는 로비에서 고르고 들어옴(InitNewPlayer가 적어 둠): 적어 둔 직업을 그대로 돌려준다.
	if (const TObjectPtr<const ULOACharacterClassData>* Found = AssignedClasses.Find(Player))
	{
		return *Found;
	}

	if (TestClasses.IsEmpty())
	{
		return nullptr;
	}

	// 로비를 거치지 않은 신규: 접속 순서대로 목록에서 하나씩. 끝까지 가면 처음으로 돌아간다.
	const ULOACharacterClassData* Chosen = TestClasses[NextClassIndex % TestClasses.Num()];
	++NextClassIndex;

	AssignedClasses.Add(Player, Chosen);

	LOA_LOG(this, Log, TEXT("Assigned class [%s] to [%s]."), *GetNameSafe(Chosen), *GetNameSafe(Player));
	return Chosen;
}

void ALOAGameMode::Logout(AController* Exiting)
{
	// 나간 플레이어의 배정 기록 정리 (컨트롤러가 사라진 뒤 남는 기록 방지)
	AssignedClasses.Remove(Exiting);

	Super::Logout(Exiting);
}
