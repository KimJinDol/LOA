// LOACharacterClassData.cpp

#include "Data/LOACharacterClassData.h"
#include "Data/LOAAbilityDataSet.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACharacterClassData)

#define LOCTEXT_NAMESPACE "LOACharacterClassData"

FPrimaryAssetId ULOACharacterClassData::GetPrimaryAssetId() const
{
	// 에셋 매니저에서 쓰는 ID = (종류 이름, 에셋 이름)
	// 예: CharacterClass:DA_Class_Berserker
	return FPrimaryAssetId(GetPrimaryAssetTypeName(), GetFName());
}

FPrimaryAssetType ULOACharacterClassData::GetPrimaryAssetTypeName()
{
	// 프로젝트 설정의 Primary Asset Types to Scan (DefaultGame.ini)에 똑같이 등록되어 있어야 한다.
	static const FPrimaryAssetType ClassDataType(TEXT("CharacterClass"));
	return ClassDataType;
}

UAnimMontage* ULOACharacterClassData::FindMontage(const FGameplayTag& MontageTag) const
{
	const TObjectPtr<UAnimMontage>* Found = MontageTag.IsValid() ? Montages.Find(MontageTag) : nullptr;
	return Found ? Found->Get() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult ULOACharacterClassData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	auto AddError = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	// --- 기본 정보 ---
	if (!ClassTag.IsValid())
	{
		AddError(LOCTEXT("NoClassTag", "ClassTag is empty."));
	}

	// --- 외형 ---
	if (SkeletalMesh.IsNull())
	{
		AddError(LOCTEXT("NoMesh", "SkeletalMesh is empty."));
	}
	if (AnimClass.IsNull())
	{
		AddError(LOCTEXT("NoAnim", "AnimClass is empty."));
	}

	// --- 항상 장착 AbilityDataSet ---
	for (int32 Index = 0; Index < DefaultAbilityDataSets.Num(); ++Index)
	{
		if (!DefaultAbilityDataSets[Index])
		{
			AddError(FText::Format(LOCTEXT("EmptyDefaultSet", "DefaultAbilityDataSets[{0}] is empty."), FText::AsNumber(Index)));
		}
	}

	// --- 몽타주 표 ---
	for (const TPair<FGameplayTag, TObjectPtr<UAnimMontage>>& Pair : Montages)
	{
		if (!Pair.Key.IsValid())
		{
			AddError(LOCTEXT("EmptyMontageTag", "Montages: a row has an empty tag."));
		}
		else if (!Pair.Value)
		{
			AddError(FText::Format(LOCTEXT("EmptyMontage", "Montages[{0}]: montage is empty."), FText::FromName(Pair.Key.GetTagName())));
		}
	}

	// --- 직업 전용 컴포넌트 ---
	for (int32 Index = 0; Index < ClassComponents.Num(); ++Index)
	{
		if (ClassComponents[Index].ComponentClass.IsNull())
		{
			AddError(FText::Format(LOCTEXT("EmptyComponent", "ClassComponents[{0}]: ComponentClass is empty."), FText::AsNumber(Index)));
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
