// LOARaidSubsystem.cpp

#include "Game/LOARaidSubsystem.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Character/LOADeathReactionComponent.h"
#include "Actors/LOALocationMarker.h"
#include "Actors/LOATerrainZone.h"
#include "Actors/LOABrawlArena.h"
#include "Character/Player/LOACharacter.h"
#include "Core/LOALog.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"

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

bool ULOARaidSubsystem::FindLocationTransform(const FGameplayTag& LocationTag, FTransform& OutTransform) const
{
	const TWeakObjectPtr<ALOALocationMarker>* Found = Locations.Find(LocationTag);
	if (!Found || !Found->IsValid())
	{
		return false;
	}

	OutTransform = Found->Get()->GetActorTransform();
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

// =============================================================================
// 지형 구역 (헤더 [지형 구역])
// =============================================================================

void ULOARaidSubsystem::RegisterBrawlArena(ALOABrawlArena* Arena)
{
	if (!Arena)
	{
		return;
	}

	if (BrawlArena.IsValid() && BrawlArena.Get() != Arena)
	{
		LOA_LOG(Arena, Warning, TEXT("[Brawl] Another brawl arena (%s) is already registered. Replacing it."), *GetNameSafe(BrawlArena.Get()));
	}
	BrawlArena = Arena;
	OnBrawlArenaChanged.Broadcast(Arena);
}

void ULOARaidSubsystem::UnregisterBrawlArena(ALOABrawlArena* Arena)
{
	if (BrawlArena.Get() == Arena)
	{
		BrawlArena.Reset();
		OnBrawlArenaChanged.Broadcast(nullptr);
	}
}

void ULOARaidSubsystem::RegisterTerrainZone(ALOATerrainZone* Zone)
{
	if (!Zone || TerrainZones.Contains(Zone))
	{
		return;
	}

	TerrainZones.Add(Zone);

	// 무너졌다는 알림은 서버에서만 울린다 (구역은 누가 듣는지 모른다)
	Zone->OnCollapsed.AddUObject(this, &ThisClass::HandleTerrainZoneCollapsed);

	LOA_LOG(Zone, Log, TEXT("[Terrain] %s (%s) registered."), *GetNameSafe(Zone), *UEnum::GetValueAsString(Zone->GetSide()));
}

void ULOARaidSubsystem::UnregisterTerrainZone(ALOATerrainZone* Zone)
{
	if (!Zone)
	{
		return;
	}

	Zone->OnCollapsed.RemoveAll(this);
	TerrainZones.Remove(Zone);
}

bool ULOARaidSubsystem::RequestTerrainCollapse()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	// 아직 멀쩡한 구역 중 무작위 하나 (예: 왼·오 둘 다 멀쩡 → 둘 중 하나, 하나 남음 → 그것)
	TArray<ALOATerrainZone*> Candidates;
	for (const TWeakObjectPtr<ALOATerrainZone>& Zone : TerrainZones)
	{
		if (Zone.IsValid() && Zone->GetZoneState() == ELOATerrainZoneState::Intact)
		{
			Candidates.Add(Zone.Get());
		}
	}

	if (Candidates.IsEmpty())
	{
		LOA_LOG(World, Log, TEXT("[Terrain] Collapse requested, but no intact zone is left (%d zone(s))."), TerrainZones.Num());
		return false;
	}

	ALOATerrainZone* Picked = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	LOA_LOG(Picked, Log, TEXT("[Terrain] Collapse requested: picked %s (%d intact zone(s))."),
		*UEnum::GetValueAsString(Picked->GetSide()), Candidates.Num());
	return Picked->StartCollapse();
}

bool ULOARaidSubsystem::RequestTerrainCollapse(ELOATerrainSide Side)
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	for (const TWeakObjectPtr<ALOATerrainZone>& Zone : TerrainZones)
	{
		if (Zone.IsValid() && Zone->GetSide() == Side)
		{
			return Zone->StartCollapse();	// 이미 예고·무너짐이면 false
		}
	}

	LOA_LOG(World, Warning, TEXT("[Terrain] No zone on side %s."), *UEnum::GetValueAsString(Side));
	return false;
}

void ULOARaidSubsystem::ResetTerrain()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	for (const TWeakObjectPtr<ALOATerrainZone>& Zone : TerrainZones)
	{
		if (Zone.IsValid())
		{
			Zone->ResetZone();	// 이미 멀쩡하면 아무것도 안 함
		}
	}
}

int32 ULOARaidSubsystem::GetCollapsedTerrainCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ALOATerrainZone>& Zone : TerrainZones)
	{
		if (Zone.IsValid() && Zone->GetZoneState() != ELOATerrainZoneState::Intact)
		{
			++Count;
		}
	}
	return Count;
}

bool ULOARaidSubsystem::FindRecoverLocation(const FVector& Location, FVector& OutLocation) const
{
	// 무너진 구역만 본다. 예고 중인 구역은 아직 바닥이 있다
	for (const TWeakObjectPtr<ALOATerrainZone>& Zone : TerrainZones)
	{
		if (Zone.IsValid() && Zone->GetZoneState() == ELOATerrainZoneState::Collapsed && Zone->IsInside(Location))
		{
			OutLocation = Zone->GetRecoverLocation(Location);
			return true;
		}
	}
	return false;
}

void ULOARaidSubsystem::HandleTerrainZoneCollapsed(ALOATerrainZone* Zone)
{
	UWorld* World = GetWorld();
	if (!Zone || !World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	// ① 시체: 이동이 꺼져 있어 허공에 남는다 → 가운데 원으로 (살아 있는 사람은 그대로 떨어져 낙사 → FellOutOfWorld가 옮김)
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			ALOACharacter* Character = PlayerState ? Cast<ALOACharacter>(PlayerState->GetPawn()) : nullptr;
			if (Character && Character->IsDead() && Zone->IsInside(Character->GetActorLocation()))
			{
				const FVector Recover = Zone->GetRecoverLocation(Character->GetActorLocation());
				LOA_LOG(Character, Log, TEXT("[Terrain] Dead body moved out of %s to %s."), *GetNameSafe(Zone), *Recover.ToCompactString());
				Character->RelocateBody(Recover);
			}
		}
	}

	// ② 보스: 떨어지면 엔진이 지운다 → 가운데 원으로 (StateTree가 지형 파괴 전에 중앙으로 보내지 않으므로 예고 중에 들어와 있을 수 있음)
	if (ALOABossCharacter* Boss = CurrentBoss.Get())
	{
		if (Zone->IsInside(Boss->GetActorLocation()))
		{
			const FVector Recover = Zone->GetRecoverLocation(Boss->GetActorLocation());
			LOA_LOG(Boss, Log, TEXT("[Terrain] Boss moved out of %s to %s."), *GetNameSafe(Zone), *Recover.ToCompactString());
			if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
			Boss->SetActorLocation(Recover, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}
