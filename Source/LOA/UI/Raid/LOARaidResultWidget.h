// LOARaidResultWidget.h
// [화면] 레이드 클리어 결과. ① 클리어 배너 몇 초 → ② MVP 화면 + "나가기"(로비로) (사용자 결정 2026-11-15, apply115)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] 전부 이 컴퓨터 안 (결과는 GameState가 복제해 둔 것을 읽기만)
//   ALOAHUD: 레이드 단계 = 클리어 → 이 위젯을 만들어 ShowResult(GameState의 결과) → 화면에
//   ShowResult  : 배너(보스 얼굴, "N분 N초 클리어 성공")를 보이고 BannerSeconds 타이머 → BP OnBannerShown (연출)
//   타이머 끝   : 배너를 접고 결과 화면을 보임. MVP 카드 + 나머지 사람 카드. 입력 = UI만 (게임 입력 막힘, 대신 정함) → BP OnResultPanelShown
//   나가기      : LeaveButton 또는 Esc → 기본 맵(Project Settings → Maps & Modes → Game Default Map = L_Lobby)을 연다
//
//   예) 3명, MVP = 세라스 → MvpCard = 세라스 / OtherCardsBox에 버서커A, 버서커B 카드 (PlayerArray 순서)
//
// [로비로 가는 길이 하나인 이유] (엔진 소스 확인 2026-11-15, UnrealEngine.cpp)
//   방장(리슨 서버)이 먼저 나가면 남은 사람은 연결이 끊긴다 → HandleNetworkFailure → HandleDisconnect → "?closed" → Browse가 Game Default Map을 연다.
//   "나가기"도 같은 Game Default Map을 열게 해서 로비 맵 이름을 따로 적지 않는다. (PIE는 에디터 쪽 처리가 따로라 테스트로 확인)
//
// [WBP] WBP_RaidResult (부모 = 이 클래스)
//   BannerPanel       (Widget, 필수)    : 배너 묶음. 처음에 보임
//   ClearTimeText     (TextBlock, 필수) : 클리어 기록 (ClearTimeFormat)
//   BossPortraitImage (Image, 선택)     : 보스 얼굴 (보스의 Clear Portrait. 비어 있으면 숨김)
//   BossNameText      (TextBlock, 선택) : 보스 이름
//   ResultPanel       (Widget, 필수)    : 결과 화면 묶음. 처음에 접힘
//   MvpCard           (WBP_RaidResultMvpCard, 필수) : MVP 한 명
//   OtherCardsBox     (가로·세로 상자 등 Panel, 필수) : 나머지 사람 카드가 들어갈 곳 (OtherCardClass로 만들어 넣음)
//   LeaveButton       (Button, 필수)    : 나가기[Esc]
//   클래스 기본값: Other Card Class = WBP_RaidResultCard, Banner Seconds, Clear Time Format
//
// [네트워크] 3 각자 (결과 화면, 나가기). 결과 값은 2 복제(ALOAGameState::ClearResult)로 이미 와 있다

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/LOARaidResultTypes.h"
#include "LOARaidResultWidget.generated.h"

class UButton;
class UImage;
class UPanelWidget;
class UTextBlock;
class ULOARaidResultCardWidget;

UCLASS(Abstract)
class LOA_API ULOARaidResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 결과로 채우고 배너부터 보인다 */
	void ShowResult(const FLOARaidClearResult& Result);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	/** 결과 화면에서 Esc → 나가기 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	/** [BP] 배너가 보이기 시작함 (빛·애니 연출) */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Raid")
	void OnBannerShown();

	/** [BP] 결과 화면이 보이기 시작함 (연출) */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Raid")
	void OnResultPanelShown();

	// ---------- 설정 (WBP 기본값) ----------
	/** 나머지 사람 카드 (WBP_RaidResultCard) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid")
	TSubclassOf<ULOARaidResultCardWidget> OtherCardClass;

	/** 배너만 보이는 시간 (초). 지나면 결과 화면 (대신 정함 4초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid", meta = (ClampMin = "0.0", Units = "Seconds"))
	float BannerSeconds = 4.f;

	/**
	 * 클리어 기록 글자. {Minutes} = 분, {Seconds} = 초 (두 자리). 예) "{Minutes}분 {Seconds}초 클리어 성공" → "3분 42초 클리어 성공"
	 * 비어 있으면 "{Minutes}:{Seconds}" (WBP에서 채운다)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid")
	FText ClearTimeFormat;

	// ---------- 위젯 (헤더 [WBP]) ----------
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> BannerPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClearTimeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BossPortraitImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ResultPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULOARaidResultCardWidget> MvpCard;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> OtherCardsBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> LeaveButton;

private:
	/** 배너를 접고 결과 화면을 보인다. 입력 = UI만 */
	void ShowResultPanel();

	/** 기본 맵(로비)을 연다 */
	UFUNCTION()
	void Leave();

	FTimerHandle BannerTimerHandle;

	/** 결과 화면이 보이는 중인가 (Esc는 이때만) */
	bool bResultPanelShown = false;
};
