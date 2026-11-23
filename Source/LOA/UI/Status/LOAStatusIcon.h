// LOAStatusIcon.h
// [화면] 상태 아이콘 한 칸. 아이콘 + (나·보스만) 남은 초. (2026-11-10)
//
// [WBP] WBP_StatusIcon (부모 = 이 클래스)
//   IconImage (Image, 필수)        : 아이콘
//   TimeText  (TextBlock, 선택)    : 남은 초 (정수, 올림). 시간을 모르면 숨김
//   Frame     (Border, 선택)       : 테두리. 버프 = BuffFrameColor, 디버프 = DebuffFrameColor
//
// 남은 초는 매 프레임 Tracker에 물어본다 (GE 시간을 그대로 읽으므로 같은 버프를 다시 받아 시간이 새로 고쳐져도 맞다)
//
// [툴팁] (2026-11-14) Setup에서 상태가 바뀌었을 때만 채운다 (ULOATooltipWidget::SetStatusTooltip). 이름 + 설명
//   Class Defaults: Tooltip Widget Class = WBP_Tooltip. 비우면 툴팁 없음
//   마우스를 받아야 툴팁이 뜬다 → 줄(ULOAStatusIconRow)이 이 위젯을 Visible로 둔다

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "LOAStatusIcon.generated.h"

class UImage;
class UTextBlock;
class UBorder;
class ULOAStatusTracker;
class ULOATooltipWidget;
struct FLOAStatusInfo;

UCLASS(Abstract)
class LOA_API ULOAStatusIcon : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 무엇을 보여 줄지. bShowTime이면 Tracker에서 남은 시간을 읽어 TimeText에 */
	void Setup(const FLOAStatusInfo& Info, ULOAStatusTracker* InTracker, bool bInShowTime);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Frame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Status")
	FLinearColor BuffFrameColor = FLinearColor(0.85f, 0.7f, 0.3f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Status")
	FLinearColor DebuffFrameColor = FLinearColor(0.8f, 0.15f, 0.15f, 1.f);

	/** 툴팁 위젯 (WBP_Tooltip). 비우면 툴팁 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Status")
	TSubclassOf<ULOATooltipWidget> TooltipWidgetClass;

private:
	/** 남은 초를 글자로 (모르면 숨김) */
	void RefreshTime();

	FGameplayTag StatusTag;
	TWeakObjectPtr<ULOAStatusTracker> Tracker;
	bool bShowTime = false;

	/** 마지막으로 쓴 초 (같으면 글자를 다시 만들지 않음) */
	int32 LastShownSeconds = -1;
};
