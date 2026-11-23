// LOAGEComponent_ShieldOverride.cpp

#include "AbilitySystem/Effects/Components/LOAGEComponent_ShieldOverride.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGEComponent_ShieldOverride)

void ULOAGEComponent_ShieldOverride::OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const
{
	UAbilitySystemComponent* ASC = ActiveGEContainer.Owner;
	if (!ActiveGEContainer.OwnerIsNetAuthority || !ASC)
	{
		return;
	}

	// 보낸 스킬을 모르면 덮어쓸 기준이 없다 → 그대로 쌓는다
	const FGameplayEffectContextHandle& NewContext = GESpec.GetContext();
	const UGameplayAbility* NewAbility = NewContext.GetAbility();
	if (!NewAbility)
	{
		return;
	}
	const AActor* NewInstigator = NewContext.GetInstigator();

	// 같은 GE 에셋으로 붙은 보호막들 중, 같은 힐러 + 같은 스킬이고 방금 붙은 것(같은 Spec)이 아닌 것 = 예전 보호막
	FGameplayEffectQuery SameEffectQuery;
	SameEffectQuery.EffectDefinition = GetOwner() ? GetOwner()->GetClass() : nullptr;

	TArray<FActiveGameplayEffectHandle> OldHandles;
	for (const FActiveGameplayEffectHandle& Handle : ActiveGEContainer.GetActiveEffects(SameEffectQuery))
	{
		const FActiveGameplayEffect* Effect = ActiveGEContainer.GetActiveGameplayEffect(Handle);
		if (!Effect || &Effect->Spec == &GESpec)
		{
			continue;	// 방금 붙은 새 보호막
		}
		const FGameplayEffectContextHandle& OldContext = Effect->Spec.GetContext();
		if (OldContext.GetAbility() == NewAbility && OldContext.GetInstigator() == NewInstigator)
		{
			OldHandles.Add(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : OldHandles)
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}

	if (OldHandles.Num() > 0)
	{
		LOA_LOG(ASC->GetAvatarActor(), Log, TEXT("[Shield] %s re-applied by %s: old shield replaced"),
			*GetNameSafe(NewAbility->GetClass()), *GetNameSafe(NewInstigator));
	}
}

#if WITH_EDITOR
EDataValidationResult ULOAGEComponent_ShieldOverride::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Instant GE는 대상에 남지 않아서 덮어쓸 예전 보호막도 없다
	const UGameplayEffect* Owner = GetOwner();
	if (Owner && Owner->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Shield Override needs Has Duration. An Instant effect does not stay, so there is nothing to replace."),
			*GetNameSafe(Owner))));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
