// LOAPartyFinderWidget.cpp

#include "UI/Lobby/LOAPartyFinderWidget.h"
#include "UI/Lobby/LOAPartyListEntryWidget.h"
#include "UI/Lobby/LOAPartyRecruitSlotWidget.h"
#include "Party/LOAPartySubsystem.h"
#include "Data/LOARaidData.h"
#include "Core/LOALog.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyFinderWidget)

namespace LOAPartyFinder
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

// ─────────────────────────────────────────────────────────────────────────────
// 시작·끝
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartyFinderWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 버튼은 위젯이 처음 만들어질 때 한 번 (NativeConstruct는 다시 붙을 때마다 불린다)
	CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	RefreshButton->OnClicked.AddDynamic(this, &ThisClass::HandleRefreshClicked);
	CreatePartyButton->OnClicked.AddDynamic(this, &ThisClass::HandleCreatePartyClicked);
	ApplyButton->OnClicked.AddDynamic(this, &ThisClass::HandleApplyClicked);
	LeaveButton->OnClicked.AddDynamic(this, &ThisClass::HandleLeaveClicked);
	EnterButton->OnClicked.AddDynamic(this, &ThisClass::HandleEnterClicked);
	MembersTabButton->OnClicked.AddDynamic(this, &ThisClass::HandleMembersTabClicked);
	ApplicantsTabButton->OnClicked.AddDynamic(this, &ThisClass::HandleApplicantsTabClicked);
}

void ULOAPartyFinderWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGameInstance* GameInstance = GetGameInstance();
	ULOAPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<ULOAPartySubsystem>() : nullptr;
	if (!Party)
	{
		LOA_LOG(this, Warning, TEXT("[PartyUI] No ULOAPartySubsystem."));
		return;
	}

	PartySubsystem = Party;
	Party->OnPartyChanged.AddUObject(this, &ThisClass::HandlePartyChanged);
	Party->OnPartiesFound.AddUObject(this, &ThisClass::HandlePartiesFound);
	Party->OnPartyNotice.AddUObject(this, &ThisClass::HandlePartyNotice);

	// 접혀 있어도 지금 상태로 맞춰 둔다 (레이드에서 돌아와 로비가 다시 열림 등)
	LastRole = Party->GetRole();
	RebuildList(Party->GetListings());
	RefreshParty();
}

void ULOAPartyFinderWidget::NativeDestruct()
{
	// 서브시스템은 맵이 바뀌어도 남는다 → 듣기를 끊는다
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		Party->OnPartyChanged.RemoveAll(this);
		Party->OnPartiesFound.RemoveAll(this);
		Party->OnPartyNotice.RemoveAll(this);
	}
	PartySubsystem.Reset();

	Super::NativeDestruct();
}

void ULOAPartyFinderWidget::Open()
{
	SetVisibility(ESlateVisibility::Visible);
	RefreshParty();

	// 창을 열 때 한 번 검색 (11-20 사용자). 파티 중에도 (11-21 사용자)
	StartSearch();
}

// ─────────────────────────────────────────────────────────────────────────────
// 서브시스템 알림
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartyFinderWidget::HandlePartyChanged()
{
	const ULOAPartySubsystem* Party = PartySubsystem.Get();
	if (!Party)
	{
		return;
	}

	const ELOAPartyRole NewRole = Party->GetRole();
	const bool bLeftParty = LastRole != ELOAPartyRole::None && NewRole == ELOAPartyRole::None;
	LastRole = NewRole;

	// 파티장이 아니면 신청자 탭을 볼 수 없다 (파티장 → 나감 → 다시 파티원 등)
	if (NewRole != ELOAPartyRole::Leader)
	{
		bShowingApplicants = false;
	}

	RefreshParty();

	// 파티에서 빠짐(나감·거절·강퇴·해산) → 목록이 오래됐다 → 다시 검색 (보이는 동안만)
	// 입장하기로 끝난 파티는 빠짐이 아니다 (파티장 컴퓨터는 곧 레이드 맵으로 간다)
	if (bLeftParty && IsVisible() && !bEnterRequested)
	{
		StartSearch();
	}
}

void ULOAPartyFinderWidget::HandlePartiesFound(const TArray<FLOAPartyListEntry>& Listings)
{
	RebuildList(Listings);
	RefreshButtons();
}

void ULOAPartyFinderWidget::HandlePartyNotice(const FLOAPartyNotice& Notice)
{
	// 이름이 있으면 남의 일, 없으면 내 일 (FLOAPartyNotice 규칙)
	const bool bSelf = Notice.PlayerName.IsEmpty();
	const FText* NoticeFormat = bSelf ? SelfNoticeFormats.Find(Notice.Type) : NoticeFormats.Find(Notice.Type);
	if (!NoticeFormat)
	{
		LOA_LOG(this, Warning, TEXT("[PartyUI] No %s notice text for %s. Fill Notice Formats on %s."),
			bSelf ? TEXT("self") : TEXT("other"), *UEnum::GetValueAsString(Notice.Type), *GetNameSafe(GetClass()));
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Name"), FText::FromString(Notice.PlayerName));
	NoticeLines.Add(FText::Format(*NoticeFormat, Args));

	// 오래된 줄부터 지운다
	while (NoticeLines.Num() > MaxNoticeLines)
	{
		NoticeLines.RemoveAt(0);
	}
	NoticeText->SetText(FText::Join(FText::FromString(TEXT("\n")), NoticeLines));
}

// ─────────────────────────────────────────────────────────────────────────────
// 화면 맞추기
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartyFinderWidget::RefreshParty()
{
	const ULOAPartySubsystem* Party = PartySubsystem.Get();
	if (!Party)
	{
		return;
	}

	const ELOAPartyRole Role = Party->GetRole();
	const FLOAPartyInfo& Info = Party->GetPartyInfo();

	// ① 띠: 파티에 있거나 신청 중일 때만
	LOAPartyFinder::SetShown(StatusBanner, Role != ELOAPartyRole::None);
	BannerStatusText->SetText(Role == ELOAPartyRole::Applicant ? ApplyingStatusText : RecruitingStatusText);
	BannerTitleText->SetText(FText::FromString(Info.Title));

	// ② 오른쪽: 역할마다 다른 칸
	switch (Role)
	{
	case ELOAPartyRole::None:
		DetailSwitcher->SetActiveWidget(CreatePanel);
		break;

	case ELOAPartyRole::Applicant:
		DetailSwitcher->SetActiveWidget(WaitingPanel);
		WaitingTitleText->SetText(FText::FromString(Info.Title));
		break;

	default:
	{
		DetailSwitcher->SetActiveWidget(PartyPanel);

		FFormatNamedArguments HeaderArgs;
		HeaderArgs.Add(TEXT("Raid"), GetRaidName(Info.RaidId));
		DetailHeaderText->SetText(FText::Format(DetailHeaderFormat, HeaderArgs));
		DetailTitleText->SetText(FText::FromString(Info.Title));

		// 신청자 탭은 파티장만 (11-19 사용자)
		LOAPartyFinder::SetShown(ApplicantsTabButton, Role == ELOAPartyRole::Leader);
		if (ApplicantsTabText)
		{
			FFormatNamedArguments TabArgs;
			TabArgs.Add(TEXT("Count"), FText::AsNumber(Info.Applicants.Num()));
			ApplicantsTabText->SetText(FText::Format(ApplicantsTabFormat, TabArgs));
		}

		RefreshSlots();
		break;
	}
	}

	// ③ 버튼
	RefreshButtons();
}

void ULOAPartyFinderWidget::RefreshSlots()
{
	SlotBox->ClearChildren();

	const ULOAPartySubsystem* Party = PartySubsystem.Get();
	if (!Party || !RecruitSlotClass)
	{
		return;
	}

	const FLOAPartyInfo& Info = Party->GetPartyInfo();
	const bool bLeader = Party->GetRole() == ELOAPartyRole::Leader;

	if (bShowingApplicants)
	{
		// 신청자 탭: 신청자 수만큼 (수락·거절)
		for (const FLOAPartyMember& Applicant : Info.Applicants)
		{
			if (ULOAPartyRecruitSlotWidget* RecruitSlot = AddRecruitSlot())
			{
				RecruitSlot->ShowApplicant(Applicant);
				RecruitSlot->OnAcceptClicked.BindUObject(this, &ThisClass::HandleAcceptClicked);
				RecruitSlot->OnRejectClicked.BindUObject(this, &ThisClass::HandleRejectClicked);
			}
		}
		return;
	}

	// 참가자 탭: 정원만큼. 빈자리는 "모집중" (예: 2/3 → 이름 2칸 + 모집중 1칸)
	for (int32 Index = 0; Index < Info.MaxMembers; ++Index)
	{
		ULOAPartyRecruitSlotWidget* RecruitSlot = AddRecruitSlot();
		if (!RecruitSlot)
		{
			continue;
		}

		if (Info.Members.IsValidIndex(Index))
		{
			const FLOAPartyMember& Member = Info.Members[Index];
			// 강퇴 X: 파티장 화면 + 파티장 자신이 아닌 칸
			RecruitSlot->ShowMember(Member, bLeader && !Member.bLeader);
			RecruitSlot->OnKickClicked.BindUObject(this, &ThisClass::HandleKickClicked);
		}
		else
		{
			RecruitSlot->ShowEmpty();
		}
	}
}

ULOAPartyRecruitSlotWidget* ULOAPartyFinderWidget::AddRecruitSlot()
{
	ULOAPartyRecruitSlotWidget* RecruitSlot = CreateWidget<ULOAPartyRecruitSlotWidget>(this, RecruitSlotClass);
	if (RecruitSlot)
	{
		SlotBox->AddChild(RecruitSlot);
	}
	return RecruitSlot;
}

void ULOAPartyFinderWidget::RefreshButtons()
{
	const ULOAPartySubsystem* Party = PartySubsystem.Get();
	if (!Party)
	{
		return;
	}

	const ELOAPartyRole Role = Party->GetRole();

	// 파티 없음: 신청하기 (줄을 골라야 켜짐) / 새로고침: 역할과 상관없이 검색 중이 아니면 (11-21 사용자)
	LOAPartyFinder::SetShown(ApplyButton, Role == ELOAPartyRole::None);
	ApplyButton->SetIsEnabled(Party->GetListings().IsValidIndex(SelectedListIndex));
	RefreshButton->SetIsEnabled(!Party->IsSearching());

	// 파티에 있거나 신청 중: 나가기 (신청자는 "신청 취소")
	LOAPartyFinder::SetShown(LeaveButton, Role != ELOAPartyRole::None);
	LeaveButtonText->SetText(Role == ELOAPartyRole::Applicant ? CancelApplyText : LeaveText);

	// 파티장: 입장하기 (3명 안 차도 됨, 11-19 사용자)
	LOAPartyFinder::SetShown(EnterButton, Role == ELOAPartyRole::Leader);
}

void ULOAPartyFinderWidget::RebuildList(const TArray<FLOAPartyListEntry>& Listings)
{
	ListEntryBox->ClearChildren();
	EntryWidgets.Reset();
	SelectedListIndex = INDEX_NONE;

	if (ListMessageText)
	{
		ListMessageText->SetText(NoPartyText);
		LOAPartyFinder::SetShown(ListMessageText, Listings.IsEmpty());
	}

	if (!ListEntryClass)
	{
		LOA_LOG(this, Warning, TEXT("[PartyUI] List Entry Class is not set on %s."), *GetNameSafe(GetClass()));
		return;
	}

	for (int32 Index = 0; Index < Listings.Num(); ++Index)
	{
		ULOAPartyListEntryWidget* EntryWidget = CreateWidget<ULOAPartyListEntryWidget>(this, ListEntryClass);
		if (!EntryWidget)
		{
			continue;
		}
		EntryWidget->Setup(Listings[Index], Index, GetRaidName(Listings[Index].RaidId));
		EntryWidget->OnClicked.BindUObject(this, &ThisClass::HandleEntryClicked);
		ListEntryBox->AddChild(EntryWidget);
		EntryWidgets.Add(EntryWidget);
	}
}

void ULOAPartyFinderWidget::StartSearch()
{
	ULOAPartySubsystem* Party = PartySubsystem.Get();
	// 이미 검색 중이면 그 결과를 기다린다 ("검색 중" 그대로)
	if (!Party || Party->IsSearching() || !Party->FindParties())
	{
		return;
	}

	// 결과가 올 때까지 (1~5초) 목록을 비우고 "검색 중"
	ListEntryBox->ClearChildren();
	EntryWidgets.Reset();
	SelectedListIndex = INDEX_NONE;
	if (ListMessageText)
	{
		ListMessageText->SetText(SearchingText);
		LOAPartyFinder::SetShown(ListMessageText, true);
	}
	RefreshButtons();
}

FText ULOAPartyFinderWidget::GetRaidName(const FPrimaryAssetId& RaidId) const
{
	const ULOARaidData* RaidData = ULOARaidData::LoadById(RaidId);
	if (RaidData && !RaidData->DisplayName.IsEmpty())
	{
		return RaidData->DisplayName;
	}
	// 이 컴퓨터에 없는 레이드(다른 빌드) 또는 이름이 빈 데이터 → 에셋 이름이라도
	return FText::FromName(RaidId.PrimaryAssetName);
}

// ─────────────────────────────────────────────────────────────────────────────
// 버튼
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartyFinderWidget::HandleCloseClicked()
{
	// 닫아도 파티는 그대로 (다시 열면 이어서 본다)
	SetVisibility(ESlateVisibility::Collapsed);
}

void ULOAPartyFinderWidget::HandleRefreshClicked()
{
	StartSearch();
}

void ULOAPartyFinderWidget::HandleCreatePartyClicked()
{
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		// 글 길이는 서브시스템이 자른다 (LOA Lobby → Max Party Title Length)
		Party->CreateParty(PartyTitleInput->GetText().ToString());
	}
}

void ULOAPartyFinderWidget::HandleApplyClicked()
{
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		Party->ApplyToParty(SelectedListIndex);
	}
}

void ULOAPartyFinderWidget::HandleLeaveClicked()
{
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		Party->LeaveParty();
	}
}

void ULOAPartyFinderWidget::HandleEnterClicked()
{
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		// 먼저 표시: EnterRaid 안에서 역할이 None이 되며 HandlePartyChanged가 바로 불린다. 실패하면 되돌린다
		bEnterRequested = true;
		bEnterRequested = Party->EnterRaid();
	}
}

void ULOAPartyFinderWidget::HandleMembersTabClicked()
{
	SetApplicantsTab(false);
}

void ULOAPartyFinderWidget::HandleApplicantsTabClicked()
{
	SetApplicantsTab(true);
}

void ULOAPartyFinderWidget::SetApplicantsTab(bool bApplicants)
{
	const ULOAPartySubsystem* Party = PartySubsystem.Get();
	bShowingApplicants = bApplicants && Party && Party->GetRole() == ELOAPartyRole::Leader;
	OnTabChanged(bShowingApplicants);
	RefreshSlots();
}

// ─────────────────────────────────────────────────────────────────────────────
// 줄·칸에서
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartyFinderWidget::HandleEntryClicked(int32 ListIndex)
{
	SelectedListIndex = ListIndex;
	for (int32 Index = 0; Index < EntryWidgets.Num(); ++Index)
	{
		if (EntryWidgets[Index])
		{
			EntryWidgets[Index]->SetSelected(Index == ListIndex);
		}
	}
	RefreshButtons();
}

void ULOAPartyFinderWidget::HandleAcceptClicked(int32 MemberId)
{
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		Party->AcceptApplicant(MemberId);
	}
}

void ULOAPartyFinderWidget::HandleRejectClicked(int32 MemberId)
{
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		Party->RejectApplicant(MemberId);
	}
}

void ULOAPartyFinderWidget::HandleKickClicked(int32 MemberId)
{
	if (ULOAPartySubsystem* Party = PartySubsystem.Get())
	{
		Party->KickMember(MemberId);
	}
}
