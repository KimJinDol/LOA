// LOAClassCardWidget.cpp

#include "UI/Lobby/LOAClassCardWidget.h"
#include "Data/LOACharacterClassData.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAClassCardWidget)

void ULOAClassCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CardButton)
	{
		CardButton->OnClicked.AddDynamic(this, &ThisClass::HandleCardButtonClicked);
	}
}

void ULOAClassCardWidget::Setup(const ULOACharacterClassData* InClassData)
{
	ClassData = InClassData;
	if (!ClassData)
	{
		return;
	}

	if (NameText)
	{
		// 이름이 빈 직업 데이터(아직 안 채운 원거리 등)도 카드끼리 구분되게 에셋 이름으로
		NameText->SetText(ClassData->DisplayName.IsEmpty() ? FText::FromName(ClassData->GetFName()) : ClassData->DisplayName);
	}

	if (IconImage)
	{
		if (ClassData->Icon.IsNull())
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			// Soft 참조를 엔진이 불러와 씌운다
			IconImage->SetBrushFromSoftTexture(ClassData->Icon);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void ULOAClassCardWidget::SetSelected(bool bSelected)
{
	OnSelectedChanged(bSelected);
}

void ULOAClassCardWidget::HandleCardButtonClicked()
{
	OnClicked.ExecuteIfBound(ClassData);
}
