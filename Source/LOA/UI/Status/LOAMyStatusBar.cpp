// LOAMyStatusBar.cpp

#include "UI/Status/LOAMyStatusBar.h"
#include "UI/Status/LOAStatusIconRow.h"
#include "UI/Party/LOAPartyMemberController.h"
#include "UI/LOAOverlayWidgetController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAMyStatusBar)

void ULOAMyStatusBar::NativeDestruct()
{
	UnbindController();

	Super::NativeDestruct();
}

void ULOAMyStatusBar::NativeOnWidgetControllerSet()
{
	Super::NativeOnWidgetControllerSet();

	UnbindController();
	ULOAOverlayWidgetController* Controller = Cast<ULOAOverlayWidgetController>(GetWidgetController());
	if (!Controller)
	{
		return;
	}
	OverlayController = Controller;
	Controller->OnPartyChanged.AddUniqueDynamic(this, &ThisClass::HandlePartyChanged);
}

void ULOAMyStatusBar::UnbindController()
{
	if (ULOAOverlayWidgetController* Controller = OverlayController.Get())
	{
		Controller->OnPartyChanged.RemoveDynamic(this, &ThisClass::HandlePartyChanged);
	}
	OverlayController.Reset();
}

void ULOAMyStatusBar::HandlePartyChanged(const TArray<ULOAPartyMemberController*>& Members)
{
	if (!StatusRow)
	{
		return;
	}

	ULOAPartyMemberController* const* Me = Members.FindByPredicate([](const ULOAPartyMemberController* Member)
	{
		return Member && Member->IsLocalPlayer();
	});
	// 나는 남은 초까지 보인다 (내 GE는 이 컴퓨터에 있다)
	StatusRow->SetStatusTracker(Me ? (*Me)->GetStatusTracker() : nullptr, true);
}
