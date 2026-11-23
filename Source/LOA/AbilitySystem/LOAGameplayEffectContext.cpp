// LOAGameplayEffectContext.cpp

#include "AbilitySystem/LOAGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGameplayEffectContext)

FLOAGameplayEffectContext* FLOAGameplayEffectContext::Get(FGameplayEffectContextHandle& Handle)
{
	// 핸들 안의 쪽지 종류가 우리 쪽지(또는 그 자식)일 때만 바꿔서 돌려준다
	FGameplayEffectContext* Context = Handle.Get();
	if (Context && Context->GetScriptStruct()->IsChildOf(FLOAGameplayEffectContext::StaticStruct()))
	{
		return static_cast<FLOAGameplayEffectContext*>(Context);
	}
	return nullptr;
}

const FLOAGameplayEffectContext* FLOAGameplayEffectContext::Get(const FGameplayEffectContextHandle& Handle)
{
	const FGameplayEffectContext* Context = Handle.Get();
	if (Context && Context->GetScriptStruct()->IsChildOf(FLOAGameplayEffectContext::StaticStruct()))
	{
		return static_cast<const FLOAGameplayEffectContext*>(Context);
	}
	return nullptr;
}

FLOAGameplayEffectContext* FLOAGameplayEffectContext::Duplicate() const
{
	// 엔진 쪽지의 Duplicate와 같은 순서: 통째로 복사 → 맞은 지점은 새로 만들어 넣는다 (같은 HitResult를 둘이 나눠 쓰지 않게)
	FLOAGameplayEffectContext* NewContext = new FLOAGameplayEffectContext();
	*NewContext = *this;
	if (GetHitResult())
	{
		NewContext->AddHitResult(*GetHitResult(), true);
	}
	return NewContext;
}
