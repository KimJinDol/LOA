// LOARaidResultCardWidget.h
// [화면] 클리어 결과의 사람 한 명 카드. 직업 아이콘, 이름, 직업 이름, 몫이 큰 칸 3줄 (2026-11-15 apply115)
//
// [흐름] ULOARaidResultWidget이 MVP 카드(디자이너에 놓은 것)와 나머지 사람 카드(만들어 넣음)에 Setup을 부른다
//   줄 = 그 사람의 몫(%)이 큰 칸부터 3개, 0%인 칸은 안 보임 (대신 정함). 남는 줄은 접는다
//   예) 버서커A 피해 60%, 카운터 50%, 나머지 0 → "준 피해 60%" / "카운터 성공 50%" / (접힘)
//
// [칸 이름] StatNames (WBP 기본값). 비어 있는 칸은 ELOARaidStat의 DisplayName
//   (DisplayName은 에디터에서만 남는 메타데이터라 패키징하면 영어 이름이 나온다 → 화면 글자는 StatNames에 넣는다)
//
// [WBP] WBP_RaidResultCard / WBP_RaidResultMvpCard (부모 = 이 클래스. MVP는 글자를 크게 한 다른 WBP)
//   PlayerNameText  (TextBlock, 필수)
//   ClassNameText   (TextBlock, 선택) : 직업 이름 (DisplayName)
//   ClassIconImage  (Image, 선택)     : 직업 아이콘 (직업 데이터 Icon, 사용자 결정: 캐릭터 그림 대신)
//   StatNameText_0 ~ 2, StatValueText_0 ~ 2 (TextBlock, 선택) : 줄 3개
//
// [네트워크] 없음 (받은 결과를 그리기만)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/LOARaidResultTypes.h"
#include "LOARaidResultCardWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS(Abstract)
class LOA_API ULOARaidResultCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 사람 결과로 채운다 */
	void Setup(const FLOARaidPlayerResult& Result);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** 칸 이름 (화면 글자). 비어 있는 칸은 enum DisplayName */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid")
	TMap<ELOARaidStat, FText> StatNames;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClassNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ClassIconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatNameText_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatNameText_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatNameText_2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatValueText_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatValueText_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatValueText_2;

private:
	/** 칸 이름: StatNames → 없으면 enum DisplayName */
	FText GetStatName(ELOARaidStat Stat) const;

	/** 줄 i의 (이름, 값) 글자. NativeOnInitialized에서 모은다 (줄 수 = 3). 위젯은 위 BindWidget 칸이 잡고 있어 여기는 가리키기만 */
	TArray<TPair<UTextBlock*, UTextBlock*>> StatRows;
};
