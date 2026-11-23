// LOAChargeGauge.cpp

#include "UI/Combat/LOAChargeGauge.h"
#include "UI/LOAOverlayWidgetController.h"

#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAChargeGauge)

namespace LOAChargeGauge
{
	/** 가로 박스 자식의 칸 비율 (Fill). 가로 박스 자식이 아니면 아무것도 안 함 */
	void SetFill(UWidget* Widget, float Value)
	{
		if (UHorizontalBoxSlot* BoxSlot = Widget ? Cast<UHorizontalBoxSlot>(Widget->Slot) : nullptr)
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(0.f, Value);
			BoxSlot->SetSize(Size);
		}
	}
}

// =============================================================================
// 수명
// =============================================================================

void ULOAChargeGauge::NativeConstruct()
{
	Super::NativeConstruct();

	// 처음엔 숨김 (bShown 기본값 true → 한 번은 반드시 숨긴다)
	SetShown(false);
}

void ULOAChargeGauge::NativeDestruct()
{
	ClearHideTimer();
	UnbindController();

	Super::NativeDestruct();
}

void ULOAChargeGauge::NativeOnWidgetControllerSet()
{
	Super::NativeOnWidgetControllerSet();

	UnbindController();
	ULOAOverlayWidgetController* Controller = Cast<ULOAOverlayWidgetController>(GetWidgetController());
	if (!Controller)
	{
		return;
	}
	OverlayController = Controller;
	Controller->OnChargeStarted.AddUniqueDynamic(this, &ThisClass::HandleChargeStarted);
	Controller->OnChargeReleased.AddUniqueDynamic(this, &ThisClass::HandleChargeReleased);
	Controller->OnChargeCancelled.AddUniqueDynamic(this, &ThisClass::HandleChargeCancelled);
}

void ULOAChargeGauge::UnbindController()
{
	if (ULOAOverlayWidgetController* Controller = OverlayController.Get())
	{
		Controller->OnChargeStarted.RemoveDynamic(this, &ThisClass::HandleChargeStarted);
		Controller->OnChargeReleased.RemoveDynamic(this, &ThisClass::HandleChargeReleased);
		Controller->OnChargeCancelled.RemoveDynamic(this, &ThisClass::HandleChargeCancelled);
	}
	OverlayController.Reset();
}

// =============================================================================
// 차지 알림
// =============================================================================

void ULOAChargeGauge::HandleChargeStarted()
{
	ClearHideTimer();
	SetShown(true);
	DrawState();
}

void ULOAChargeGauge::HandleChargeReleased(int32 Stage, bool bPerfect)
{
	// 발사: 다 채운 단계 수를 잠깐 보여 주고 숨긴다
	if (StageText)
	{
		StageText->SetText(FText::AsNumber(Stage));
		StageText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	ClearHideTimer();
	UWorld* World = GetWorld();
	if (!World || HideDelay <= 0.f)
	{
		HideGauge();
		return;
	}
	World->GetTimerManager().SetTimer(HideTimer, this, &ThisClass::HideGauge, HideDelay, false);
}

void ULOAChargeGauge::HandleChargeCancelled()
{
	ClearHideTimer();
	HideGauge();
}

void ULOAChargeGauge::HideGauge()
{
	SetShown(false);
}

void ULOAChargeGauge::ClearHideTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
}

// =============================================================================
// 그리기
// =============================================================================

void ULOAChargeGauge::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bShown)
	{
		DrawState();
	}
}

void ULOAChargeGauge::DrawState()
{
	const ULOAOverlayWidgetController* Controller = OverlayController.Get();
	if (!Controller)
	{
		return;
	}

	// 발사 뒤(차지 중 아님)에는 마지막 모습 그대로 둔다
	const FLOAChargeGaugeState State = Controller->GetChargeGaugeState();
	if (!State.bActive)
	{
		return;
	}

	if (ChargeBar)
	{
		ChargeBar->SetPercent(State.Progress);
		ChargeBar->SetFillColorAndOpacity(State.bInPerfectZone ? PerfectFillColor : FillColor);
	}

	SetPerfectZone(State.bShowPerfectZone, State.PerfectZoneStart, State.PerfectZoneEnd);

	if (StageText)
	{
		StageText->SetText(FText::AsNumber(State.Stage));
		StageText->SetVisibility(State.Stage > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ULOAChargeGauge::SetPerfectZone(bool bShow, float ZoneStart, float ZoneEnd)
{
	if (!PerfectZoneRow)
	{
		return;
	}
	// 숨길 때도 자리는 남긴다 (Hidden). 줄이 막대 뒤에 겹쳐 있어서 크기에 영향이 없다
	PerfectZoneRow->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (!bShow)
	{
		return;
	}

	const float Start = FMath::Clamp(ZoneStart, 0.f, 1.f);
	const float End = FMath::Clamp(ZoneEnd, Start, 1.f);
	LOAChargeGauge::SetFill(PerfectZoneBefore, Start);
	LOAChargeGauge::SetFill(PerfectZoneBand, End - Start);
	LOAChargeGauge::SetFill(PerfectZoneAfter, 1.f - End);
}

void ULOAChargeGauge::SetShown(bool bShow)
{
	if (bShown == bShow)
	{
		return;
	}
	bShown = bShow;

	// 숨길 때 Collapsed (자리도 차지하지 않게). 보일 때 클릭은 통과
	if (ChargePanel)
	{
		ChargePanel->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
