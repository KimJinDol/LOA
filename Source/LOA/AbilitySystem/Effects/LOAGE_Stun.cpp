// LOAGE_Stun.cpp

#include "AbilitySystem/Effects/LOAGE_Stun.h"
#include "AbilitySystem/Calculations/LOAStunDurationMagnitude.h"
#include "Core/LOAGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_Stun)

ULOAGE_Stun::ULOAGE_Stun()
{
	// 시간 = Payload의 SetByCaller.Duration, 없으면 Default Duration (헤더 [시간])
	// 엔진이 GE를 붙일 때 시간을 다시 계산한다 → Spec을 만든 뒤 넣은 SetByCaller도 반영됨 (GameplayEffect.cpp ApplyGameplayEffectSpec)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FCustomCalculationBasedFloat DurationCalculation;
	DurationCalculation.CalculationClassMagnitude = ULOAStunDurationMagnitude::StaticClass();
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationCalculation);

	// Modifier 없음. 태그·면역 조건은 BP 에셋의 Components에서 (헤더 [면역])
}

#if WITH_EDITOR
EDataValidationResult ULOAGE_Stun::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// C++ 부모 자신(이 클래스의 CDO)은 BP 칸이 비어 있는 게 정상이라 검사하지 않는다
	if (GetClass() == ULOAGE_Stun::StaticClass())
	{
		return Result;
	}

	const FString OwnerName = GetNameSafe(GetClass());

	// ① 스턴 태그를 붙여야 ULOAGA_Stun이 발동한다
	if (!GetGrantedTags().HasTagExact(LOAGameplayTags::State_CrowdControl_Status_Stunned))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Grant Tags must include %s. Otherwise nobody gets stunned."),
			*OwnerName, *LOAGameplayTags::State_CrowdControl_Status_Stunned.GetTag().ToString())));
		Result = EDataValidationResult::Invalid;
	}

	// ② 시간 계산기를 바꾸면 Payload의 시간이 안 먹는다 → 경고만
	if (DurationMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::CustomCalculationClass
		|| DurationMagnitude.GetCustomMagnitudeCalculationClass() != ULOAStunDurationMagnitude::StaticClass())
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Duration Magnitude should stay Custom Calculation Class = LOAStunDurationMagnitude (Payload SetByCaller.Duration, else Default Duration)."),
			*OwnerName)));
	}

	// ③ 면역·피격 이상 조건 → 경고만
	const UTargetTagRequirementsGameplayEffectComponent* Requirements = FindComponent<UTargetTagRequirementsGameplayEffectComponent>();
	const FGameplayTagContainer* IgnoreTags = Requirements ? &Requirements->ApplicationTagRequirements.IgnoreTags : nullptr;

	const FGameplayTag ExpectedIgnoreTags[] = {
		LOAGameplayTags::State_Immune_Status,
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
