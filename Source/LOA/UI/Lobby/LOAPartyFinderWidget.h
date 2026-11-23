// LOAPartyFinderWidget.h
// [화면] 로비 파티 찾기 창. 왼쪽 = 모집 목록, 오른쪽 = 내 파티 (역할마다 바뀜), 아래 = 버튼. (2026-11-20 apply132)
//
// ─────────────────────────────────────────────────────────────────────────────
// [모양] 로아 파티 찾기 사진을 3인·레이드 하나로 줄임 (11-20 사용자 확인)
//   ┌ 모집 중 띠 (파티에 있을 때만: "모집 중" / "신청 중" + 한 줄 글)
//   ├ 왼쪽: 새로고침 / 목록 (목표 · 왕관 파티장 + 한 줄 글 · 1 / 3)
//   ├ 오른쪽 (DetailSwitcher):
//   │   파티 없음 → CreatePanel  : 한 줄 글 입력 + 모집 등록
//   │   신청자   → WaitingPanel : 신청한 방 제목
//   │   파티장·파티원 → PartyPanel : "{레이드} 파티 모집 상세 정보" / 참가자·신청자 탭 / 칸 3개
//   ├ 시스템 알림 (최근 몇 줄)
//   └ 버튼: 신청하기(파티 없음) / 파티에서 나가기·신청 취소 / 입장하기(파티장)
//
// [흐름]
//   로비 파티 찾기 버튼 → Open() → 보이기 + 목록 검색 (창을 열 때 한 번, 11-20 사용자. 파티 중에도, 11-21 사용자)
//   서브시스템 알림 (ULOAPartySubsystem, 멀티캐스트)
//     OnPartyChanged → 띠·오른쪽·버튼·칸 다시. 파티에서 빠지면(역할 → None) 목록 다시 검색
//     OnPartiesFound → 목록 줄 다시 (고른 줄은 풀림)
//     OnPartyNotice  → 알림 한 줄 추가 (글 = 아래 Notice Formats, 이름은 {Name})
//   목록 줄 클릭 → 고름 → 신청하기 → ApplyToParty(고른 번호)
//   칸 버튼(수락·거절·강퇴) / 입장하기 / 나가기 → 서브시스템 입구를 부를 뿐 (판단은 파티장 컴퓨터의 HostObject)
//
// [하지 않는 일]
//   파티 판단·검사. 이름 정하기 (로비 화면 ULOACharacterSelectWidget). 목록 자동 갱신·채팅·초대·모집 설정 (나중)
//   파티 중에도 검색·새로고침 된다 (11-21 사용자). 내 파티는 목록 맨 위 (서브시스템이 정렬, FLOAPartyListEntry::bMyParty)
//
// [WBP] WBP_PartyFinder (부모 = 이 클래스). WBP_CharacterSelect 안에 자식으로 넣고 처음엔 접어 둔다 (Collapsed)
//   CloseButton (Button) / RefreshButton (Button)
//   StatusBanner (아무 위젯) + BannerStatusText · BannerTitleText (TextBlock)
//   ListEntryBox (Panel, 스크롤 상자 추천) + ListMessageText (TextBlock, 선택: 검색 중 / 없음)
//   DetailSwitcher (WidgetSwitcher) 의 자식 3개: CreatePanel · WaitingPanel · PartyPanel (아무 위젯)
//     CreatePanel  안: PartyTitleInput (EditableTextBox), CreatePartyButton (Button)
//     WaitingPanel 안: WaitingTitleText (TextBlock)
//     PartyPanel   안: DetailHeaderText · DetailTitleText (TextBlock), MembersTabButton · ApplicantsTabButton (Button),
//                      ApplicantsTabText (TextBlock, 선택), SlotBox (Panel)
//   NoticeText (TextBlock, 여러 줄)
//   ApplyButton · LeaveButton · EnterButton (Button), LeaveButtonText (TextBlock)
//   Class Defaults: List Entry Class = WBP_PartyListEntry / Recruit Slot Class = WBP_PartyRecruitSlot / 글 칸들 (아래)
//
// [네트워크] 3 각자 (이 컴퓨터의 화면). 서버·파티장과의 일은 전부 ULOAPartySubsystem

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UObject/PrimaryAssetId.h"
#include "Party/LOAPartyTypes.h"
#include "LOAPartyFinderWidget.generated.h"

class UButton;
class UEditableTextBox;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;
class ULOAPartySubsystem;
class ULOAPartyListEntryWidget;
class ULOAPartyRecruitSlotWidget;

UCLASS(Abstract)
class LOA_API ULOAPartyFinderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 창을 연다: 보이기 + 화면 맞추기 + 목록 검색. 로비의 파티 찾기 버튼이 부른다 */
	void Open();

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** 탭이 바뀜 (골라진 탭 모양은 BP에서). bApplicantsTab = 신청자 탭 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Party")
	void OnTabChanged(bool bApplicantsTab);

	// ─── 위젯 (WBP 이름 그대로) ───

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> StatusBanner;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BannerStatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BannerTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ListEntryBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ListMessageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> DetailSwitcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CreatePanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> PartyTitleInput;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CreatePartyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> WaitingPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WaitingTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PartyPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailHeaderText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailTitleText;

	/** 탭 버튼 둘은 BP OnTabChanged가 색을 바꾸므로 BP에서 읽기 (작업 규칙: BindWidget을 BP 그래프에서 쓰면 BlueprintReadOnly) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> MembersTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ApplicantsTabButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ApplicantsTabText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SlotBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NoticeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ApplyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LeaveButtonText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EnterButton;

	// ─── 칸 위젯 ───

	/** 목록 한 줄 (WBP_PartyListEntry) */
	UPROPERTY(EditAnywhere, Category = "LOA|Party")
	TSubclassOf<ULOAPartyListEntryWidget> ListEntryClass;

	/** 상세의 한 칸 (WBP_PartyRecruitSlot) */
	UPROPERTY(EditAnywhere, Category = "LOA|Party")
	TSubclassOf<ULOAPartyRecruitSlotWidget> RecruitSlotClass;

	// ─── 글 (화면 글은 WBP에. 코드에 두지 않는다) ───

	/** 남의 일 알림. {Name} = 그 사람. 예) Joined → "{Name}님이 모집에 참여하였습니다." */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	TMap<ELOAPartyNoticeType, FText> NoticeFormats;

	/** 내 일 알림 (이름 없음). 예) Rejected → "참여 신청이 거절되었습니다." */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	TMap<ELOAPartyNoticeType, FText> SelfNoticeFormats;

	/** 알림을 몇 줄까지 보이나 (오래된 줄부터 지움) */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text", meta = (ClampMin = "1"))
	int32 MaxNoticeLines = 5;

	/** 상세 제목. {Raid} = 레이드 이름. 예) "{Raid} 파티 모집 상세 정보" */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText DetailHeaderFormat;

	/** 신청자 탭 글. {Count} = 신청자 수. 예) "신청자 ({Count})" */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText ApplicantsTabFormat;

	/** 띠의 상태 글. 파티장·파티원 = "모집 중" / 신청자 = "신청 중" */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText RecruitingStatusText;

	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText ApplyingStatusText;

	/** 나가기 버튼 글. 파티장·파티원 = "파티에서 나가기" / 신청자 = "신청 취소" */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText LeaveText;

	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText CancelApplyText;

	/** 목록 안내. 검색 중 = "검색 중…" / 결과 없음 = "모집 중인 파티가 없습니다." */
	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText SearchingText;

	UPROPERTY(EditAnywhere, Category = "LOA|Party|Text")
	FText NoPartyText;

private:
	// ─── 서브시스템 알림 ───
	void HandlePartyChanged();
	void HandlePartiesFound(const TArray<FLOAPartyListEntry>& Listings);
	void HandlePartyNotice(const FLOAPartyNotice& Notice);

	// ─── 화면 맞추기 ───
	/** 띠·오른쪽·버튼·칸 (목록 줄은 그대로) */
	void RefreshParty();
	/** 상세 칸 (참가자 탭 = 정원만큼, 신청자 탭 = 신청자 수만큼) */
	void RefreshSlots();
	/** 상세 칸 하나를 만들어 SlotBox에 넣는다 (Recruit Slot Class가 비면 nullptr) */
	ULOAPartyRecruitSlotWidget* AddRecruitSlot();
	/** 아래 버튼 보이기·켜기 */
	void RefreshButtons();
	/** 목록 줄 다시 만들기 */
	void RebuildList(const TArray<FLOAPartyListEntry>& Listings);
	/** 목록 검색 시작 (이미 검색 중이면 그 결과를 기다림) */
	void StartSearch();
	/** 레이드 이름 (레이드 데이터. 못 찾으면 에셋 이름) */
	FText GetRaidName(const FPrimaryAssetId& RaidId) const;

	// ─── 버튼 ───
	UFUNCTION()
	void HandleCloseClicked();
	UFUNCTION()
	void HandleRefreshClicked();
	UFUNCTION()
	void HandleCreatePartyClicked();
	UFUNCTION()
	void HandleApplyClicked();
	UFUNCTION()
	void HandleLeaveClicked();
	UFUNCTION()
	void HandleEnterClicked();
	UFUNCTION()
	void HandleMembersTabClicked();
	UFUNCTION()
	void HandleApplicantsTabClicked();

	/** 탭 바꾸기 → 칸 다시 + BP 모양 */
	void SetApplicantsTab(bool bApplicants);

	// ─── 줄·칸에서 ───
	void HandleEntryClicked(int32 ListIndex);
	void HandleAcceptClicked(int32 MemberId);
	void HandleRejectClicked(int32 MemberId);
	void HandleKickClicked(int32 MemberId);

	/** 이 컴퓨터의 파티 (GameInstance 서브시스템이라 로비 맵이 다시 열려도 같은 것) */
	TWeakObjectPtr<ULOAPartySubsystem> PartySubsystem;

	/** 지금 목록 줄 (골라짐 표시용) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULOAPartyListEntryWidget>> EntryWidgets;

	/** 고른 줄 번호 (서브시스템 목록 번호와 같다). 없으면 INDEX_NONE */
	int32 SelectedListIndex = INDEX_NONE;

	/** 신청자 탭을 보는 중인가 (파티장만) */
	bool bShowingApplicants = false;

	/** 지난번 역할 (파티에서 빠지는 순간을 알려고) */
	ELOAPartyRole LastRole = ELOAPartyRole::None;

	/**
	 * 이 창에서 입장하기를 눌렀다 (파티장). 입장하기는 파티 정보를 비우고(역할 → None) 레이드 맵을 연다 →
	 * 그 역할 변화는 "파티에서 빠짐"이 아니므로 목록을 다시 검색하지 않는다 (11-20 MCP 테스트에서 발견)
	 */
	bool bEnterRequested = false;

	/** 보이는 알림 줄 (오래된 것부터) */
	TArray<FText> NoticeLines;
};
