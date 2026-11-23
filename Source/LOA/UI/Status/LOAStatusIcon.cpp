// LOAStatusIcon.cpp

#include "UI/Status/LOAStatusIcon.h"
#include "UI/Status/LOAStatusInfoData.h"
#include "UI/Status/LOAStatusTracker.h"
#include "UI/LOATooltipWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStatusIcon)

void ULOAStatusIcon::Setup(const FLOAStatusInfo& Info, ULOAStatusTracker* InTracker, bool bInShowTime)
{
	// 툴팁: 이 칸에 보이는 상태가 바뀔 때만 다시 채운다 (줄은 태그가 바뀔 때마다 모든 칸의 Setup을 부른다)
	if (TooltipWidgetClass && Info.StatusTag != StatusTag)
	{
		ULOATooltipWidget::SetStatusTooltip(this, TooltipWidgetClass, Info);
	}

	StatusTag = Info.StatusTag;
	Tracker = InTracker;
	bShowTime = bInShowTime;
	LastShownSeconds = -1;

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(Info.Icon);
	}
	if (Frame)
	{
		Frame->SetBrushColor(Info.IsDebuff() ? DebuffFrameColor : BuffFrameColor);
	}
	RefreshTime();
}

void ULOAStatusIcon::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshTime();
}

void ULOAStatusIcon::RefreshTime()
{
	if (!TimeText)
	{
		return;
	}

	float TimeRemaining = 0.f;
	float Duration = 0.f;
	const ULOAStatusTracker* MyTracker = Tracker.Get();
	const bool bHasTime = bShowTime && MyTracker && MyTracker->GetStatusTimeRemaining(StatusTag, TimeRemaining, Duration);
	if (!bHasTime)
	{
		// 남의 버프, 무한 효과, 시간 안 보이기 → 숨김
		LastShownSeconds = -1;
		TimeText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 올림: 0.3초 남았으면 1
	const int32 Seconds = FMath::CeilToInt(TimeRemaining);
	if (Seconds != LastShownSeconds)
	{
		LastShownSeconds = Seconds;
		TimeText->SetText(FText::AsNumber(Seconds));
	}
	TimeText->SetVisibility(ESlateVisibility::HitTestInvisible);
}
