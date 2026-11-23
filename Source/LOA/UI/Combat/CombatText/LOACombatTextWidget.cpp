// LOACombatTextWidget.cpp

#include "UI/Combat/CombatText/LOACombatTextWidget.h"
#include "UI/Status/LOAStatusInfoData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACombatTextWidget)

void ULOACombatTextWidget::Setup(const FLOACombatTextInfo& Info, const FLOAStatusInfo* StatusRow)
{
	const FText StatusName = StatusRow ? StatusRow->DisplayName : FText::GetEmpty();
	OnTextSet(Info, StatusName);
}
