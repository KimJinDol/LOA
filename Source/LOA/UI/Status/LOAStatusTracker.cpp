// LOAStatusTracker.cpp

#include "UI/Status/LOAStatusTracker.h"
#include "AbilitySystem/LOAStatusTags.h"
#include "AbilitySystem/LOAStatusReplicationComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAStatusTracker)

void ULOAStatusTracker::Init(UAbilitySystemComponent* InASC, const ULOAStatusReplicationComponent* InReplicatedTimes)
{
	Shutdown();

	ASC = InASC;
	ReplicatedTimes = InReplicatedTimes;
	if (!InASC)
	{
		return;
	}

	// 지금 붙어 있는 것은 "새로 붙음"으로 치지 않는다 (헤더 [알림])
	LastStatusTags = GetStatusTags();

	// 부모 태그 개수는 자식이 붙고 떨어질 때마다 바뀐다 → 상태 하나가 붙거나 떨어질 때마다 알림
	for (const FGameplayTag& ParentTag : LOAStatusTags::GetParentTags())
	{
		TagChangedHandles.Add(ParentTag,
			InASC->RegisterGameplayTagEvent(ParentTag, EGameplayTagEventType::AnyCountChange)
				.AddUObject(this, &ThisClass::HandleStatusTagChanged));
	}
}

void ULOAStatusTracker::Shutdown()
{
	if (UAbilitySystemComponent* MyASC = ASC.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : TagChangedHandles)
		{
			MyASC->UnregisterGameplayTagEvent(Pair.Value, Pair.Key, EGameplayTagEventType::AnyCountChange);
		}
	}
	TagChangedHandles.Reset();
	LastStatusTags.Reset();
	ASC.Reset();
	ReplicatedTimes.Reset();
}

void ULOAStatusTracker::HandleStatusTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const FGameplayTagContainer StatusTags = GetStatusTags();

	// 새로 붙은 것만 하나씩 (지난번 목록에 없던 태그. 헤더 [알림])
	//   목록을 먼저 바꾸고 알린다 → 듣는 쪽이 그 자리에서 GetStatusTags를 불러도 지금 목록
	const FGameplayTagContainer PreviousTags = LastStatusTags;
	LastStatusTags = StatusTags;
	for (const FGameplayTag& StatusTag : StatusTags)
	{
		if (!PreviousTags.HasTagExact(StatusTag))
		{
			OnStatusTagAdded.Broadcast(StatusTag);
		}
	}

	OnStatusTagsChanged.Broadcast(StatusTags);
}

FGameplayTagContainer ULOAStatusTracker::GetStatusTags() const
{
	FGameplayTagContainer StatusTags;
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return StatusTags;
	}

	// 붙은 태그 중 상태 태그만 (부모 태그 자체는 이름표가 아님)
	for (const FGameplayTag& OwnedTag : MyASC->GetOwnedGameplayTags())
	{
		if (LOAStatusTags::IsStatusTag(OwnedTag))
		{
			StatusTags.AddTag(OwnedTag);
		}
	}
	return StatusTags;
}

bool ULOAStatusTracker::GetStatusTimeRemaining(FGameplayTag StatusTag, float& OutTimeRemaining, float& OutDuration) const
{
	OutTimeRemaining = 0.f;
	OutDuration = 0.f;

	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC || !StatusTag.IsValid())
	{
		return false;
	}

	// 시간 복제 부품을 받았으면 거기서 (모든 컴퓨터가 같은 값)
	if (const ULOAStatusReplicationComponent* Times = ReplicatedTimes.Get())
	{
		return Times->GetStatusTimeRemaining(StatusTag, OutTimeRemaining, OutDuration);
	}

	// 그 태그를 주는 GE 중 가장 오래 남은 것 (이 컴퓨터에 GE가 없으면 빈 목록)
	const TArray<TPair<float, float>> Times = MyASC->GetActiveEffectsTimeRemainingAndDuration(
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(StatusTag)));

	bool bFound = false;
	for (const TPair<float, float>& Time : Times)
	{
		// 무한 효과는 전체 시간이 음수 (엔진: FGameplayEffectConstants::INFINITE_DURATION)
		if (Time.Value > 0.f && Time.Key > OutTimeRemaining)
		{
			OutTimeRemaining = Time.Key;
			OutDuration = Time.Value;
			bFound = true;
		}
	}
	return bFound;
}
