// LOASkillSlotWidget.cpp

#include "UI/Combat/LOASkillSlotWidget.h"
#include "UI/LOATooltipWidget.h"

#include "Components/Image.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillSlotWidget)

void ULOASkillSlotWidget::ShowSkill(const FLOASkillSlotInfo& SlotInfo)
{
	if (IconImage)
	{
		// Soft 참조를 엔진이 불러와 씌운다 (원래 BP 노드와 같음, 크기 맞추기 끔)
		IconImage->SetBrushFromSoftTexture(SlotInfo.DisplayInfo.Icon, false);
		IconImage->SetVisibility(ESlateVisibility::Visible);
	}

	if (TooltipWidgetClass)
	{
		// 툴팁은 이 슬롯 전체에 단다 (원래 BP와 같음: Target = 자기)
		ULOATooltipWidget::SetSkillTooltip(this, TooltipWidgetClass, SlotInfo);
	}
}

void ULOASkillSlotWidget::ShowEmpty()
{
	if (IconImage)
	{
		// Hidden: 자리는 차지한 채 안 보임 (원래 BP와 같음)
		IconImage->SetVisibility(ESlateVisibility::Hidden);
	}

	SetToolTip(nullptr);
}
