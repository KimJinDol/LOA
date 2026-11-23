// LOACharacterSelection.h
// [데이터] 플레이어가 로비에서 고른 것. "누구로 접속하나" 한 묶음. (2026-11-14 apply113)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   직업 + 이름 (이름은 2026-11-19 apply131, 파티 찾기). 카오스던전 레벨업이 생기면 칸이 늘어난다 (예: 레벨).
//   칸이 늘어도 보내는 곳(ULOALocalPlayer)·받는 곳(ALOAGameMode)은 이 구조체만 주고받으므로 그대로다.
//
// [흐름]
//   로비 카드 클릭 → ULOALocalPlayer::SetSelectedClass → 이 구조체에 적어 둠
//   접속할 때 엔진이 묻는다 (ULocalPlayer::GetGameLoginOptions) → ToLoginOptions() = "Class=DA_Class_Berserker"
//     엔진이 접속 주소 뒤에 붙여 보낸다 → 127.0.0.1?Name=진돌?Class=DA_Class_Berserker
//     (호스트 본인: LocalPlayer.cpp SpawnPlayActor / 원격: PendingNetGame.cpp 로그인 보낼 때. UE 5.5 소스 확인)
//   서버 GameMode::InitNewPlayer(Options) → FromLoginOptions(Options) → 직업 찾기·검사 → 배정
//   글자로 바꾸기·읽기는 이 파일 한 곳에서만 한다 (보내는 쪽과 받는 쪽이 같은 함수를 쓴다)
//
// [이름은 엔진 칸으로 간다] (apply131, UE 5.5 소스 확인)
//   엔진이 접속 주소에 ?Name=을 스스로 붙인다 (LocalPlayer.cpp / PendingNetGame.cpp: GetNickname())
//   → ULOALocalPlayer::GetNickname()이 PlayerName을 돌려준다. 서버는 엔진이 PlayerState 이름으로 넣는다
//     (GameModeBase.cpp InitNewPlayer: ParseOption "Name" → Left(20) → ChangeName). 레이드 결과 화면 이름도 이것
//   그래서 ToLoginOptions에는 이름을 넣지 않는다 (두 번 보내지 않음)
//   이름 규칙(길이·못 쓰는 글자)은 MakeValidPlayerName 한 곳. 로비 입력 칸과 파티장 컴퓨터의 신청 검사가 같이 쓴다
//
// [직업은 에셋 이름으로 보낸다]
//   ClassId = 에셋 매니저 ID (CharacterClass:DA_Class_Berserker). 주소에는 이름 부분만.
//   서버는 이름으로 경로를 바로 찾는다 (직업 태그로 보내면 모든 직업 데이터를 불러와 비교해야 함)
//
// [네트워크]
//   로그인 주소로 한 번 간다 (RPC 없음). 레이드 안에서 바뀌지 않는다 (맵 안에서 직업은 바뀌지 않음 규칙).
//   믿지 않는다: 서버가 종류(CharacterClass)와 클래스(ULOACharacterClassData)를 검사한 뒤에만 쓴다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "LOACharacterSelection.generated.h"

USTRUCT(BlueprintType)
struct LOA_API FLOACharacterSelection
{
	GENERATED_BODY()

	/** 고른 직업 데이터의 에셋 매니저 ID (CharacterClass:DA_Class_Berserker). 비면 아직 안 고름 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Lobby")
	FPrimaryAssetId ClassId;

	/** 로비에서 정한 이름. 비면 아직 안 정함 (엔진 기본 이름이 쓰인다) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Lobby")
	FString PlayerName;

	/** 직업을 골랐나 */
	bool IsValid() const { return ClassId.IsValid(); }

	/**
	 * 이름 규칙에 맞게 고친다: 앞뒤 공백 빼기 → 접속 주소를 깨는 글자 빼기(공백 ? = # / \ : ") → 최대 글자 수(LOA Lobby 설정)로 자르기.
	 * 예) "  인생 운?  " → "인생운". 남는 글자가 없으면 빈 글자
	 */
	static FString MakeValidPlayerName(const FString& InName);

	/** 접속 주소에 붙일 글자. 예) "Class=DA_Class_Berserker". 안 골랐으면 빈 글자 */
	FString ToLoginOptions() const;

	/** 서버: 접속 주소(Options)에서 읽는다. Class가 없으면 빈 선택 (PIE 다인처럼 로비를 거치지 않은 접속) */
	static FLOACharacterSelection FromLoginOptions(const FString& Options);
};
