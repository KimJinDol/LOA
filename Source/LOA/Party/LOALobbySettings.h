// LOALobbySettings.h
// [배관] 로비 규칙 설정: 이름 길이, 방 만들 레이드, 레이드 입장. Project Settings → Game → LOA Lobby. (값은 Config/DefaultGame.ini)
//   (2026-11-19 apply131, 2026-11-20 apply132 레이드 맵·파티 인원 → 레이드 데이터)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 여기에]
//   이름 길이는 로비 입력 칸과 파티장 컴퓨터의 신청 검사가 같은 숫자를 쓴다 → 한 곳.
//   ULOAPartySubsystem은 GameInstance 서브시스템이라 BP 기본값 칸이 없다 → 프로젝트 설정.
//   레이드마다 다른 것(이름·맵·파티 인원)은 여기가 아니라 레이드 데이터(ULOARaidData). 여기는 "방 만들 때 어느 레이드"만
//
// [Beacon 포트는 여기 없다]
//   엔진 설정 [/Script/OnlineSubsystemUtils.OnlineBeaconHost] ListenPort (BaseEngine.ini 기본 15000).
//   방 광고에 실제 포트를 실어 보내므로(SETTING_BEACONPORT) 지원자는 숫자를 몰라도 된다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LOALobbySettings.generated.h"

class ULOARaidData;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "LOA Lobby"))
class LOA_API ULOALobbySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Project Settings의 "Game" 묶음에 보이게 */
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 * 이름 최대 글자 수. 로비 입력 칸과 파티장 컴퓨터의 신청 검사가 같이 쓴다.
	 * 엔진이 접속 주소의 이름을 20자에서 자르므로(GameModeBase.cpp InitNewPlayer, Left(20)) 20까지.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Player Name", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxPlayerNameLength = 12;

	/**
	 * 방 만들기로 모집하는 레이드 (이름·맵·파티 인원은 이 데이터에서).
	 * 레이드를 고르는 화면이 생기기 전까지 하나. 예) DA_Raid_ReviveBeastHeart
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Party")
	TSoftObjectPtr<ULOARaidData> RaidData;

	/** 파티 한 줄 글 최대 글자 수 */
	UPROPERTY(Config, EditAnywhere, Category = "Party", meta = (ClampMin = "1"))
	int32 MaxPartyTitleLength = 30;

	/** 목록 검색 한 번에 받을 방 수 */
	UPROPERTY(Config, EditAnywhere, Category = "Party", meta = (ClampMin = "1"))
	int32 MaxSearchResults = 20;

	/**
	 * 파티원: 입장 신호를 받고 접속을 시작하기까지 기다리는 시간 (초).
	 * 파티장이 레이드 맵을 여는 동안 기다린다. 너무 일찍 붙어도 아래 다시 시도가 받쳐 준다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Enter Raid", meta = (ClampMin = "0.0", Units = "Seconds"))
	float EnterConnectDelay = 2.f;

	/** 파티원: 접속 실패 때 다시 시도하는 횟수 (처음 한 번은 빼고) */
	UPROPERTY(Config, EditAnywhere, Category = "Enter Raid", meta = (ClampMin = "0"))
	int32 EnterConnectRetryCount = 5;

	/** 파티원: 다시 시도 간격 (초) */
	UPROPERTY(Config, EditAnywhere, Category = "Enter Raid", meta = (ClampMin = "0.1", Units = "Seconds"))
	float EnterConnectRetryInterval = 2.f;
};
