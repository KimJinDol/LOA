// LOARaidResultCardWidget.cpp

#include "UI/Raid/LOARaidResultCardWidget.h"
#include "Data/LOACharacterClassData.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARaidResultCardWidget)

void ULOARaidResultCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	StatRows = {
		{ StatNameText_0.Get(), StatValueText_0.Get() },
		{ StatNameText_1.Get(), StatValueText_1.Get() },
		{ StatNameText_2.Get(), StatValueText_2.Get() },
	};
}

void ULOARaidResultCardWidget::Setup(const FLOARaidPlayerResult& Result)
{
	if (PlayerNameText)
	{
		PlayerNameText->SetText(FText::FromString(Result.PlayerName));
	}

	const ULOACharacterClassData* ClassData = Result.ClassData;
	if (ClassNameText)
	{
		ClassNameText->SetText(ClassData ? ClassData->DisplayName : FText::GetEmpty());
	}

	if (ClassIconImage)
	{
		if (!ClassData || ClassData->Icon.IsNull())
		{
			ClassIconImage->SetVisibility(ESlateVisibility::Hidden);	// 자리는 남긴다 (카드 모양 유지)
		}
		else
		{
			ClassIconImage->SetBrushFromSoftTexture(ClassData->Icon);	// Soft 참조를 엔진이 불러와 씌운다 (로비 카드와 같은 방식)
			ClassIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// 몫이 큰 칸부터 줄 수만큼. 예) 0.443 → "44%"
	FNumberFormattingOptions PercentFormat;
	PercentFormat.SetMaximumFractionalDigits(0);

	const TArray<ELOARaidStat> TopStats = Result.GetTopStats(StatRows.Num());
	for (int32 Row = 0; Row < StatRows.Num(); ++Row)
	{
		UTextBlock* NameText = StatRows[Row].Key;
		UTextBlock* ValueText = StatRows[Row].Value;
		const bool bHasStat = TopStats.IsValidIndex(Row);

		if (NameText)
		{
			NameText->SetText(bHasStat ? GetStatName(TopStats[Row]) : FText::GetEmpty());
			NameText->SetVisibility(bHasStat ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (ValueText)
		{
			ValueText->SetText(bHasStat ? FText::AsPercent(Result.GetShare(TopStats[Row]), &PercentFormat) : FText::GetEmpty());
			ValueText->SetVisibility(bHasStat ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

FText ULOARaidResultCardWidget::GetStatName(ELOARaidStat Stat) const
{
	if (const FText* Name = StatNames.Find(Stat))
	{
		if (!Name->IsEmpty())
		{
			return *Name;
		}
	}
	return StaticEnum<ELOARaidStat>()->GetDisplayNameTextByValue(static_cast<int64>(Stat));
}
