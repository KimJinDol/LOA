// LOAStatusReplicationComponent.cpp

#include "AbilitySystem/LOAStatusReplicationComponent.h"
#include "AbilitySystem/LOAStatusTags.h"
#include "Core/LOANetTime.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStatusReplicationComponent)

ULOAStatusReplicationComponent::ULOAStatusReplicationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULOAStatusReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULOAStatusReplicationComponent, StatusTimes);
}

void ULOAStatusReplicationComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	// 클라는 받기만 한다
	if (!InASC || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	ASC = InASC;
	EffectAppliedHandle = InASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectApplied);
	EffectRemovedHandle = InASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::HandleEffectRemoved);

	// 이미 붙어 있던 것까지 한 번
	RebuildStatusTimes();
}

void ULOAStatusReplicationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* MyASC = ASC.Get())
	{
		MyASC->OnGameplayEffectAppliedDelegateToSelf.Remove(EffectAppliedHandle);
		MyASC->OnAnyGameplayEffectRemovedDelegate().Remove(EffectRemovedHandle);
	}
	EffectAppliedHandle.Reset();
	EffectRemovedHandle.Reset();
	ASC.Reset();

	Super::EndPlay(EndPlayReason);
}

bool ULOAStatusReplicationComponent::GrantsStatusTag(const FGameplayEffectSpec& Spec)
{
	FGameplayTagContainer GrantedTags;
	Spec.GetAllGrantedTags(GrantedTags);

	for (const FGameplayTag& Tag : GrantedTags)
	{
		if (LOAStatusTags::IsStatusTag(Tag))
		{
			return true;
		}
	}
	return false;
}

void ULOAStatusReplicationComponent::HandleEffectApplied(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	if (GrantsStatusTag(Spec))
	{
		RebuildStatusTimes();
	}
}

void ULOAStatusReplicationComponent::HandleEffectRemoved(const FActiveGameplayEffect& Effect)
{
	if (GrantsStatusTag(Effect.Spec))
	{
		RebuildStatusTimes();
	}
}

void ULOAStatusReplicationComponent::RebuildStatusTimes()
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	const UWorld* World = GetWorld();
	if (!MyASC || !World)
	{
		return;
	}

	// 서버의 지금 서버 시각 + GE 남은 시간 = 끝나는 서버 시각 (클라도 같은 시계로 뺀다)
	const double ServerTimeNow = LOANetTime::GetServerTimeNow(World);
	const float WorldTime = World->GetTimeSeconds();

	TArray<FLOAReplicatedStatusTime> NewTimes;
	for (const FActiveGameplayEffect& Effect : &MyASC->GetActiveGameplayEffects())
	{
		// 제거 중(제거 알림 순간에는 아직 목록에 있음)이거나 무한 GE는 뺀다
		if (Effect.IsPendingRemove || Effect.GetDuration() <= 0.f)
		{
			continue;
		}

		FGameplayTagContainer GrantedTags;
		Effect.Spec.GetAllGrantedTags(GrantedTags);

		const double EndServerTime = ServerTimeNow + Effect.GetTimeRemaining(WorldTime);
		for (const FGameplayTag& Tag : GrantedTags)
		{
			if (!LOAStatusTags::IsStatusTag(Tag))
			{
				continue;
			}

			// 같은 상태를 주는 GE가 여럿이면 가장 늦게 끝나는 것
			FLOAReplicatedStatusTime* Existing = NewTimes.FindByPredicate([&Tag](const FLOAReplicatedStatusTime& Time)
			{
				return Time.StatusTag == Tag;
			});
			if (!Existing)
			{
				FLOAReplicatedStatusTime& NewTime = NewTimes.AddDefaulted_GetRef();
				NewTime.StatusTag = Tag;
				NewTime.EndServerTime = EndServerTime;
				NewTime.Duration = Effect.GetDuration();
			}
			else if (EndServerTime > Existing->EndServerTime)
			{
				Existing->EndServerTime = EndServerTime;
				Existing->Duration = Effect.GetDuration();
			}
		}
	}

	StatusTimes = MoveTemp(NewTimes);
}

bool ULOAStatusReplicationComponent::GetStatusTimeRemaining(FGameplayTag StatusTag, float& OutTimeRemaining, float& OutDuration) const
{
	OutTimeRemaining = 0.f;
	OutDuration = 0.f;

	const FLOAReplicatedStatusTime* Time = StatusTimes.FindByPredicate([&StatusTag](const FLOAReplicatedStatusTime& Entry)
	{
		return Entry.StatusTag == StatusTag;
	});
	if (!Time)
	{
		return false;
	}

	const double TimeRemaining = Time->EndServerTime - LOANetTime::GetServerTimeNow(GetWorld());
	if (TimeRemaining <= 0.0)
	{
		return false;	// 끝났는데 제거 소식이 아직 안 옴
	}

	OutTimeRemaining = static_cast<float>(TimeRemaining);
	OutDuration = Time->Duration;
	return true;
}
