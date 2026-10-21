// LOAGE_Paralysis.cpp

#include "AbilitySystem/Effects/LOAGE_Paralysis.h"
#include "Core/LOAGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_Paralysis)

ULOAGE_Paralysis::ULOAGE_Paralysis()
{
	// 안전용 최대 시간. 정상이면 ULOAGA_Paralysis가 경직 시간(SetByCaller.Duration) 뒤 이 GE를 지운다.
	// 이 시간이 먼저 끝나면 GA가 경고하고 끝낸다. BP의 Duration Magnitude에서 바꿀 수 있다
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.f));

	// 밀리는 거리 기본값 (부모 칸). 경직은 살짝만
	DefaultDistance = 100.f;

	// Modifier 없음. 태그·면역 조건은 BP 에셋의 Components에서 (헤더 [면역])
}

#if WITH_EDITOR
EDataValidationResult ULOAGE_Paralysis::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// C++ 부모 자신(이 클래스의 CDO)은 BP 칸이 비어 있는 게 정상이라 검사하지 않는다
	if (GetClass() == ULOAGE_Paralysis::StaticClass())
	{
		return Result;
	}

	const FString OwnerName = GetNameSafe(GetClass());

	// ① 경직 태그를 붙여야 ULOAGA_Paralysis가 발동한다
	if (!GetGrantedTags().HasTagExact(LOAGameplayTags::State_CrowdControl_Paralysis))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Grant Tags must include %s. Otherwise nobody gets paralyzed."),
			*OwnerName, *LOAGameplayTags::State_CrowdControl_Paralysis.GetTag().ToString())));
		Result = EDataValidationResult::Invalid;
	}

	// ② 면역·피격 이상 조건 (빠지면 슈퍼아머 중에도 끊기거나, 누워 있는데 경직이 붙는다) → 경고만
	const UTargetTagRequirementsGameplayEffectComponent* Requirements = FindComponent<UTargetTagRequirementsGameplayEffectComponent>();
	const FGameplayTagContainer* IgnoreTags = Requirements ? &Requirements->ApplicationTagRequirements.IgnoreTags : nullptr;

	const FGameplayTag ExpectedIgnoreTags[] = {
		LOAGameplayTags::State_Immune_Paralysis,
		LOAGameplayTags::State_Immune_Push,
		LOAGameplayTags::State_CrowdControl_Push,
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
