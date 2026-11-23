// LOAPartyRecruitSlotWidget.cpp

#include "UI/Lobby/LOAPartyRecruitSlotWidget.h"
#include "Party/LOAPartyTypes.h"
#include "Data/LOACharacterClassData.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyRecruitSlotWidget)

namespace LOARecruitSlot
{
	/** 보이기 / 숨기기 (자리도 비움) */
	static void SetShown(UWidget* Widget, bool bShown)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

void ULOAPartyRecruitSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (AcceptButton)
	{
		AcceptButton->OnClicked.AddDynamic(this, &ThisClass::HandleAcceptClicked);
	}
	if (RejectButton)
	{
		RejectButton->OnClicked.AddDynamic(this, &ThisClass::HandleRejectClicked);
	}
	if (KickButton)
	{
		KickButton->OnClicked.AddDynamic(this, &ThisClass::HandleKickClicked);
	}
}

void ULOAPartyRecruitSlotWidget::ShowMember(const FLOAPartyMember& Member, bool bCanKick)
{
	ShowPerson(Member);
	SetButtonsVisible(false, false, bCanKick);
}

void ULOAPartyRecruitSlotWidget::ShowApplicant(const FLOAPartyMember& Member)
{
	ShowPerson(Member);
	SetButtonsVisible(true, true, false);
}

void ULOAPartyRecruitSlotWidget::ShowEmpty()
{
	MemberId = INDEX_NONE;

	if (NameText)
	{
		NameText->SetText(EmptyText);
	}
	LOARecruitSlot::SetShown(LeaderIcon, false);
	LOARecruitSlot::SetShown(ClassIcon, false);
	LOARecruitSlot::SetShown(EmptyIcon, true);
	SetButtonsVisible(false, false, false);
}

void ULOAPartyRecruitSlotWidget::ShowPerson(const FLOAPartyMember& Member)
{
	MemberId = Member.MemberId;

	if (NameText)
	{
		NameText->SetText(FText::FromString(Member.PlayerName));
	}
	LOARecruitSlot::SetShown(LeaderIcon, Member.bLeader);
	LOARecruitSlot::SetShown(EmptyIcon, false);

	if (ClassIcon)
	{
		// 로비 화면이 불러 둔 직업 데이터에서 찾는다. 없으면(못 불러옴·다른 직업 목록) 아이콘만 숨긴다
		const ULOACharacterClassData* ClassData = UAssetManager::IsInitialized()
			? UAssetManager::Get().GetPrimaryAssetObject<ULOACharacterClassData>(Member.ClassId)
			: nullptr;
		if (ClassData && !ClassData->Icon.IsNull())
		{
			ClassIcon->SetBrushFromSoftTexture(ClassData->Icon);
			LOARecruitSlot::SetShown(ClassIcon, true);
		}
		else
		{
			LOARecruitSlot::SetShown(ClassIcon, false);
		}
	}
}

void ULOAPartyRecruitSlotWidget::SetButtonsVisible(bool bAccept, bool bReject, bool bKick)
{
	LOARecruitSlot::SetShown(AcceptButton, bAccept);
	LOARecruitSlot::SetShown(RejectButton, bReject);
	LOARecruitSlot::SetShown(KickButton, bKick);
}

void ULOAPartyRecruitSlotWidget::HandleAcceptClicked()
{
	OnAcceptClicked.ExecuteIfBound(MemberId);
}

void ULOAPartyRecruitSlotWidget::HandleRejectClicked()
{
	OnRejectClicked.ExecuteIfBound(MemberId);
}

void ULOAPartyRecruitSlotWidget::HandleKickClicked()
{
	OnKickClicked.ExecuteIfBound(MemberId);
}
