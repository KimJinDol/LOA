// LOADeathReactionComponent.cpp

#include "Character/LOADeathReactionComponent.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOADeathReactionComponent)

ULOADeathReactionComponent::ULOADeathReactionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 태그 알림으로만 움직인다. 매 프레임 할 일이 없다.
	PrimaryComponentTick.bCanEverTick = false;

	// 복제하지 않는다. 각 컴퓨터가 복제된 태그를 보고 각자 반응한다.
	SetIsReplicatedByDefault(false);
}

void ULOADeathReactionComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		LOA_LOG(GetOwner(), Warning, TEXT("[DeathReaction] InitializeWithAbilitySystem called with no ASC."));
		return;
	}

	// 같은 ASC로 다시 부르면 할 일 없음 / 다른 ASC면 이전 구독부터 끊는다.
	if (BoundASC.Get() == InASC)
	{
		return;
	}
	UninitializeFromAbilitySystem();

	BoundASC = InASC;

	// NewOrRemoved: 개수가 0 → 1 이상(붙음), 1 이상 → 0(떨어짐)일 때만 부른다. (2 → 3 같은 변화는 무시)
	DeadTagChangedHandle = InASC->RegisterGameplayTagEvent(LOAGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnDeadTagChanged);

	// 늦게 들어온 플레이어: 보스가 이미 죽은 상태로 복제되어 왔으면 구독 시점엔 알림이 다시 오지 않는다. → 지금 확인
	if (InASC->HasMatchingGameplayTag(LOAGameplayTags::State_Dead))
	{
		HandleDied();
	}
}

void ULOADeathReactionComponent::UninitializeFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->UnregisterGameplayTagEvent(DeadTagChangedHandle, LOAGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
	}

	DeadTagChangedHandle.Reset();
	BoundASC.Reset();
}

void ULOADeathReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void ULOADeathReactionComponent::OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		HandleDied();
	}
	else
	{
		HandleRevived();
	}
}

void ULOADeathReactionComponent::HandleDied()
{
	// 이미 시체면 다시 하지 않는다. (초기화 확인 + 태그 알림이 겹치는 경우)
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	AActor* Owner = GetOwner();

	// ① 충돌 끄기. 루트 충돌체 = 캐릭터의 캡슐 (판정 후보 검색도 이 충돌체로 찾는다)
	if (bDisableCollisionOnDeath)
	{
		if (UPrimitiveComponent* RootPrimitive = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr)
		{
			CollisionBeforeDeath = RootPrimitive->GetCollisionEnabled();
			RootPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// ② 이동 멈춤. 걷던 속도를 0으로 하고 이동 모드를 꺼서 AI가 이동을 시켜도 움직이지 않게 한다.
	//    (충돌을 껐으므로 이동 모드가 살아 있으면 바닥을 뚫고 떨어질 수 있다 → 반드시 같이 끈다)
	if (bStopMovementOnDeath)
	{
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
				Movement->DisableMovement();
			}
		}
	}

	LOA_LOG(Owner, Log, TEXT("[DeathReaction] Died. Collision and movement disabled."));

	// ③ 알림 (이 컴퓨터 안에서만)
	OnDied.Broadcast(Owner);
}

void ULOADeathReactionComponent::HandleRevived()
{
	if (!bIsDead)
	{
		return;
	}
	bIsDead = false;

	AActor* Owner = GetOwner();

	if (bDisableCollisionOnDeath)
	{
		if (UPrimitiveComponent* RootPrimitive = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr)
		{
			RootPrimitive->SetCollisionEnabled(CollisionBeforeDeath);
		}
	}

	if (bStopMovementOnDeath)
	{
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				// 캐릭터의 기본 이동 모드(걷기 등)로 되돌린다.
				Movement->SetDefaultMovementMode();
			}
		}
	}

	LOA_LOG(Owner, Log, TEXT("[DeathReaction] Revived. Collision and movement restored."));

	OnRevived.Broadcast(Owner);
}
