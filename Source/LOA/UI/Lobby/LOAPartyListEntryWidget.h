// LOAPartyListEntryWidget.h
// [화면] 파티 찾기 목록 한 줄. 목표(레이드 이름) / 왕관 + 파티장 이름 + 한 줄 글 / 인원 "1 / 3". 누르면 고른다. (2026-11-20 apply132)
//
// [흐름] ULOAPartyFinderWidget이 검색 결과 한 줄마다 만들고 Setup → 눌림은 OnClicked(번호)로 그 창에 알린다
//        (이 줄은 서브시스템을 모른다. 신청은 창의 "신청하기" 버튼이 한다)
//        골라짐 표시는 BP 이벤트 OnSelectedChanged (배경색 등은 BP에서)
//
// [WBP] WBP_PartyListEntry (부모 = 이 클래스)
//   EntryButton      (Button, 필수)    : 줄 전체를 덮는 버튼
//   RaidNameText     (TextBlock, 필수) : 목표 = 레이드 이름 ("부활한 마수의 심장")
//   LeaderNameText   (TextBlock, 필수) : 파티장 이름 (왕관 그림은 WBP에 고정)
//   TitleText        (TextBlock, 필수) : 한 줄 글
//   MemberCountText  (TextBlock, 필수) : "1 / 3"
//
// [네트워크] 없음 (각자 화면)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOAPartyListEntryWidget.generated.h"

class UButton;
class UTextBlock;
struct FLOAPartyListEntry;

/** 줄이 눌림. 인자 = 목록 번호 (ULOAPartySubsystem::ApplyToParty에 넘기는 번호) */
DECLARE_DELEGATE_OneParam(FLOAPartyListEntryClickedSignature, int32 /*ListIndex*/);

UCLASS(Abstract)
class LOA_API ULOAPartyListEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 줄이 보여 줄 방. RaidName = 창이 레이드 데이터에서 찾은 이름 (못 찾으면 빈 글자) */
	void Setup(const FLOAPartyListEntry& Entry, int32 InListIndex, const FText& RaidName);

	/** 골라졌나 표시 → BP OnSelectedChanged */
	void SetSelected(bool bSelected);

	/** 줄이 눌림 (만든 창 하나가 듣는다) */
	FLOAPartyListEntryClickedSignature OnClicked;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** 골라짐 표시가 바뀜. 배경·글자색은 BP에서 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Party")
	void OnSelectedChanged(bool bSelected);

	/** BP OnSelectedChanged가 배경색을 바꾸므로 BP에서 읽기 (작업 규칙: BindWidget을 BP 그래프에서 쓰면 BlueprintReadOnly) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> EntryButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RaidNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LeaderNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MemberCountText;

private:
	UFUNCTION()
	void HandleEntryButtonClicked();

	/** 목록 번호 */
	int32 ListIndex = INDEX_NONE;
};
