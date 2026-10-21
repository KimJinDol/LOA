// LOACharacterClassData.cpp

#include "Data/LOACharacterClassData.h"
#include "Data/LOAAbilityDataSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACharacterClassData)

#define LOCTEXT_NAMESPACE "LOACharacterClassData"

FPrimaryAssetId ULOACharacterClassData::GetPrimaryAssetId() const
{
	// 에셋 매니저에서 쓰는 ID = (종류 이름, 에셋 이름)
	// 예: CharacterClass:DA_Class_Berserker
	// 종류 이름 "CharacterClass"는 프로젝트 설정의 Primary Asset Types to Scan 에 똑같이 등록해야 한다.
	static const FPrimaryAssetType ClassDataType(TEXT("CharacterClass"));
	return FPrimaryAssetId(ClassDataType, GetFName());
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
