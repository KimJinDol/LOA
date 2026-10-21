// LOATelegraphStyle.cpp

#include "Combat/Telegraph/LOATelegraphStyle.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOATelegraphStyle)

#if WITH_EDITOR
EDataValidationResult ULOATelegraphStyle::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (DecalLayers.IsEmpty())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Decal Layers is empty. Nothing will be drawn."), *GetName())));
		Result = EDataValidationResult::Invalid;
	}

	for (int32 Index = 0; Index < DecalLayers.Num(); ++Index)
	{
		if (!DecalLayers[Index])
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("%s : Decal Layers[%d] has no material."), *GetName(), Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
