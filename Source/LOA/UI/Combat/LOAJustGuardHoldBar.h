// LOAJustGuardHoldBar.h
// [화면] 홀딩 저스트 가드 게이지. 홀딩 구간에서 G를 누르고 있는 동안 "첫 타부터 구간 끝까지 몇 %가 지났나"를 막대로 보여 준다. (2026-11-05)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (내 화면만)
//   HUD가 BeginPlay에서 만들어 화면에 붙인다 (ALOAHUD의 Just Guard Hold Bar Class)
//   NativeTick: 내 캐릭터의 ULOAJustGuardComponent::GetHoldProgress
//     true  → 게이지 보임 + 진행률 (0 ~ 1)
//     false → 게이지 숨김 (홀딩이 아님 / 첫 타 전 / 손을 뗌 / 구간 끝)
//
// [진행률] 첫 타 기준 (사용자 결정 2026-11-08. 전에는 "열림" 받은 순간 기준)
//   0% = 이 구간의 타격이 내 화면에서 처음 닿은 순간, 100% = 구간 끝 (이 컴퓨터가 "열림"을 받은 순간 + 구간 길이)
//   구간 길이 = 보스 몽타주의 저가 구간 노티파이 길이 ÷ 몽타주 재생 속도 (서버가 "열림"과 같이 보냄)
//   시계는 전부 이 컴퓨터의 게임 시각 (내 화면 UI라 서버 시각 추정을 쓰지 않는다)
//   예) 구간 10.0 ~ 13.0, 첫 타 10.5 → 첫 타 전엔 안 보임, 10.5에 0%로 나타나 13.0에 100%
//
// [WBP] WBP_JustGuardHoldBar (부모 = 이 클래스)
//   HoldBar   (Progress Bar, 필수)  : 게이지
//   HoldPanel (아무 위젯, 선택)      : 숨기고 보일 묶음 (배경·글자까지 같이). 없으면 HoldBar만 숨긴다
//   HoldText  (Text Block, 선택)     : "53%" 같은 글자
//   루트는 숨기지 않는다 (숨기면 Tick이 멈춘다)
//
// [네트워크] 없음. 내 컴퓨터의 장부만 읽는다

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOAJustGuardHoldBar.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;
class ULOAJustGuardComponent;
class APawn;

UCLASS(Abstract)
class LOA_API ULOAJustGuardHoldBar : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	/** 처음엔 숨김 */
	virtual void NativeConstruct() override;
	/** 매 프레임 내 장부의 홀딩 진행률을 읽어 게이지에 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	/** 게이지 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HoldBar;

	/** 숨기고 보일 묶음 (배경·글자 포함). 없으면 HoldBar만 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HoldPanel;

	/** "53%" 같은 글자 (선택) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HoldText;

private:
	/** 내 캐릭터의 저가 장부 (캐릭터가 바뀌면 다시 찾는다) */
	ULOAJustGuardComponent* FindMyJustGuardComponent();

	/** 보이기 / 숨기기 (바뀔 때만) */
	void SetShown(bool bShow);

	TWeakObjectPtr<ULOAJustGuardComponent> CachedJustGuardComp;
	TWeakObjectPtr<APawn> CachedPawn;
	bool bShown = true;
};
