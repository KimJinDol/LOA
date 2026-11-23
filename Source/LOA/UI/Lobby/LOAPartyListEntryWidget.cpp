// LOAPartyListEntryWidget.cpp

#include "UI/Lobby/LOAPartyListEntryWidget.h"
#include "Party/LOAPartyTypes.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyListEntryWidget)

void ULOAPartyListEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EntryButton)
	{
		EntryButton->OnClicked.AddDynamic(this, &ThisClass::HandleEntryButtonClicked);
	}
}

void ULOAPartyListEntryWidget::Setup(const FLOAPartyListEntry& Entry, int32 InListIndex, const FText& RaidName)
{
	ListIndex = InListIndex;

	if (RaidNameText)
	{
		RaidNameText->SetText(RaidName);
	}
	if (LeaderNameText)
	{
		LeaderNameText->SetText(FText::FromString(Entry.LeaderName));
	}
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Entry.Title));
	}
	if (MemberCountText)
	{
		// 예) "1 / 3" (사진과 같은 모양. 숫자 두 개를 잇는 기호라 글 칸으로 빼지 않음)
		MemberCountText->SetText(FText::Format(INVTEXT("{0} / {1}"), FText::AsNumber(Entry.NumMembers), FText::AsNumber(Entry.MaxMembers)));
	}
}

void ULOAPartyListEntryWidget::SetSelected(bool bSelected)
{
	OnSelectedChanged(bSelected);
}

void ULOAPartyListEntryWidget::HandleEntryButtonClicked()
{
	OnClicked.ExecuteIfBound(ListIndex);
}
