// LOABossHealthBarWidget.cpp

#include "UI/LOABossHealthBarWidget.h"
#include "UI/LOABossWidgetController.h"
#include "UI/Status/LOAStatusIconRow.h"

#include "Components/ProgressBar.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOABossHealthBarWidget)

void ULOABossHealthBarWidget::NativeOnWidgetControllerSet()
{
	Super::NativeOnWidgetControllerSet();

	// 컨트롤러가 바뀌면 옛 구독부터 푼다 (같은 알림이 두 번 걸리지 않게)
	UnbindController();

	ULOABossWidgetController* Controller = Cast<ULOABossWidgetController>(GetWidgetController());

	// 보스는 남은 초까지 보인다 (보스의 시간 복제 부품이 모든 컴퓨터에 보낸다)
	if (StatusRow)
	{
		StatusRow->SetStatusTracker(Controller ? Controller->GetStatusTracker() : nullptr, true);
	}

	// 무력화 게이지 (2026-11-18 apply120). 첫 값은 HUD가 곧 부르는 BroadcastInitialValues로 온다
	if (StaggerBar)
	{
		// 값이 오기 전에는 숨겨 둔다 (무력화 없는 보스면 계속 숨김)
		StaggerBar->SetVisibility(ESlateVisibility::Collapsed);

		if (Controller)
		{
			Controller->OnBossStaggerChanged.AddDynamic(this, &ThisClass::HandleStaggerChanged);
			BoundController = Controller;
		}
	}
}

void ULOABossHealthBarWidget::NativeDestruct()
{
	UnbindController();

	Super::NativeDestruct();
}

void ULOABossHealthBarWidget::HandleStaggerChanged(float CurrentValue, float MaxValue)
{
	if (!StaggerBar)
	{
		return;
	}

	// 최대가 0 = 무력화 없는 보스이거나 아직 복제 전 → 숨김
	if (MaxValue <= 0.f)
	{
		StaggerBar->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 예) 80 / 100 → 0.8
	StaggerBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	StaggerBar->SetPercent(FMath::Clamp(CurrentValue / MaxValue, 0.f, 1.f));
}

void ULOABossHealthBarWidget::UnbindController()
{
	if (ULOABossWidgetController* Controller = BoundController.Get())
	{
		Controller->OnBossStaggerChanged.RemoveDynamic(this, &ThisClass::HandleStaggerChanged);
	}
	BoundController.Reset();
}
