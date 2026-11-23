// LOAPartyRecruitSlotWidget.h
// [화면] 파티 모집 상세의 한 칸. 참가자(왕관·직업 아이콘·이름·강퇴 X) / 신청자(수락·거절) / 빈자리("모집중"). (2026-11-20 apply132)
//
// [흐름] ULOAPartyFinderWidget이 탭마다 칸을 만들고 ShowMember / ShowApplicant / ShowEmpty 중 하나를 부른다
//        버튼 눌림은 OnAcceptClicked / OnRejectClicked / OnKickClicked(번호)로 그 창에 알린다 (칸은 서브시스템을 모른다)
//        강퇴 X는 창이 "보여도 되나"를 정해 넘긴다 (파티장 화면 + 파티장 자신이 아닌 칸)
//
// [직업 아이콘]
//   멤버의 ClassId로 이미 불러온 직업 데이터를 찾는다 (로비 화면이 직업 목록을 불러 두었다: ULOACharacterSelectWidget::LoadClassList)
//   못 찾으면(아직 안 불러옴) 아이콘만 숨긴다
//
// [WBP] WBP_PartyRecruitSlot (부모 = 이 클래스)
//   NameText      (TextBlock, 필수) : 이름. 빈자리면 Empty Text ("모집중")
//   LeaderIcon    (아무 위젯, 선택) : 왕관. 파티장 칸만 보임
//   ClassIcon     (Image, 선택)     : 직업 아이콘
//   EmptyIcon     (아무 위젯, 선택) : 빈자리 표시 (사진의 "!")
//   AcceptButton / RejectButton (Button, 선택) : 신청자 칸만 보임
//   KickButton    (Button, 선택)    : 강퇴 X. 창이 허락한 참가자 칸만 보임
//   Class Defaults: Empty Text
//
// [네트워크] 없음 (각자 화면)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOAPartyRecruitSlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;
struct FLOAPartyMember;

/** 칸의 버튼이 눌림. 인자 = 그 칸 멤버의 번호 (MemberId) */
DECLARE_DELEGATE_OneParam(FLOARecruitSlotClickedSignature, int32 /*MemberId*/);

UCLASS(Abstract)
class LOA_API ULOAPartyRecruitSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 참가자 칸. bCanKick = 강퇴 X를 보일까 (창이 정함) */
	void ShowMember(const FLOAPartyMember& Member, bool bCanKick);

	/** 신청자 칸 (수락·거절 버튼). 파티장 화면에만 */
	void ShowApplicant(const FLOAPartyMember& Member);

	/** 빈자리 칸 ("모집중") */
	void ShowEmpty();

	/** 버튼 눌림 (만든 창 하나가 듣는다) */
	FLOARecruitSlotClickedSignature OnAcceptClicked;
	FLOARecruitSlotClickedSignature OnRejectClicked;
	FLOARecruitSlotClickedSignature OnKickClicked;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LeaderIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ClassIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AcceptButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RejectButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> KickButton;

	/** 빈자리 글 (사진의 "모집중"). 글은 WBP에 (코드에 화면 글을 두지 않는다) */
	UPROPERTY(EditAnywhere, Category = "LOA|Party")
	FText EmptyText;

private:
	/** 이름·왕관·직업 아이콘 (참가자·신청자 공통) */
	void ShowPerson(const FLOAPartyMember& Member);

	/** 버튼 셋 보이기 */
	void SetButtonsVisible(bool bAccept, bool bReject, bool bKick);

	UFUNCTION()
	void HandleAcceptClicked();

	UFUNCTION()
	void HandleRejectClicked();

	UFUNCTION()
	void HandleKickClicked();

	/** 이 칸의 멤버 번호 (빈자리면 INDEX_NONE) */
	int32 MemberId = INDEX_NONE;
};
