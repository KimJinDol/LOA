// LOATooltipWidget.cpp

#include "UI/LOATooltipWidget.h"
#include "UI/Status/LOAStatusInfoData.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "Core/LOALog.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOATooltipWidget)

// =============================================================================
// 숫자 모양
// =============================================================================

FText LOATooltipFormat::Decimal(float Value)
{
	FNumberFormattingOptions Options;
	Options.SetUseGrouping(false);
	Options.SetMinimumFractionalDigits(0);
	Options.SetMaximumFractionalDigits(1);	// 7.5 / 12 (12.0이 아니라)
	return FText::AsNumber(Value, &Options);
}

FText LOATooltipFormat::WholeNumber(float Value)
{
	FNumberFormattingOptions Options;
	Options.SetUseGrouping(true);			// 2,500
	Options.SetMaximumFractionalDigits(0);
	Options.SetRoundingMode(ERoundingMode::HalfFromZero);
	return FText::AsNumber(Value, &Options);
}

void LOATooltipFormat::FillSkillSlotInfo(FLOASkillSlotInfo& OutSlot, const FGameplayTag& InputTag, const ULOAPlayerAbility& PlayerAbility, float Level, float AttackPowerBase)
{
	OutSlot.InputTag = InputTag;
	OutSlot.DisplayInfo = PlayerAbility.GetSkillDisplayInfo();
	OutSlot.Cooldown = PlayerAbility.GetCooldownAtLevel(Level);
	OutSlot.ManaCost = PlayerAbility.GetManaCostAtLevel(Level);

	// 자리표시 셋을 늘 넣는다. 값이 없으면 빈 글자
	FFormatNamedArguments Args;
	Args.Add(TEXT("Cooldown"), OutSlot.Cooldown > 0.f ? Decimal(OutSlot.Cooldown) : FText::GetEmpty());
	Args.Add(TEXT("Mana"), OutSlot.ManaCost > 0.f ? WholeNumber(OutSlot.ManaCost) : FText::GetEmpty());

	// {Damage} = 공격력 BaseValue × 첫 DamageRate. 공격력을 모르면(0) 빈칸
	float DamageRate = 0.f;
	const bool bHasDamage = AttackPowerBase > 0.f && PlayerAbility.FindDamageRate(Level, DamageRate) && DamageRate > 0.f;
	Args.Add(TEXT("Damage"), bHasDamage ? WholeNumber(AttackPowerBase * DamageRate) : FText::GetEmpty());

	OutSlot.FilledDescription = FText::Format(FTextFormat(OutSlot.DisplayInfo.Description), Args);
}

// =============================================================================
// 붙이기
// =============================================================================

void ULOATooltipWidget::SetSkillTooltip(UWidget* Target, TSubclassOf<ULOATooltipWidget> TooltipClass, const FLOASkillSlotInfo& SlotInfo)
{
	if (!Target)
	{
		return;
	}

	// 빈 슬롯 → 툴팁 떼기 (아이콘이 없는 줄은 슬롯 목록에 오지 않으므로 이름으로 판단)
	if (SlotInfo.DisplayInfo.DisplayName.IsEmpty())
	{
		Target->SetToolTip(nullptr);
		return;
	}

	ULOATooltipWidget* Tooltip = FindOrCreate(Target, TooltipClass);
	if (!Tooltip)
	{
		return;
	}

	// 쿨·마나 줄: 둘 다 0이면 숨김 (빈 글자를 넘김)
	FText Info;
	if (!Tooltip->SkillInfoFormat.IsEmpty() && (SlotInfo.Cooldown > 0.f || SlotInfo.ManaCost > 0.f))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Cooldown"), LOATooltipFormat::Decimal(SlotInfo.Cooldown));
		Args.Add(TEXT("Mana"), LOATooltipFormat::WholeNumber(SlotInfo.ManaCost));
		Info = FText::Format(FTextFormat(Tooltip->SkillInfoFormat), Args);
	}

	// 무력화 줄 (2026-11-18). 예) 길로틴 스윙 High → "무력화 : 상"
	const FText Stagger = Tooltip->MakeStaggerLine(SlotInfo.DisplayInfo.StaggerGrade);

	Tooltip->SetContent(SlotInfo.DisplayInfo.DisplayName, Info, Stagger, SlotInfo.FilledDescription);
}

void ULOATooltipWidget::SetStatusTooltip(UWidget* Target, TSubclassOf<ULOATooltipWidget> TooltipClass, const FLOAStatusInfo& StatusInfo)
{
	ULOATooltipWidget* Tooltip = FindOrCreate(Target, TooltipClass);
	if (!Tooltip)
	{
		return;
	}

	Tooltip->SetContent(StatusInfo.DisplayName, FText::GetEmpty(), FText::GetEmpty(), StatusInfo.MakeDescription());
}

ULOATooltipWidget* ULOATooltipWidget::FindOrCreate(UWidget* Target, TSubclassOf<ULOATooltipWidget> TooltipClass)
{
	if (!Target || !TooltipClass)
	{
		LOA_LOG(Target, Warning, TEXT("[Tooltip] Target or TooltipClass is not set (%s)."), *GetNameSafe(Target));
		return nullptr;
	}

	// 이미 붙인 것이 있으면 다시 쓴다 (아이콘마다 하나)
	if (ULOATooltipWidget* Existing = Cast<ULOATooltipWidget>(Target->GetToolTip()))
	{
		if (Existing->IsA(TooltipClass))
		{
			return Existing;
		}
	}

	ULOATooltipWidget* NewTooltip = CreateWidget<ULOATooltipWidget>(Target, TooltipClass);
	if (NewTooltip)
	{
		// 엔진이 이 위젯을 SToolTip으로 감싸 아이콘에 단다 (UWidget::SetToolTip)
		Target->SetToolTip(NewTooltip);
	}
	return NewTooltip;
}

FText ULOATooltipWidget::MakeStaggerLine(ELOAStaggerGrade Grade) const
{
	const FText* GradeText = StaggerGradeTexts.Find(Grade);
	if (Grade == ELOAStaggerGrade::None || StaggerFormat.IsEmpty() || !GradeText || GradeText->IsEmpty())
	{
		return FText::GetEmpty();
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Grade"), *GradeText);
	return FText::Format(FTextFormat(StaggerFormat), Args);
}

void ULOATooltipWidget::SetContent(const FText& Title, const FText& Info, const FText& Stagger, const FText& Description)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (InfoText)
	{
		InfoText->SetText(Info);
		InfoText->SetVisibility(Info.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (StaggerText)
	{
		StaggerText->SetText(Stagger);
		StaggerText->SetVisibility(Stagger.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
		DescriptionText->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
