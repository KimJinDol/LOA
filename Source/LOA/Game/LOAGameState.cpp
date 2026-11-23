// LOAGameState.cpp

#include "Game/LOAGameState.h"
#include "Game/LOARaidSubsystem.h"
#include "Game/LOARaidRecordSubsystem.h"
#include "Character/Player/LOACharacter.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Actors/Skill/LOASkillActor.h"
#include "Actors/LOARepairExit.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOANetTime.h"
#include "Core/LOALog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGameState)

ALOAGameState::ALOAGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ALOAGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALOAGameState, ReviveLives);
	DOREPLIFETIME(ALOAGameState, MaxReviveLives);
	DOREPLIFETIME(ALOAGameState, NextReviveLifeServerTime);
	DOREPLIFETIME(ALOAGameState, RaidPhase);
	DOREPLIFETIME(ALOAGameState, WipeRespawnServerTime);
	DOREPLIFETIME(ALOAGameState, TransferStartServerTime);
	DOREPLIFETIME(ALOAGameState, ClearResult);
}

void ALOAGameState::BeginPlay()
{
	Super::BeginPlay();

	// 보스 쓰러짐은 모든 컴퓨터에서 울린다. 클리어 판단은 서버만
	if (HasAuthority())
	{
		if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
		{
			Raid->OnBossDefeated.AddDynamic(this, &ThisClass::HandleBossDefeated);
		}
	}
}

void ALOAGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->OnBossDefeated.RemoveDynamic(this, &ThisClass::HandleBossDefeated);
	}

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// 인원 변화
// =============================================================================

void ALOAGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	// 클라에서도 불린다 (PlayerState 복제 도착). 계산은 서버만.
	if (HasAuthority())
	{
		RefreshMaxReviveLives();
	}

	// 파티 틀 등 (모든 컴퓨터)
	OnPartyChanged.Broadcast();
}

void ALOAGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	if (HasAuthority())
	{
		RefreshMaxReviveLives();

		// 마지막 생존자가 나갔으면 전멸
		CheckWipe();

		// 정비소에서 출구 밖에 있던 사람이 나갔으면 남은 사람끼리 출발할 수 있다
		CheckRepairExit();
	}

	// 파티 틀 등 (모든 컴퓨터)
	OnPartyChanged.Broadcast();
}

// =============================================================================
// 전투 시작 / 목숨 쓰기
// =============================================================================

void ALOAGameState::StartBattle()
{
	if (!HasAuthority())
	{
		return;
	}

	// 전투 전일 때만. (전투 중에 다시 들어와도 무시 / 전멸 처리 중에도 무시)
	if (RaidPhase != ELOARaidPhase::Idle)
	{
		return;
	}

	SetRaidPhase(ELOARaidPhase::Battle);

	// 걸린 시간·레이드 기록은 이번 도전부터 (전멸 뒤 다시 도전하면 마지막 도전만, 헤더 [클리어])
	BattleStartServerTime = LOANetTime::GetServerTimeNow(GetWorld());
	if (ULOARaidRecordSubsystem* RaidRecord = ULOARaidRecordSubsystem::Get(this))
	{
		RaidRecord->BeginRecording();
	}

	ReviveLives = 0;
	MaxReviveLives = PlayerArray.Num();
	StartLifeTimer();

	LOA_LOG(this, Log, TEXT("[Revive] Battle started. Lives 0/%d, next life in %.0fs"), MaxReviveLives, ReviveLifeInterval);

	BroadcastReviveLivesChanged();
}

bool ALOAGameState::TryConsumeReviveLife()
{
	if (!HasAuthority() || ReviveLives <= 0)
	{
		return false;
	}

	--ReviveLives;

	// 가득 차서 멈춰 있었으면 지금부터 다시 잰다. (A안: 가득 찬 동안 흐른 시간은 쌓이지 않는다)
	// 이미 돌고 있었으면 그대로 둔다. (예: 1/3에서 60초 중 40초 지남 → 0/3이 돼도 20초 뒤 1/3)
	if (NextReviveLifeServerTime <= 0.0)
	{
		StartLifeTimer();
	}

	LOA_LOG(this, Log, TEXT("[Revive] Life used. Lives %d/%d"), ReviveLives, MaxReviveLives);

	BroadcastReviveLivesChanged();
	return true;
}

float ALOAGameState::GetSecondsUntilNextReviveLife() const
{
	if (NextReviveLifeServerTime <= 0.0)
	{
		return -1.f;
	}

	// 각 컴퓨터가 자기 서버 시각으로 계산한다. (남은 시간을 매 초 복제하지 않음)
	const double Remaining = NextReviveLifeServerTime - LOANetTime::GetServerTimeNow(GetWorld());
	return static_cast<float>(FMath::Max(Remaining, 0.0));
}

// =============================================================================
// 시계 [서버]
// =============================================================================

void ALOAGameState::StartLifeTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 복제용 "다음 시각"과 실제 타이머를 같은 시계(게임 시간)로 맞춘다.
	NextReviveLifeServerTime = LOANetTime::GetServerTimeNow(World) + ReviveLifeInterval;
	World->GetTimerManager().SetTimer(ReviveLifeTimerHandle, this, &ThisClass::OnLifeTimerFired, ReviveLifeInterval, false);
}

void ALOAGameState::StopLifeTimer()
{
	NextReviveLifeServerTime = 0.0;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReviveLifeTimerHandle);
	}
}

void ALOAGameState::OnLifeTimerFired()
{
	ReviveLives = FMath::Min(ReviveLives + 1, MaxReviveLives);

	// 아직 빈자리가 있으면 다음 60초, 가득 찼으면 멈춤
	if (ReviveLives < MaxReviveLives)
	{
		StartLifeTimer();
	}
	else
	{
		StopLifeTimer();
	}

	LOA_LOG(this, Log, TEXT("[Revive] Life filled. Lives %d/%d%s"), ReviveLives, MaxReviveLives,
		NextReviveLifeServerTime > 0.0 ? TEXT("") : TEXT(" (full, timer stopped)"));

	BroadcastReviveLivesChanged();
}

void ALOAGameState::RefreshMaxReviveLives()
{
	// 전투 중이 아니면 계산하지 않는다. (StartBattle에서 정함)
	if (RaidPhase != ELOARaidPhase::Battle)
	{
		return;
	}

	const int32 NewMax = PlayerArray.Num();
	if (NewMax == MaxReviveLives)
	{
		return;
	}

	MaxReviveLives = NewMax;

	// 사람이 나가서 최대가 줄었으면 넘친 목숨은 버린다. (예: 3/3에서 1명 나감 → 2/2)
	ReviveLives = FMath::Min(ReviveLives, MaxReviveLives);

	// 가득 = 시계 멈춤 / 빈자리 생김 = 멈춰 있었으면 지금부터 다시
	if (ReviveLives >= MaxReviveLives)
	{
		StopLifeTimer();
	}
	else if (NextReviveLifeServerTime <= 0.0)
	{
		StartLifeTimer();
	}

	LOA_LOG(this, Log, TEXT("[Revive] Party size changed. Lives %d/%d"), ReviveLives, MaxReviveLives);

	BroadcastReviveLivesChanged();
}

// =============================================================================
// 알림
// =============================================================================

void ALOAGameState::OnRep_ReviveLives()
{
	// 세 값이 한 번에 오면 세 번 불린다. 받는 쪽이 Getter로 다시 읽으므로 상관없다.
	BroadcastReviveLivesChanged();
}

void ALOAGameState::BroadcastReviveLivesChanged()
{
	OnReviveLivesChanged.Broadcast();
}

// =============================================================================
// 전멸 [서버]
// =============================================================================

void ALOAGameState::NotifyPlayerDied(ALOACharacter* DeadCharacter)
{
	if (!HasAuthority())
	{
		return;
	}

	// 다음 프레임에 확인한다. 지금은 막타 데미지를 적용하는 도중(장판의 판정 → GE → 사망 알림 안쪽)이라,
	// 여기서 바로 전멸 처리를 하면 그 장판을 지우고 보스 패턴을 취소하는 일이 판정 함수 한가운데서 일어난다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CheckWipe);
	}
}

void ALOAGameState::CheckWipe()
{
	if (!HasAuthority() || RaidPhase != ELOARaidPhase::Battle)
	{
		return;
	}

	// 보스가 이미 쓰러졌으면 전멸이 아니다
	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this);
	if (Raid && Raid->IsCurrentBossDefeated())
	{
		return;
	}

	// 살아 있는 플레이어가 한 명이라도 있으면 아님 (몸이 없는 사람은 살아 있지 않은 것으로 본다)
	for (const APlayerState* PlayerState : PlayerArray)
	{
		const ALOACharacter* Character = PlayerState ? PlayerState->GetPawn<ALOACharacter>() : nullptr;
		if (Character && !Character->IsDead())
		{
			return;
		}
	}

	BeginWipe();
}

void ALOAGameState::BeginWipe()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LOA_LOG(this, Log, TEXT("[Raid] Wiped. Respawn in %.1fs"), WipeRespawnDelay);

	// ① 단계 = 전멸 + 부활할 서버 시각 (복제 → 각 컴퓨터가 남은 초 계산)
	WipeRespawnServerTime = LOANetTime::GetServerTimeNow(World) + WipeRespawnDelay;
	SetRaidPhase(ELOARaidPhase::Wiped);

	// ② 공용 목숨 초기화 (이 3초 동안 부활 버튼은 목숨 0이라 서버가 거절한다)
	ResetReviveLives();

	// ②-1 레이드 기록 멈춤 (다음 전투 시작 때 새로. 2026-11-15)
	if (ULOARaidRecordSubsystem* RaidRecord = ULOARaidRecordSubsystem::Get(this))
	{
		RaidRecord->StopRecording();
	}

	// ③ 보스 리셋 + 무너진 지형 되돌리기 (2026-11-14. 다음 도전은 멀쩡한 경기장에서)
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		if (ALOABossCharacter* Boss = Raid->GetCurrentBoss())
		{
			Boss->ResetForWipe();
		}
		Raid->ResetTerrain();
	}

	// ④ 남은 장판·투사체
	DestroyAllSkillActors();

	World->GetTimerManager().SetTimer(WipeRespawnTimerHandle, this, &ThisClass::FinishWipe, FMath::Max(WipeRespawnDelay, 0.01f), false);
}

void ALOAGameState::FinishWipe()
{
	WipeRespawnServerTime = 0.0;

	// 정비소로 (부활은 순간이동할 때 같이)
	BeginTransfer(ELOATransferDestination::RepairArea);
}

// =============================================================================
// 클리어 [서버] (헤더 [클리어])
// =============================================================================

void ALOAGameState::HandleBossDefeated(ALOABossCharacter* Boss)
{
	if (!HasAuthority())
	{
		return;
	}

	// 다음 프레임에. 보스는 보통 스킬·장판 판정 함수 안쪽에서 죽는다 → 그 장판을 지우는 일을 판정 한가운데서 하지 않게 (전멸과 같은 이유)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::BeginClear);
	}
}

void ALOAGameState::BeginClear()
{
	if (!HasAuthority())
	{
		return;
	}

	// 전투 중에 쓰러졌을 때만 (예: 전투 전에 치트로 죽이면 클리어 아님)
	if (RaidPhase != ELOARaidPhase::Battle)
	{
		LOA_LOG(this, Warning, TEXT("[Raid] Boss defeated outside battle (phase %s). No clear."), *UEnum::GetValueAsString(RaidPhase));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ① 결과: 몫·MVP(기록 담당) + 걸린 시간 + 보스 이름·얼굴
	ULOARaidRecordSubsystem* RaidRecord = ULOARaidRecordSubsystem::Get(this);
	FLOARaidClearResult NewResult = RaidRecord ? RaidRecord->BuildClearResult(PlayerArray) : FLOARaidClearResult();
	NewResult.ClearSeconds = static_cast<float>(FMath::Max(LOANetTime::GetServerTimeNow(World) - BattleStartServerTime, 0.0));
	if (const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		if (const ALOABossCharacter* Boss = Raid->GetCurrentBoss())
		{
			NewResult.BossName = Boss->GetBossDisplayName();
			NewResult.BossPortrait = Boss->GetClearPortrait();
		}
	}
	ClearResult = MoveTemp(NewResult);

	// ② 기록 멈춤, 목숨 시계 멈춤, 남은 장판·투사체 지우기
	if (RaidRecord)
	{
		RaidRecord->StopRecording();
	}
	StopLifeTimer();
	BroadcastReviveLivesChanged();
	DestroyAllSkillActors();

	LOA_LOG(this, Log, TEXT("[Raid] Cleared in %.1fs. MVP index %d."), ClearResult.ClearSeconds, ClearResult.MvpIndex);

	// ③ 단계 = 클리어 (결과를 넣은 뒤. 리슨 호스트도 단계 알림 때 결과가 있다)
	SetRaidPhase(ELOARaidPhase::Cleared);
}

// =============================================================================
// 화면 전환 [서버]
// =============================================================================

void ALOAGameState::BeginTransfer(ELOATransferDestination Destination)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TransferDestination = Destination;

	// 시작 서버 시각을 단계와 같이 복제 → 각 컴퓨터가 지난 시간으로 어둡기를 계산 (서버 시각 원칙)
	TransferStartServerTime = LOANetTime::GetServerTimeNow(World);
	SetRaidPhase(ELOARaidPhase::Transferring);

	LOA_LOG(this, Log, TEXT("[Raid] Transfer to %s started."),
		Destination == ELOATransferDestination::RepairArea ? TEXT("repair area") : TEXT("battle area"));

	// 0초 타이머는 예약되지 않으므로 최소 0.01초
	FTimerManager& Timers = World->GetTimerManager();
	Timers.SetTimer(TransferTeleportTimerHandle, this, &ThisClass::TeleportForTransfer,
		FMath::Max(TransferFadeOutTime, 0.01f), false);
	Timers.SetTimer(TransferFinishTimerHandle, this, &ThisClass::FinishTransfer,
		FMath::Max(TransferFadeOutTime + TransferBlackHoldTime, 0.02f), false);
}

void ALOAGameState::TeleportForTransfer()
{
	// 몸이 있는 플레이어만 센다 (입구에서 벌릴 칸 수)
	TArray<ALOACharacter*> Characters;
	for (APlayerState* PlayerState : PlayerArray)
	{
		if (ALOACharacter* Character = PlayerState ? PlayerState->GetPawn<ALOACharacter>() : nullptr)
		{
			Characters.Add(Character);
		}
	}

	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		ALOACharacter* Character = Characters[Index];

		FVector Location;
		const bool bFound = (TransferDestination == ELOATransferDestination::RepairArea)
			? FindRespawnLocation(Character->GetController(), Location)
			: FindEntranceLocation(Character->GetController(), Index, Characters.Num(), Location);

		if (!bFound)
		{
			Location = Character->GetActorLocation();
			LOA_LOG(Character, Warning, TEXT("[Raid] No %s location marker or PlayerStart. Staying in place."),
				TransferDestination == ELOATransferDestination::RepairArea ? TEXT("Location.Raid.Respawn") : TEXT("Location.Raid.Entrance"));
		}

		// 죽어 있으면 살리고(무적 없음) 옮긴다. 주인 클라도 같은 자리로 바로 (ClientSnapToLocation)
		Character->RespawnAt(Location);
	}
}

void ALOAGameState::FinishTransfer()
{
	if (TransferDestination == ELOATransferDestination::RepairArea)
	{
		SetRaidPhase(ELOARaidPhase::Repair);
		LOA_LOG(this, Log, TEXT("[Raid] Arrived at repair area."));

		// 도착 자리가 이미 출구 상자 안일 수도 있다
		CheckRepairExit();
		return;
	}

	// 전투 구역 도착 → 보스 AI 다시 켬 (대기부터) → 전투 전. 다가가면 전투 시작
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		if (ALOABossCharacter* Boss = Raid->GetCurrentBoss())
		{
			Boss->ResumeAfterWipe();
		}
	}

	SetRaidPhase(ELOARaidPhase::Idle);
	LOA_LOG(this, Log, TEXT("[Raid] Arrived at battle area. Back to idle."));
}

void ALOAGameState::CheckRepairExit()
{
	if (!HasAuthority() || RaidPhase != ELOARaidPhase::Repair)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 정비소 출구는 레벨에 하나. 여러 개면 아무 상자에나 전원이 모이면 출발
	for (TActorIterator<ALOARepairExit> It(World); It; ++It)
	{
		int32 NumLiving = 0;
		int32 NumInside = 0;
		for (const APlayerState* PlayerState : PlayerArray)
		{
			const ALOACharacter* Character = PlayerState ? PlayerState->GetPawn<ALOACharacter>() : nullptr;
			if (!Character || Character->IsDead())
			{
				continue;
			}

			++NumLiving;
			if (It->IsCharacterInside(Character))
			{
				++NumInside;
			}
		}

		LOA_LOG(this, Log, TEXT("[Raid] Repair exit: %d / %d inside."), NumInside, NumLiving);

		if (NumLiving > 0 && NumInside == NumLiving)
		{
			BeginTransfer(ELOATransferDestination::BattleArea);
			return;
		}
	}
}

bool ALOAGameState::FindEntranceLocation(AController* ForController, int32 Index, int32 Count, FVector& OutLocation) const
{
	FTransform Entrance;
	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this);
	if (Raid && Raid->FindLocationTransform(LOAGameplayTags::Location_Raid_Entrance, Entrance))
	{
		// 가운데를 기준으로 오른쪽 방향으로 벌린다. 예) 3명 → -150 / 0 / +150
		const float Offset = (static_cast<float>(Index) - (static_cast<float>(Count) - 1.f) * 0.5f) * EntranceSpacing;
		const FVector Right = Entrance.GetRotation().GetRightVector().GetSafeNormal2D();
		OutLocation = Entrance.GetLocation() + Right * Offset;
		return true;
	}

	// 없으면 그 플레이어의 PlayerStart
	AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	if (GameMode && ForController)
	{
		if (const AActor* Start = GameMode->FindPlayerStart(ForController))
		{
			OutLocation = Start->GetActorLocation();
			return true;
		}
	}

	return false;
}

float ALOAGameState::GetTransferFadeAlpha() const
{
	if (TransferStartServerTime <= 0.0)
	{
		return 0.f;
	}

	// 예) 시작 100.0 / 지금 100.25 → 어두워짐 0.5초의 절반 → 0.5
	const double Elapsed = LOANetTime::GetServerTimeNow(GetWorld()) - TransferStartServerTime;
	if (Elapsed < 0.0)
	{
		return 0.f;
	}

	if (Elapsed < TransferFadeOutTime)
	{
		return static_cast<float>(Elapsed / TransferFadeOutTime);
	}

	const double FadeInStart = TransferFadeOutTime + TransferBlackHoldTime;
	if (Elapsed < FadeInStart)
	{
		return 1.f;
	}

	if (Elapsed < FadeInStart + TransferFadeInTime)
	{
		return 1.f - static_cast<float>((Elapsed - FadeInStart) / TransferFadeInTime);
	}

	return 0.f;
}

void ALOAGameState::ResetReviveLives()
{
	StopLifeTimer();
	ReviveLives = 0;
	MaxReviveLives = 0;
	BroadcastReviveLivesChanged();
}

void ALOAGameState::DestroyAllSkillActors()
{
	// 서버가 지우면 모든 컴퓨터에서 사라진다. 장판은 사라질 때 자기 예고도 지운다 (ALOAAreaActor::EndPlay → HideToAll)
	int32 NumDestroyed = 0;
	for (TActorIterator<ALOASkillActor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			It->Destroy();
			++NumDestroyed;
		}
	}

	if (NumDestroyed > 0)
	{
		LOA_LOG(this, Log, TEXT("[Raid] Destroyed %d skill actors."), NumDestroyed);
	}
}

bool ALOAGameState::FindRespawnLocation(AController* ForController, FVector& OutLocation) const
{
	// 위치 표시 (정비소 자리)
	if (const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		if (Raid->FindLocation(LOAGameplayTags::Location_Raid_Respawn, OutLocation))
		{
			return true;
		}
	}

	// 없으면 그 플레이어의 PlayerStart
	AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	if (GameMode && ForController)
	{
		if (const AActor* Start = GameMode->FindPlayerStart(ForController))
		{
			OutLocation = Start->GetActorLocation();
			return true;
		}
	}

	return false;
}

float ALOAGameState::GetSecondsUntilWipeRespawn() const
{
	if (RaidPhase != ELOARaidPhase::Wiped || WipeRespawnServerTime <= 0.0)
	{
		return -1.f;
	}

	const double Remaining = WipeRespawnServerTime - LOANetTime::GetServerTimeNow(GetWorld());
	return static_cast<float>(FMath::Max(Remaining, 0.0));
}

// =============================================================================
// 단계 알림
// =============================================================================

void ALOAGameState::SetRaidPhase(ELOARaidPhase NewPhase)
{
	if (RaidPhase == NewPhase)
	{
		return;
	}

	RaidPhase = NewPhase;

	// 서버(리슨 호스트 포함)는 OnRep이 안 불리므로 직접
	HandleRaidPhaseChanged();
}

void ALOAGameState::OnRep_RaidPhase()
{
	HandleRaidPhaseChanged();
}

void ALOAGameState::HandleRaidPhaseChanged()
{
	OnRaidPhaseChanged.Broadcast(RaidPhase);

	// 임시 화면 표시 (사용자 결정: 나중에 전멸 UI). 데디케이티드 서버는 화면이 없어 아무 일 없음
	if (!GEngine)
	{
		return;
	}

	if (RaidPhase == ELOARaidPhase::Wiped)
	{
		const float Remaining = GetSecondsUntilWipeRespawn();
		GEngine->AddOnScreenDebugMessage(-1, FMath::Max(Remaining, 1.f), FColor::Red,
			FString::Printf(TEXT("WIPED - to repair area in %.0fs"), FMath::Max(Remaining, 0.f)));
	}
	else if (RaidPhase == ELOARaidPhase::Repair)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("REPAIR AREA - all players go to the exit"));
	}
}
