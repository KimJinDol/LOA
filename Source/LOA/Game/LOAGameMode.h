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
// [직업은 어디서 정하나 - 지금은 임시]
// 직업 선택 화면(로비)이 아직 없으므로, BP에 넣어 둔 TestClasses 목록을 "접속 순서대로" 나눠 준다.
//   1번째 접속 → TestClasses[0], 2번째 → [1], ... 목록 끝이면 처음으로 돌아감
// 로비가 생기면 ChooseClassData()만 바꾸면 된다. (스폰 흐름은 그대로)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] BP_LOAGameMode (부모: LOAGameMode)
//   Default Pawn Class      = BP_LOAHero
//   Player Controller Class = BP_LOAPlayerController
//   Player State Class      = BP_LOAPlayerState
//   Test Classes            = [ DA_Class_Berserker, ... ]

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LOAGameMode.generated.h"

class ULOACharacterClassData;

UCLASS()
class LOA_API ALOAGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALOAGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AGameModeBase Interface
	/** 플레이어 캐릭터 스폰. 스폰이 끝나기 전에 직업 데이터를 넣는다. [서버] */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/** 플레이어가 나갈 때 배정 기록 정리 [서버] */
	virtual void Logout(AController* Exiting) override;
	//~ End AGameModeBase Interface

protected:
	/**
	 * 이 플레이어의 직업을 정한다. [서버]
	 * - 이미 배정된 적이 있으면(부활) 같은 직업을 돌려준다. (맵 안에서 직업은 바뀌지 않음)
	 * - 처음이면 TestClasses에서 접속 순서대로 고른다.
	 * 로비가 생기면 이 함수만 바꾸면 된다.
	 */
	virtual const ULOACharacterClassData* ChooseClassData(AController* Player);

private:
	/** 테스트용 직업 목록. 접속 순서대로 나눠 준다. (로비가 생기기 전까지 임시) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Class")
	TArray<TObjectPtr<const ULOACharacterClassData>> TestClasses;

	/** 컨트롤러별로 배정한 직업. 부활할 때 같은 직업으로 스폰하기 위해 기억한다. [서버 전용] */
	UPROPERTY(Transient)
	TMap<TObjectPtr<AController>, TObjectPtr<const ULOACharacterClassData>> AssignedClasses;

	/** 다음 신규 플레이어에게 줄 TestClasses 번호 */
	int32 NextClassIndex = 0;
};
