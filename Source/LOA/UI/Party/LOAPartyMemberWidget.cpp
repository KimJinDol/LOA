// LOAPartyMemberWidget.cpp

#include "UI/Party/LOAPartyMemberWidget.h"
#include "UI/Party/LOAPartyMemberController.h"
#include "UI/Status/LOAStatusIconRow.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyMemberWidget)

void ULOAPartyMemberWidget::SetMemberController(ULOAPartyMemberController* InController)
{
	Unbind();
	Controller = InController;

	if (StatusRow)
	{
		StatusRow->SetStatusTracker(InController ? InController->GetStatusTracker() : nullptr, false);
	}

	if (!InController)
	{
		return;
	}

	InController->OnInfoChanged.AddUniqueDynamic(this, &ThisClass::HandleInfoChanged);
	InController->OnHealthBarChanged.AddUniqueDynamic(this, &ThisClass::HandleHealthBarChanged);
	InController->OnDeadChanged.AddUniqueDynamic(this, &ThisClass::HandleDeadChanged);

	// 지금 값으로 한 번 채운다 (같은 컨트롤러에 묶인 다른 위젯도 같은 값을 다시 받는다)
	InController->BroadcastInitialValues();
}

void ULOAPartyMemberWidget::NativeDestruct()
{
	Unbind();

	Super::NativeDestruct();
}

void ULOAPartyMemberWidget::Unbind()
{
	if (ULOAPartyMemberController* MyController = Controller.Get())
	{
		MyController->OnInfoChanged.RemoveDynamic(this, &ThisClass::HandleInfoChanged);
		MyController->OnHealthBarChanged.RemoveDynamic(this, &ThisClass::HandleHealthBarChanged);
		MyController->OnDeadChanged.RemoveDynamic(this, &ThisClass::HandleDeadChanged);
	}
	Controller.Reset();
}

void ULOAPartyMemberWidget::HandleInfoChanged(FText PlayerName, FText ClassName, UTexture2D* InClassIcon)
{
	NameText->SetText(PlayerName);

	if (ClassText)
	{
		ClassText->SetText(ClassName);
	}
	if (ClassIcon)
	{
		if (InClassIcon)
		{
			ClassIcon->SetBrushFromTexture(InClassIcon);
			ClassIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 직업 데이터가 아직 안 왔거나 아이콘이 비어 있음 → 자리만 남기고 숨김
			ClassIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void ULOAPartyMemberWidget::HandleHealthBarChanged(float HealthPercent, float ShieldEndPercent, float Shield)
{
	HealthBar->SetPercent(HealthPercent);
	if (ShieldBar)
	{
		ShieldBar->SetPercent(ShieldEndPercent);
	}
}

void ULOAPartyMemberWidget::HandleDeadChanged(bool bIsDead)
{
	SetColorAndOpacity(bIsDead ? DeadTint : FLinearColor::White);
	if (DeadMarker)
	{
		DeadMarker->SetVisibility(bIsDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
