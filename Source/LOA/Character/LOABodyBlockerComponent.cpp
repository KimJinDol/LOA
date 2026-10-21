// LOABodyBlockerComponent.cpp

#include "Character/LOABodyBlockerComponent.h"
#include "Character/LOADeathReactionComponent.h"
#include "Core/LOACollisionChannels.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOABodyBlockerComponent)

ULOABodyBlockerComponent::ULOABodyBlockerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 막기만 한다. 물리 시뮬레이션·겹침 알림 없음
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(LOACollision::BodyBlock);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SetGenerateOverlapEvents(false);

	// 길찾기에 영향 없음 (보스가 움직이면 길이 바뀌는 일 없게) / 플레이어가 보스 위로 올라서지 못하게
	//   (2026-10-20 내비메시에 칠해 봤다가 되돌림: 보스가 걸을 때마다 내비메시 조각을 다시 만들어 보스 AI가 부들거림.
	//    플레이어가 보스를 돌아가는 일은 ULOAClickMoveComponent가 직접 한다)
	SetCanEverAffectNavigation(false);
	CanCharacterStepUpOn = ECB_No;

	// 보이지 않는다
	SetHiddenInGame(true);
}

void ULOABodyBlockerComponent::OnRegister()
{
	Super::OnRegister();

	// 주인 캡슐 크기에 맞추기 (BP에서 보스 캡슐 크기를 바꿔도 따라감)
	const AActor* OwnerActor = GetOwner();
	const UCapsuleComponent* OwnerCapsule = OwnerActor ? Cast<UCapsuleComponent>(OwnerActor->GetRootComponent()) : nullptr;
	if (bMatchOwnerCapsule && OwnerCapsule && OwnerCapsule != this)
	{
		const float Radius = FMath::Max(OwnerCapsule->GetUnscaledCapsuleRadius() + ExtraRadius, 1.f);
		const float HalfHeight = FMath::Max(OwnerCapsule->GetUnscaledCapsuleHalfHeight(), Radius);
		SetCapsuleSize(Radius, HalfHeight);
	}
}

void ULOABodyBlockerComponent::BeginPlay()
{
	Super::BeginPlay();

	// 시체에 막히지 않게: 주인의 사망 반응을 듣는다. (없으면 늘 켜져 있음. 예: 허수아비)
	ULOADeathReactionComponent* DeathReaction = GetOwner() ? GetOwner()->FindComponentByClass<ULOADeathReactionComponent>() : nullptr;
	if (!DeathReaction)
	{
		return;
	}
	DeathReaction->OnDied.AddDynamic(this, &ThisClass::OnOwnerDied);
	DeathReaction->OnRevived.AddDynamic(this, &ThisClass::OnOwnerRevived);

	// 이미 죽어 있으면(늦게 들어온 플레이어) 바로 끈다
	if (DeathReaction->IsDead())
	{
		OnOwnerDied(GetOwner());
	}
}

void ULOABodyBlockerComponent::SetSuppressed(bool bInSuppressed)
{
	bSuppressed = bInSuppressed;
	UpdateCollision();
}

void ULOABodyBlockerComponent::OnOwnerDied(AActor* OwnerActor)
{
	bOwnerDead = true;
	UpdateCollision();
}

void ULOABodyBlockerComponent::OnOwnerRevived(AActor* OwnerActor)
{
	bOwnerDead = false;
	UpdateCollision();
}

void ULOABodyBlockerComponent::UpdateCollision()
{
	SetCollisionEnabled((bOwnerDead || bSuppressed) ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}
