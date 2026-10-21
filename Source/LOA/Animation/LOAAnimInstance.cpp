// LOAAnimInstance.cpp

#include "Animation/LOAAnimInstance.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAnimInstance)

void ULOAAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		LOA_LOG(GetOwningActor(), Warning, TEXT("[Anim] InitializeWithAbilitySystem called with no ASC. Tag variables will not update."));
		return;
	}

	// 엔진이 하는 일: 이전 연결 해제 → 각 태그의 지금 개수로 변수 채우기 → 태그가 바뀔 때마다 변수 갱신
	GameplayTagPropertyMap.Initialize(this, ASC);
}

#if WITH_EDITOR
EDataValidationResult ULOAAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	// 연결 칸마다: 태그가 비었는지, 변수 이름이 이 AnimBP에 있는지, 형식이 bool/int/float인지
	GameplayTagPropertyMap.IsDataValid(this, Context);

	return (Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
