// LOARaidData.cpp

#include "Data/LOARaidData.h"

#include "Engine/AssetManager.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARaidData)

#define LOCTEXT_NAMESPACE "LOARaidData"

FPrimaryAssetId ULOARaidData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetPrimaryAssetTypeName(), GetFName());
}

FPrimaryAssetType ULOARaidData::GetPrimaryAssetTypeName()
{
	static const FPrimaryAssetType RaidDataType(TEXT("Raid"));
	return RaidDataType;
}

const ULOARaidData* ULOARaidData::LoadById(const FPrimaryAssetId& RaidId)
{
	// 다른 컴퓨터가 보낸 ID라 종류부터 본다 (직업 찾기 ALOAGameMode::FindSelectedClassData와 같은 방식)
	if (!RaidId.IsValid() || RaidId.PrimaryAssetType != GetPrimaryAssetTypeName() || !UAssetManager::IsInitialized())
	{
		return nullptr;
	}

	// 목록에 없는 이름이면 빈 경로 (DefaultGame.ini에 Raid 등록이 필요)
	const FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(RaidId);
	if (AssetPath.IsNull())
	{
		return nullptr;
	}
	return Cast<ULOARaidData>(AssetPath.TryLoad());
}

#if WITH_EDITOR
EDataValidationResult ULOARaidData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (DisplayName.IsEmpty())
	{
		Context.AddError(LOCTEXT("EmptyDisplayName", "DisplayName is empty (shown in the party finder)."));
		Result = EDataValidationResult::Invalid;
	}
	if (RaidMap.IsNull())
	{
		Context.AddError(LOCTEXT("EmptyRaidMap", "RaidMap is not set (Enter Raid opens it)."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
