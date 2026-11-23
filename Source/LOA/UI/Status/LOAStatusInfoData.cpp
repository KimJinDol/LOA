// LOAStatusInfoData.cpp

#include "UI/Status/LOAStatusInfoData.h"
#include "Core/LOAGameplayTags.h"
#include "UI/LOATooltipWidget.h"

#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStatusInfoData)

bool FLOAStatusInfo::IsDebuff() const
{
	// 상태 이상(스턴 등)도 해로운 쪽 (2026-11-10)
	return StatusTag.MatchesTag(LOAGameplayTags::State_Debuff) || StatusTag.MatchesTag(LOAGameplayTags::State_CrowdControl_Status);
}

FText FLOAStatusInfo::MakeDescription() const
{
	// 레벨 1 값 (상태 아이콘은 누가 몇 레벨로 걸었는지 모른다)
	constexpr float Level = 1.f;

	// {Value0} {Value1} … = Values 순서, {Value} = {Value0}. 예) 0.15 → 15 / 1.06(곱하기) → 6
	FFormatNamedArguments Args;
	FText FirstValueText;	// {Value}. Values가 비면 빈 글자 (넣지 않으면 "{Value}" 글자가 그대로 남는다)
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const FLOATooltipValue& TooltipValue = Values[Index];
		const float RawValue = TooltipValue.Value.GetValueAtLevel(Level);
		const float Percent = FMath::Abs((TooltipValue.bMultiply ? RawValue - 1.f : RawValue) * 100.f);	// 부호는 글이 말한다
		const FText ValueText = LOATooltipFormat::Decimal(Percent);
		Args.Add(FString::Printf(TEXT("Value%d"), Index), ValueText);
		if (Index == 0)
		{
			FirstValueText = ValueText;
		}
	}
	Args.Add(TEXT("Value"), FirstValueText);

	// {Duration} = Effect의 고정 지속 시간. 시간이 있는 GE만
	FText DurationText;
	float Duration = 0.f;
	const UGameplayEffect* EffectCDO = Effect ? Effect->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (EffectCDO && EffectCDO->DurationPolicy == EGameplayEffectDurationType::HasDuration
		&& EffectCDO->DurationMagnitude.GetStaticMagnitudeIfPossible(Level, Duration))
	{
		DurationText = LOATooltipFormat::Decimal(Duration);
	}
	Args.Add(TEXT("Duration"), DurationText);

	return FText::Format(FTextFormat(Description), Args);
}

const FLOAStatusInfo* ULOAStatusInfoData::FindRow(const FGameplayTag& StatusTag) const
{
	// 몇 줄 안 되므로 그냥 훑는다
	for (const FLOAStatusInfo& Row : Rows)
	{
		if (Row.StatusTag == StatusTag)
		{
			return &Row;
		}
	}
	return nullptr;
}

#if WITH_EDITOR
EDataValidationResult ULOAStatusInfoData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 툴팁 숫자는 GE·스킬과 같은 커브 행을 가리켜야 한 곳에서 고쳐진다. 숫자를 직접 적은 칸은 경고 (헤더 [툴팁])
	for (const FLOAStatusInfo& Info : Rows)
	{
		for (int32 Index = 0; Index < Info.Values.Num(); ++Index)
		{
			if (Info.Values[Index].Value.Curve.CurveTable == nullptr)
			{
				Context.AddWarning(FText::FromString(FString::Printf(
					TEXT("%s : %s Values[%d] is a plain number. Point it to the same curve table row as the gameplay effect so the tooltip cannot drift."),
					*GetName(), *Info.StatusTag.ToString(), Index)));
			}
		}
	}
	return Result;
}
#endif
