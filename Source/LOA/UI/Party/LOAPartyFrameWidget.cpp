// LOAPartyFrameWidget.cpp

#include "UI/Party/LOAPartyFrameWidget.h"
#include "UI/Party/LOAPartyMemberWidget.h"
#include "UI/Party/LOAPartyMemberController.h"
#include "UI/LOAOverlayWidgetController.h"
#include "Core/LOALog.h"

#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyFrameWidget)

void ULOAPartyFrameWidget::NativeDestruct()
{
	UnbindController();

	Super::NativeDestruct();
}

void ULOAPartyFrameWidget::NativeOnWidgetControllerSet()
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

void ULOAPartyFrameWidget::UnbindController()
{
	if (ULOAOverlayWidgetController* Controller = OverlayController.Get())
	{
		Controller->OnPartyChanged.RemoveDynamic(this, &ThisClass::HandlePartyChanged);
	}
	OverlayController.Reset();
}

void ULOAPartyFrameWidget::HandlePartyChanged(const TArray<ULOAPartyMemberController*>& Members)
{
	if (!MemberBox)
	{
		return;
	}
	if (!MemberWidgetClass)
	{
		LOA_LOG(this, Warning, TEXT("[PartyFrame] MemberWidgetClass is not set on %s."), *GetNameSafe(GetClass()));
		return;
	}

	int32 Index = 0;
	for (ULOAPartyMemberController* Member : Members)
	{
		if (!Member)
		{
			continue;
		}

		if (!MemberWidgets.IsValidIndex(Index))
		{
			ULOAPartyMemberWidget* NewWidget = CreateWidget<ULOAPartyMemberWidget>(this, MemberWidgetClass);
			if (!NewWidget)
			{
				break;
			}
			if (UVerticalBoxSlot* BoxSlot = MemberBox->AddChildToVerticalBox(NewWidget))
			{
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MemberSpacing));
			}
			MemberWidgets.Add(NewWidget);
		}

		ULOAPartyMemberWidget* Widget = MemberWidgets[Index];
		// 같은 사람이면 다시 묶지 않는다 (순서가 그대로인 흔한 경우)
		if (Widget->GetMemberController() != Member)
		{
			Widget->SetMemberController(Member);
		}
		Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		++Index;
	}

	// 남는 칸: 구독을 풀고 숨긴다
	for (int32 Rest = Index; Rest < MemberWidgets.Num(); ++Rest)
	{
		MemberWidgets[Rest]->SetMemberController(nullptr);
		MemberWidgets[Rest]->SetVisibility(ESlateVisibility::Collapsed);
	}
}
