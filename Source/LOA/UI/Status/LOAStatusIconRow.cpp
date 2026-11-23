// LOAStatusIconRow.cpp

#include "UI/Status/LOAStatusIconRow.h"
#include "UI/Status/LOAStatusIcon.h"
#include "UI/Status/LOAStatusInfoData.h"
#include "UI/Status/LOAStatusTracker.h"
#include "Core/LOALog.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStatusIconRow)

void ULOAStatusIconRow::SetStatusTracker(ULOAStatusTracker* InTracker, bool bInShowTime)
{
	Unbind();

	Tracker = InTracker;
	bShowTime = bInShowTime;

	if (!InTracker)
	{
		Rebuild(FGameplayTagContainer());
		return;
	}

	InTracker->OnStatusTagsChanged.AddUniqueDynamic(this, &ThisClass::HandleStatusTagsChanged);
	Rebuild(InTracker->GetStatusTags());
}

void ULOAStatusIconRow::NativeDestruct()
{
	Unbind();

	Super::NativeDestruct();
}

void ULOAStatusIconRow::Unbind()
{
	if (ULOAStatusTracker* MyTracker = Tracker.Get())
	{
		MyTracker->OnStatusTagsChanged.RemoveDynamic(this, &ThisClass::HandleStatusTagsChanged);
	}
	Tracker.Reset();
}

void ULOAStatusIconRow::HandleStatusTagsChanged(const FGameplayTagContainer& StatusTags)
{
	Rebuild(StatusTags);
}

void ULOAStatusIconRow::Rebuild(const FGameplayTagContainer& StatusTags)
{
	if (!IconBox)
	{
		return;
	}
	if (!StatusInfoData || !IconWidgetClass)
	{
		LOA_LOG(this, Warning, TEXT("[StatusIconRow] StatusInfoData or IconWidgetClass is not set on %s."), *GetNameSafe(GetClass()));
		return;
	}

	// 표 순서대로, 붙어 있는 것만
	int32 Shown = 0;
	for (const FLOAStatusInfo& Info : StatusInfoData->Rows)
	{
		if (Shown >= MaxIcons)
		{
			break;
		}
		if (!StatusTags.HasTagExact(Info.StatusTag))
		{
			continue;
		}

		// 모자라면 만든다
		if (!IconWidgets.IsValidIndex(Shown))
		{
			ULOAStatusIcon* NewIcon = CreateWidget<ULOAStatusIcon>(this, IconWidgetClass);
			if (!NewIcon)
			{
				break;
			}
			if (UHorizontalBoxSlot* BoxSlot = IconBox->AddChildToHorizontalBox(NewIcon))
			{
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			}
			IconWidgets.Add(NewIcon);
		}

		ULOAStatusIcon* Icon = IconWidgets[Shown];
		Icon->Setup(Info, Tracker.Get(), bShowTime);
		Icon->SetVisibility(ESlateVisibility::Visible);	// 툴팁이 마우스를 받아야 한다 (헤더)
		++Shown;
	}

	// 남는 칸은 숨긴다
	for (int32 Index = Shown; Index < IconWidgets.Num(); ++Index)
	{
		IconWidgets[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}
