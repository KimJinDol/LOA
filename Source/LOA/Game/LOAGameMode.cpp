// LOAGameMode.cpp

#include "Game/LOAGameMode.h"
#include "Character/Player/LOACharacter.h"
#include "Data/LOACharacterClassData.h"
#include "Core/LOALog.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGameMode)

ALOAGameMode::ALOAGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Pawn / PlayerController / PlayerState 클래스는 여기서 정하지 않는다.
	// 설정값(IMC, InputConfig 등)은 BP 쪽에 있으므로 BP_LOAGameMode에서 BP 클래스들을 지정한다.
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

const ULOACharacterClassData* ALOAGameMode::ChooseClassData(AController* Player)
{
	// 부활: 이전에 배정한 직업을 그대로 돌려준다.
	if (const TObjectPtr<const ULOACharacterClassData>* Found = AssignedClasses.Find(Player))
	{
		return *Found;
	}

	if (TestClasses.IsEmpty())
	{
		return nullptr;
	}

	// 신규: 접속 순서대로 목록에서 하나씩. 끝까지 가면 처음으로 돌아간다.
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
