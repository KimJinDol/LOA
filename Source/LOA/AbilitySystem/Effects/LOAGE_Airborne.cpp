// LOAGE_Airborne.cpp

#include "AbilitySystem/Effects/LOAGE_Airborne.h"
#include "Core/LOAGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_Airborne)

ULOAGE_Airborne::ULOAGE_Airborne()
{
	// 안전용 최대 시간. 정상이면 ULOAGA_Airborne이 다 일어난 뒤 이 GE를 지운다 (누워 있는 시간은 SetByCaller.DownDuration).
	// 이 시간이 먼저 끝나면 = 착지를 못 하고 있다 (맵 밖으로 떨어지는 중 등) → GA가 경고하고 끝낸다. BP의 Duration Magnitude에서 바꿀 수 있다
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));

	// 공중에서 가는 거리 기본값 (부모 칸). 0이면 제자리에서 위로만
	DefaultDistance = 300.f;

	// Modifier 없음. 태그·면역 조건은 BP 에셋의 Components에서 (헤더 [면역])
}

#if WITH_EDITOR
EDataValidationResult ULOAGE_Airborne::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// C++ 부모 자신(이 클래스의 CDO)은 BP 칸이 비어 있는 게 정상이라 검사하지 않는다
	if (GetClass() == ULOAGE_Airborne::StaticClass())
	{
		return Result;
	}

	const FString OwnerName = GetNameSafe(GetClass());

	// ① 띄움 태그를 붙여야 ULOAGA_Airborne이 발동한다
	if (!GetGrantedTags().HasTagExact(LOAGameplayTags::State_CrowdControl_Push_Airborne))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Grant Tags must include %s. Otherwise nobody gets launched."),
			*OwnerName, *LOAGameplayTags::State_CrowdControl_Push_Airborne.GetTag().ToString())));
		Result = EDataValidationResult::Invalid;
	}

	// ② 면역·누움 조건 (빠지면 회피 중에도 뜨거나, 누워 있는데 또 뜬다) → 경고만
	const UTargetTagRequirementsGameplayEffectComponent* Requirements = FindComponent<UTargetTagRequirementsGameplayEffectComponent>();
	const FGameplayTagContainer* IgnoreTags = Requirements ? &Requirements->ApplicationTagRequirements.IgnoreTags : nullptr;

	const FGameplayTag ExpectedIgnoreTags[] = {
		LOAGameplayTags::State_Immune_Push,
		LOAGameplayTags::State_CrowdControl_Push_Down,
	};
	for (const FGameplayTag& Expected : ExpectedIgnoreTags)
	{
		if (!IgnoreTags || !IgnoreTags->HasTagExact(Expected))
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s : Target Tag Requirements > Application > Ignore Tags should include %s."),
				*OwnerName, *Expected.ToString())));
		}
	}

	return Result;
}
#endif
