// LOAClassCardWidget.h
// [화면] 로비 직업 카드 한 장. 아이콘 + 이름, 누르면 그 직업을 고른다. (2026-11-14 apply113)
//
// [흐름] ULOACharacterSelectWidget이 직업마다 하나씩 만들고 Setup → 눌림은 OnClicked로 그 위젯에 알린다
//        (카드는 LocalPlayer를 모른다. 고르는 일은 화면 하나에서)
//        골라진 카드 표시는 BP 이벤트 OnSelectedChanged(테두리 색 등은 BP에서)
//
// [WBP] WBP_ClassCard (부모 = 이 클래스)
//   CardButton (Button, 필수)     : 카드 전체를 덮는 버튼
//   NameText   (TextBlock, 필수)  : 직업 이름 (DisplayName. 비었으면 에셋 이름 → 빈 직업 데이터도 구분됨)
//   IconImage  (Image, 선택)      : 직업 아이콘 (비었으면 숨김)
//
// [네트워크] 없음

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOAClassCardWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class ULOACharacterClassData;

/** 카드가 눌림. 인자 = 이 카드의 직업 */
DECLARE_DELEGATE_OneParam(FLOAClassCardClickedSignature, const ULOACharacterClassData* /*ClassData*/);

UCLASS(Abstract)
class LOA_API ULOAClassCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 카드가 보여 줄 직업 */
	void Setup(const ULOACharacterClassData* InClassData);

	/** 골라졌나 표시 → BP OnSelectedChanged */
	void SetSelected(bool bSelected);

	const ULOACharacterClassData* GetClassData() const { return ClassData; }

	/** 카드가 눌림 (만든 화면이 듣는다) */
	FLOAClassCardClickedSignature OnClicked;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** 골라짐 표시가 바뀜. 테두리·색은 BP에서 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Lobby")
	void OnSelectedChanged(bool bSelected);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

private:
	UFUNCTION()
	void HandleCardButtonClicked();

	UPROPERTY(Transient)
	TObjectPtr<const ULOACharacterClassData> ClassData;
};
