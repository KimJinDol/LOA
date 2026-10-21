// LOARaidSubsystem.cpp

#include "Game/LOARaidSubsystem.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Character/LOADeathReactionComponent.h"
#include "Actors/LOALocationMarker.h"
#include "Core/LOALog.h"

#include "Engine/World.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARaidSubsystem)

ULOARaidSubsystem* ULOARaidSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOARaidSubsystem>() : nullptr;
}

bool ULOARaidSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOARaidSubsystem::RegisterBoss(ALOABossCharacter* Boss)
{
	if (!Boss || CurrentBoss.Get() == Boss)
	{
		return;
	}

	if (ALOABossCharacter* OldBoss = CurrentBoss.Get())
	{
		LOA_LOG(Boss, Warning, TEXT("[Raid] Boss %s replaces %s. Only one boss at a time is supported."),
			*GetNameSafe(Boss), *GetNameSafe(OldBoss));

		// 예전 보스의 사망 알림은 더 이상 듣지 않는다.
		if (ULOADeathReactionComponent* OldDeath = OldBoss->GetDeathReactionComponent())
		{
			OldDeath->OnDied.RemoveDynamic(this, &ThisClass::HandleBossDied);
		}
	}

	CurrentBoss = Boss;
	bCurrentBossDefeated = false;
	LOA_LOG(Boss, Log, TEXT("[Raid] Boss registered: %s"), *GetNameSafe(Boss));

	// ① 보스가 바뀌었다 (체력바)
	OnBossChanged.Broadcast(Boss);

	// ② 보스의 사망 알림 구독. 이미 죽은 채로 등록됐으면(늦게 들어온 플레이어) 바로 토벌 완료.
	if (ULOADeathReactionComponent* Death = Boss->GetDeathReactionComponent())
	{
		Death->OnDied.AddUniqueDynamic(this, &ThisClass::HandleBossDied);

		if (Death->IsDead())
		{
			MarkCurrentBossDefeated();
		}
	}
	else
	{
		LOA_LOG(Boss, Warning, TEXT("[Raid] Boss %s has no DeathReactionComponent. OnBossDefeated will not fire."), *GetNameSafe(Boss));
	}
}

void ULOARaidSubsystem::RegisterLocation(ALOALocationMarker* Marker)
{
	if (!Marker || !Marker->GetLocationTag().IsValid())
	{
		return;
	}

	const FGameplayTag& Tag = Marker->GetLocationTag();
	if (const TWeakObjectPtr<ALOALocationMarker>* Existing = Locations.Find(Tag))
	{
		if (Existing->IsValid() && Existing->Get() != Marker)
		{
			LOA_LOG(Marker, Warning, TEXT("[Location] %s is used by both %s and %s. Using %s."),
				*Tag.ToString(), *GetNameSafe(Existing->Get()), *GetNameSafe(Marker), *GetNameSafe(Marker));
		}
	}

	Locations.Add(Tag, Marker);
	LOA_LOG(Marker, Log, TEXT("[Location] %s registered at %s"), *Tag.ToString(), *Marker->GetActorLocation().ToCompactString());
}

void ULOARaidSubsystem::UnregisterLocation(ALOALocationMarker* Marker)
{
	if (!Marker)
	{
		return;
	}

	const TWeakObjectPtr<ALOALocationMarker>* Existing = Locations.Find(Marker->GetLocationTag());
	if (Existing && Existing->Get() == Marker)
	{
		Locations.Remove(Marker->GetLocationTag());
	}
}

bool ULOARaidSubsystem::FindLocation(const FGameplayTag& LocationTag, FVector& OutLocation) const
{
	const TWeakObjectPtr<ALOALocationMarker>* Found = Locations.Find(LocationTag);
	if (!Found || !Found->IsValid())
	{
		return false;
	}

	OutLocation = Found->Get()->GetActorLocation();
	return true;
}

void ULOARaidSubsystem::UnregisterBoss(ALOABossCharacter* Boss)
{
	// 다른 보스로 이미 바뀌었으면(교체 후 예전 보스가 사라지는 경우) 지금 보스를 건드리지 않는다.
	if (!Boss || CurrentBoss.Get() != Boss)
	{
		return;
	}

	if (ULOADeathReactionComponent* Death = Boss->GetDeathReactionComponent())
	{
		Death->OnDied.RemoveDynamic(this, &ThisClass::HandleBossDied);
	}

	CurrentBoss.Reset();
	bCurrentBossDefeated = false;
	LOA_LOG(Boss, Log, TEXT("[Raid] Boss unregistered: %s"), *GetNameSafe(Boss));

	OnBossChanged.Broadcast(nullptr);
}

void ULOARaidSubsystem::HandleBossDied(AActor* DeadActor)
{
	// 지금 보스의 알림만 받는다. (교체된 예전 보스는 위에서 구독을 끊지만, 한 번 더 확인)
	if (!DeadActor || DeadActor != CurrentBoss.Get())
	{
		return;
	}

	MarkCurrentBossDefeated();
}

void ULOARaidSubsystem::MarkCurrentBossDefeated()
{
	// 한 보스에 한 번만 (등록 직후 확인 + 사망 알림이 겹치는 경우)
	if (bCurrentBossDefeated)
	{
		return;
	}
	bCurrentBossDefeated = true;

	ALOABossCharacter* Boss = CurrentBoss.Get();
	LOA_LOG(Boss, Log, TEXT("[Raid] Boss defeated: %s"), *GetNameSafe(Boss));

	OnBossDefeated.Broadcast(Boss);
}
