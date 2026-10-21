// LOAAnimNotify_SendGameplayEvent.cpp

#include "Animation/LOAAnimNotify_SendGameplayEvent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAnimNotify_SendGameplayEvent)

void ULOAAnimNotify_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner || !EventTag.IsValid())
	{
		return;
	}

	// 메시 주인의 ASC 찾기 (IAbilitySystemInterface → 없으면 컴포넌트 검색)
	// 애니메이션 에디터 미리보기에서는 ASC가 없다 → 조용히 무시 (에러 로그 도배 방지)
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!ASC)
	{
		return;
	}

	// 이벤트 내용물. 받는 쪽에서 "누가 보냈나"를 알 수 있게 Instigator를 채운다.
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;

	// 이 ASC에서 EventTag를 기다리는 Task(WaitGameplayEvent)에 전달
	ASC->HandleGameplayEvent(EventTag, &Payload);
}

FString ULOAAnimNotify_SendGameplayEvent::GetNotifyName_Implementation() const
{
	// 타임라인에 "Event.Montage.Hit"처럼 태그 이름이 보인다.
	return EventTag.IsValid() ? EventTag.ToString() : Super::GetNotifyName_Implementation();
}

bool ULOAAnimNotify_SendGameplayEvent::FindEventTimeInMontage(
	const UAnimMontage* Montage, const FGameplayTag& InEventTag, float FromTime, float& OutTime)
{
	if (!Montage || !InEventTag.IsValid())
	{
		return false;
	}

	bool bFound = false;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const ULOAAnimNotify_SendGameplayEvent* SendEvent = Cast<ULOAAnimNotify_SendGameplayEvent>(NotifyEvent.Notify);
		if (!SendEvent || SendEvent->GetEventTag() != InEventTag)
		{
			continue;
		}

		// 몽타주 타임라인에서 이 노티파이가 울리는 시각 (초)
		const float TriggerTime = NotifyEvent.GetTriggerTime();
		if (TriggerTime >= FromTime && (!bFound || TriggerTime < OutTime))
		{
			OutTime = TriggerTime;
			bFound = true;
		}
	}
	return bFound;
}
