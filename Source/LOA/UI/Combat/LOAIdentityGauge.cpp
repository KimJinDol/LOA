// LOAIdentityGauge.cpp

#include "UI/Combat/LOAIdentityGauge.h"
#include "UI/LOAOverlayWidgetController.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAIdentityGauge)

namespace LOAIdentityGauge
{
	// M_IdentityGauge의 파라미터 이름
	const FName PercentParam(TEXT("Percent"));
	const FName ActiveParam(TEXT("Active"));
	const FName FullParam(TEXT("Full"));
}

// =============================================================================
// 수명
// =============================================================================

void ULOAIdentityGauge::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (GaugeImage)
	{
		// 브러시의 머티리얼을 이 위젯 전용 인스턴스로 바꿔서 받는다 (이미 인스턴스면 그대로)
		GaugeMaterial = GaugeImage->GetDynamicMaterial();
	}

	// 최대가 오기 전(초기화 전, 게이지 없는 직업)에는 숨김
	SetVisibility(ESlateVisibility::Collapsed);
	ApplyToMaterial();
}

void ULOAIdentityGauge::NativeDestruct()
{
	UnbindController();

	Super::NativeDestruct();
}

void ULOAIdentityGauge::NativeOnWidgetControllerSet()
{
	Super::NativeOnWidgetControllerSet();

	UnbindController();
	ULOAOverlayWidgetController* Controller = Cast<ULOAOverlayWidgetController>(GetWidgetController());
	if (!Controller)
	{
		return;
	}
	OverlayController = Controller;
	Controller->OnIdentityGaugeChanged.AddUniqueDynamic(this, &ThisClass::HandleGaugeChanged);
	Controller->OnIdentityActiveChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveChanged);
}

void ULOAIdentityGauge::UnbindController()
{
	if (ULOAOverlayWidgetController* Controller = OverlayController.Get())
	{
		Controller->OnIdentityGaugeChanged.RemoveDynamic(this, &ThisClass::HandleGaugeChanged);
		Controller->OnIdentityActiveChanged.RemoveDynamic(this, &ThisClass::HandleActiveChanged);
	}
	OverlayController.Reset();
}

// =============================================================================
// 알림
// =============================================================================

void ULOAIdentityGauge::HandleGaugeChanged(float CurrentValue, float MaxValue)
{
	// 최대 0 = 이 직업은 게이지가 없음 → 숨김
	if (MaxValue <= 0.f)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	Percent = FMath::Clamp(CurrentValue / MaxValue, 0.f, 1.f);
	ApplyToMaterial();
}

void ULOAIdentityGauge::HandleActiveChanged(bool bInActive)
{
	bActive = bInActive;
	ApplyToMaterial();
}

void ULOAIdentityGauge::ApplyToMaterial()
{
	if (!GaugeMaterial)
	{
		return;
	}

	// 가득 빛남은 꺼져 있을 때만 (켜는 순간은 아직 100이지만 이미 켜진 색이어야 한다)
	const bool bFull = !bActive && Percent >= 1.f;

	GaugeMaterial->SetScalarParameterValue(LOAIdentityGauge::PercentParam, Percent);
	GaugeMaterial->SetScalarParameterValue(LOAIdentityGauge::ActiveParam, bActive ? 1.f : 0.f);
	GaugeMaterial->SetScalarParameterValue(LOAIdentityGauge::FullParam, bFull ? 1.f : 0.f);
}
