// LOABrawlBar.h
// [화면] 대난투 UI. 대난투 동안 보스 체력바 자리에 누적 피해·단계·단계 게이지·남은 시간을 보여 준다. (2026-11-18 apply125)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (내 화면만)
//   HUD가 BeginPlay에서 만들어 화면에 붙인다 (ALOAHUD의 Brawl Bar Class). 보스 체력바 숨기기는 HUD가 한다
//   NativeTick: 레이드 상황판에서 경기장(ALOABrawlArena)을 찾아
//     ShouldShowBrawlUI (진행 중 · 나오는 중) → 묶음 보임 + 값 채움 / 아니면 숨김
//   값은 경기장이 복제한 진행을 읽기만 한다. 남은 시간은 경기장이 서버 시각으로 계산한다
//   예) 2단계, 게이지 4,500 / 6,000, 누적 7,500, 남은 시간 12.3456초
//       → TotalDamageText "7,500" / 단계 칸 [가득][75%][잠금] / TimeText "12.346s"
//       3단계(마지막) → [가득][가득][보스 남은 체력 기준 게이지] (apply127), StageText(있으면) "마지막 단계"
//       협동 저가 모션 중 → 시간 멈춤 (경기장이 멈춘 값을 준다)
//
// [단계 칸] (2026-11-18 apply126, 사용자 그림: 로아 대난투처럼 단계가 눈에 띄게, 아직 못 간 단계는 잠금)
//   단계 수 = 경기장 GetStageCount (Stage Goals 개수 + 1). 그 수만큼 Stage Segment Class 위젯을 만들어 StageSegmentBox에 넣는다
//   칸마다: 지난 단계 가득 / 지금 단계 게이지 / 다음 단계 비고 잠금 (ULOABrawlStageSegment)
//   단계 수가 바뀌면(데이터 수정) 다시 만든다
//
// [WBP] WBP_BrawlBar (부모 = 이 클래스)
//   BrawlPanel       (아무 위젯, 선택) : 숨기고 보일 묶음. 없으면 글자·칸을 각각 숨긴다
//   TotalDamageText  (Text Block, 필수): 누적 피해 (화면 위 큰 숫자)
//   StageSegmentBox  (Horizontal Box 등 패널, 선택): 단계 칸을 넣을 줄
//   StageText        (Text Block, 선택): 단계 글자 ("{Stage}단계" / 마지막 단계 글자). 칸만 쓰면 안 넣어도 된다
//   TimeText         (Text Block, 선택): 남은 초 (소수 Time Fractional Digits 자리)
//   루트는 숨기지 않는다 (숨기면 Tick이 멈춘다. ULOAJustGuardHoldBar와 같음)
//   BP 기본값: Stage Segment Class (WBP_BrawlStageSegment), Stage Format / Last Stage Text / Time Format / Time Fractional Digits
//
// [네트워크] 없음. 이 컴퓨터에 복제된 경기장 진행을 읽기만 한다

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOABrawlBar.generated.h"

class UTextBlock;
class UWidget;
class UPanelWidget;
class ULOABrawlStageSegment;
class ALOABrawlArena;

UCLASS(Abstract)
class LOA_API ULOABrawlBar : public UUserWidget
{
	GENERATED_BODY()

public:
	ULOABrawlBar(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface
	/** 처음엔 숨김 */
	virtual void NativeConstruct() override;
	/** 매 프레임 경기장 진행을 읽어 채운다 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	/** 숨기고 보일 묶음 (배경 포함). 없으면 아래 위젯을 각각 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BrawlPanel;

	/** 누적 피해 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TotalDamageText;

	/** 단계 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StageText;

	/** 단계 칸을 넣을 줄 (헤더 [단계 칸]) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StageSegmentBox;

	/** 단계 칸 위젯 (WBP_BrawlStageSegment). 비어 있으면 칸 없음 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl")
	TSubclassOf<ULOABrawlStageSegment> StageSegmentClass;

	/** 남은 초 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeText;

	/** 단계 글자. {Stage} = 지금 단계 (1부터) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl")
	FText StageFormat;

	/** 마지막 단계 글자 (목표 = 보스 남은 체력) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl")
	FText LastStageText;

	/** 남은 시간 글자. {Seconds} = 남은 초 (소수 Time Fractional Digits 자리) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl")
	FText TimeFormat;

	/** 남은 초 소수 자리 (사용자 그림 "2.516s" → 3) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl", meta = (ClampMin = "0", ClampMax = "3"))
	int32 TimeFractionalDigits = 3;

private:
	/** 보이기 / 숨기기 (바뀔 때만) */
	void SetShown(bool bShow);

	/** 단계 칸: 수가 다르면 다시 만들고, 칸마다 가득 / 게이지 / 잠금 (헤더 [단계 칸]) */
	void UpdateStageSegments(const ALOABrawlArena& Arena);

	/** 만든 단계 칸 (1단계부터) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULOABrawlStageSegment>> StageSegments;

	bool bShown = true;
};
