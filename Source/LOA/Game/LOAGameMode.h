// LOAGameMode.h
// [배관] 게임 규칙 담당. 플레이어가 들어오면 캐릭터를 스폰하고 직업 데이터를 넣어 준다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [GameMode는 어디에 있나]
// 서버에만 존재한다. 클라이언트에는 GameMode가 없다. (클라이언트에서 GetAuthGameMode()는 nullptr)
// 그래서 "누가 어떤 직업인가" 같은 게임 규칙 결정은 전부 여기서, 서버에서만 한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [스폰 흐름]
//
//   플레이어 접속 (또는 부활)
//      │ 엔진이 호출
//      ▼
//   SpawnDefaultPawnFor()                    ← 이 함수를 오버라이드
//      ├─ ① 이 플레이어의 직업 결정 (ChooseClassData)
//      ├─ ② 캐릭터 스폰 "시작" (지연 스폰: 아직 BeginPlay 전)
//      ├─ ③ SetCharacterClassData(직업)      ← 스폰이 끝나기 전에 넣는다
//      └─ ④ 스폰 "완료" (FinishSpawning)
//      │
//      ▼ 엔진이 이어서 빙의(Possess) → 캐릭터의 PossessedBy → 택배 A 도착 → 초기화 완료
//
// ③을 ④보다 먼저 하는 이유:
//   스폰이 끝나는 순간 캐릭터가 클라이언트로 처음 복제된다.
//   그 전에 직업을 넣어 두면 "첫 복제 패킷"에 직업이 함께 실려서, 클라이언트에 직업 없는 맨몸이 잠깐 보이는 일이 줄어든다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [직업은 어디서 정하나] (2026-11-14 apply113: 로비)
//   ① 로비를 거쳐 들어옴: 접속 주소의 ?Class=DA_Class_Berserker (ULOALocalPlayer가 붙임)
//      InitNewPlayer(엔진이 접속 처리 중, 스폰보다 먼저 부름)에서 읽어 검사 → AssignedClasses에 적어 둠
//   ② 주소에 직업이 없음 (PIE 다인처럼 로비를 거치지 않음) 또는 잘못된 이름: TestClasses를 접속 순서대로
//      1번째 → TestClasses[0], 2번째 → [1], ... 목록 끝이면 처음으로 돌아감
//   스폰은 지금처럼 ChooseClassData가 AssignedClasses에서 꺼낸다 (부활도 같은 표)
//
//   엔진 순서: Login → InitNewPlayer(Options) → PostLogin → … → SpawnDefaultPawnFor  (GameModeBase.cpp)
//   데디케이티드 서버도 같다 (서버에 LocalPlayer가 없어도 주소는 원격 클라가 보냄)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] BP_LOAGameMode (부모: LOAGameMode)
//   Default Pawn Class      = BP_LOAHero
//   Player Controller Class = BP_LOAPlayerController
//   Player State Class      = BP_LOAPlayerState
//   Game State Class        = LOAGameState (C++ 기본값. 공용 부활 목숨)
//   Test Classes            = [ DA_Class_Berserker, ... ]

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LOAGameMode.generated.h"

class ULOACharacterClassData;
struct FLOACharacterSelection;

UCLASS()
class LOA_API ALOAGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALOAGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AGameModeBase Interface
	/** 플레이어 캐릭터 스폰. 스폰이 끝나기 전에 직업 데이터를 넣는다. [서버] */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/**
	 * 새 플레이어 접속 처리 중 (스폰 전). 접속 주소의 직업을 읽어 AssignedClasses에 적는다. [서버]
	 * 주소에 없거나 잘못된 이름이면 적지 않는다 → 스폰 때 TestClasses
	 */
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;

	/** 플레이어가 나갈 때 배정 기록 정리 [서버] */
	virtual void Logout(AController* Exiting) override;
	//~ End AGameModeBase Interface

protected:
	/**
	 * 이 플레이어의 직업을 정한다. [서버]
	 * - 이미 배정된 적이 있으면(부활) 같은 직업을 돌려준다. (맵 안에서 직업은 바뀌지 않음)
	 * - 로비에서 고르고 들어왔으면 InitNewPlayer가 이미 적어 두었다 → 그 직업.
	 * - 아니면 TestClasses에서 접속 순서대로 고른다.
	 */
	virtual const ULOACharacterClassData* ChooseClassData(AController* Player);

private:
	/**
	 * 테스트용 직업 목록. 로비를 거치지 않은 접속(PIE 다인 등)에 접속 순서대로 나눠 준다.
	 * (2026-11-14) 로비가 생겨도 남긴다: PIE 여러 창은 로비 없이 바로 레이드 맵으로 들어온다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Class")
	TArray<TObjectPtr<const ULOACharacterClassData>> TestClasses;

	/** 컨트롤러별로 배정한 직업. 부활할 때 같은 직업으로 스폰하기 위해 기억한다. [서버 전용] */
	UPROPERTY(Transient)
	TMap<TObjectPtr<AController>, TObjectPtr<const ULOACharacterClassData>> AssignedClasses;

	/** 다음 신규 플레이어에게 줄 TestClasses 번호 */
	int32 NextClassIndex = 0;

	/** 접속 주소에서 받은 선택으로 직업 데이터를 찾는다. 종류·클래스가 맞지 않으면 nullptr [서버] */
	const ULOACharacterClassData* FindSelectedClassData(const FLOACharacterSelection& Selection) const;
};
