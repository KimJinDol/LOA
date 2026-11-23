// LOABrawlArena.cpp

#include "Actors/LOABrawlArena.h"
#include "Game/LOARaidSubsystem.h"
#include "Game/LOAGameState.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Character/Player/LOACharacter.h"
#include "Library/LOACombatLibrary.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Effects/LOAGE_HealthChange.h"
#include "Combat/LOACombatNoticeSubsystem.h"
#include "Combat/LOAJustGuardSubsystem.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOANetTime.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOABrawlArena)

ALOABrawlArena::ALOABrawlArena(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 모든 컴퓨터가 매 프레임 슬로우(애니 속도)를 맞춘다. 대난투가 아닐 때는 바로 돌아간다 (Tick 첫 줄)
	PrimaryActorTick.bCanEverTick = true;

	// 상태 하나만 복제. 레벨에 놓인 액터라 모든 컴퓨터에 있고, 거리와 상관없이 항상 받게 한다 (지형 구역과 같음)
	bReplicates = true;
	bAlwaysRelevant = true;

	// 루트 = 통로 가운데 바닥 = 보스 자리
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	EditorArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EditorArrow"));
	if (EditorArrow)
	{
		EditorArrow->SetupAttachment(RootComponent);
		EditorArrow->ArrowSize = 3.f;
	}
#endif

	// 기본 자리: 보스 앞 500, 좌우 150 간격 (대신 정함 11-18. 레벨에서 옮긴다)
	PlayerSpots = { FVector(500.f, -150.f, 0.f), FVector(500.f, 0.f, 0.f), FVector(500.f, 150.f, 0.f) };

	// 단계 목표 (2배 된 피해 기준): 1단계 3,000 → 2단계 6,000 → 3단계 목표 없음 (대신 정함 11-18, 사용자 OK)
	StageGoals = { 3000.f, 6000.f };

	// 협동 저가 등급별 추가 시간 (대신 정함 11-18, apply128)
	CoopJustGuardBonusSeconds.Add(ELOAJustGuardGrade::Perfect, 5.f);
	CoopJustGuardBonusSeconds.Add(ELOAJustGuardGrade::Great, 3.f);
	CoopJustGuardBonusSeconds.Add(ELOAJustGuardGrade::Bad, 0.f);
}

void ALOABrawlArena::BeginPlay()
{
	Super::BeginPlay();

	// [모든 컴퓨터] 상황판에 등록 → 보스 그로기 GA(서버)·내 컨트롤러(각자)가 여기서 찾는다
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->RegisterBrawlArena(this);
	}
}

void ALOABrawlArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
	}
	UnsubscribeBossDamage();

	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->UnregisterBrawlArena(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ALOABrawlArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALOABrawlArena, RepState);
	DOREPLIFETIME(ALOABrawlArena, Progress);
	DOREPLIFETIME(ALOABrawlArena, CoopJustGuardResult);
}

// =============================================================================
// 서버
// =============================================================================

bool ALOABrawlArena::StartBrawl(ALOABossCharacter* Boss)
{
	if (!HasAuthority() || !Boss || RepState.Phase != ELOABrawlPhase::None)
	{
		return false;
	}

	BrawlBoss = Boss;
	EntryHealth = 0.f;
	LastHitter.Reset();
	Progress = FLOABrawlProgress();

	// ① 원래 자리 기억 (나올 때 여기로)
	BossReturnTransform = Boss->GetActorTransform();
	PlayerReturnLocations.Reset();
	for (ALOACharacter* Character : GetPlayerCharacters())
	{
		PlayerReturnLocations.Add(Character, Character->GetActorLocation());
	}

	// ② 입력 막기 + 남은 장판·투사체 지우기 (사용자 결정 11-18: 지움)
	BlockPlayerInput();
	if (ALOAGameState* LOAGameState = GetWorld()->GetGameState<ALOAGameState>())
	{
		LOAGameState->DestroyAllSkillActors();
	}

	// ③ 들어가는 중 → 슬로우 + 어두워짐 뒤 순간이동
	SetPhase(ELOABrawlPhase::Entering);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::TeleportIn, GetSecondsUntilTeleport(), false);

	LOA_LOG(this, Log, TEXT("[Brawl] Start. %d players, boss %s"), PlayerReturnLocations.Num(), *GetNameSafe(Boss));
	return true;
}

void ALOABrawlArena::TeleportIn()
{
	// 보스 = 경기장 가운데 (바닥 + 캡슐 반 높이), 앞 = 경기장 앞
	if (ALOABossCharacter* Boss = BrawlBoss.Get())
	{
		if (AAIController* AIController = Cast<AAIController>(Boss->GetController()))
		{
			AIController->StopMovement();
		}
		Boss->GetCharacterMovement()->StopMovementImmediately();

		const float HalfHeight = Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Boss->SetActorLocationAndRotation(GetActorLocation() + FVector(0.f, 0.f, HalfHeight), GetActorRotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	// 플레이어 = Player Spots (죽은 사람도 시체 그대로. 사용자 결정 11-18)
	const TArray<ALOACharacter*> Characters = GetPlayerCharacters();
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		Characters[Index]->RelocateBody(GetPlayerSpotLocation(Index, Characters[Index]));
	}

	// 검은 화면 뒤 진행
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::FinishEntering, BlackSeconds, false);
}

void ALOABrawlArena::FinishEntering()
{
	// 진행 처음부터 (헤더 [진행]): 1단계, 남은 시간 Brawl Seconds
	Progress = FLOABrawlProgress();
	Progress.Stage = 1;
	Progress.TimerEndServerTime = LOANetTime::GetServerTimeNow(GetWorld()) + BrawlSeconds;

	SetPhase(ELOABrawlPhase::Active);
	UnblockPlayerInput();

	// 보스: 남은 체력 적어 두기 / 약점 공격 + 대난투 태그 (×2, 체력 대신 대난투 입구) / 맞을 때마다 세기
	ALOABossCharacter* Boss = BrawlBoss.Get();
	const UAbilitySystemComponent* BossASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Boss);
	EntryHealth = BossASC ? BossASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute()) : 0.f;
	if (IsLastStage())
	{
		// 단계 목표가 하나도 없으면 1단계가 곧 마지막 단계 → 목표 = 보스 체력 전부
		Progress.LastStageGoal = EntryHealth;
	}
	ApplyActiveBossTags();
	SubscribeBossDamage(Boss);

	// 남은 시간 0 → 끝 (협동 저가 패턴 동안은 멈춤: AdvanceStage / FinishCoopJustGuard)
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::EndBrawl, BrawlSeconds, false);

	LOA_LOG(this, Log, TEXT("[Brawl] Active. %.1fs, %d stages, boss health %.0f"), BrawlSeconds, GetStageCount(), EntryHealth);
}

void ALOABrawlArena::EndBrawl()
{
	if (!HasAuthority() || RepState.Phase != ELOABrawlPhase::Active)
	{
		return;
	}

	// 남은 시간 고정 (UI는 나오는 중에 0을 보임). 진행 태그를 먼저 뗀다 → 나오는 중에 남은 도트는 1배·체력으로, 약점 공격 없음
	Progress.bTimerPaused = false;
	RemoveActiveBossTags();
	UnsubscribeBossDamage();
	BlockPlayerInput();
	SetPhase(ELOABrawlPhase::Exiting);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::TeleportBack, GetSecondsUntilTeleport(), false);

	LOA_LOG(this, Log, TEXT("[Brawl] End. Stage %d, total damage %.0f / boss health %.0f. Going back."),
		Progress.Stage, Progress.TotalDamage, EntryHealth);
}

void ALOABrawlArena::TeleportBack()
{
	if (ALOABossCharacter* Boss = BrawlBoss.Get())
	{
		Boss->GetCharacterMovement()->StopMovementImmediately();
		Boss->SetActorLocationAndRotation(BossReturnTransform.GetLocation(), BossReturnTransform.GetRotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	// 들어갈 때 기억한 자리로. 들어갈 때 없던 사람(기억 없음)은 그대로 둔다
	for (ALOACharacter* Character : GetPlayerCharacters())
	{
		if (const FVector* ReturnLocation = PlayerReturnLocations.Find(Character))
		{
			Character->RelocateBody(*ReturnLocation);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::FinishExiting, BlackSeconds, false);
}

void ALOABrawlArena::FinishExiting()
{
	ALOABossCharacter* Boss = BrawlBoss.Get();

	SetPhase(ELOABrawlPhase::None);
	UnblockPlayerInput();
	UnsubscribeBossDamage();
	PlayerReturnLocations.Reset();
	BrawlBoss.Reset();

	LOA_LOG(this, Log, TEXT("[Brawl] Back in the original arena."));

	// 누가 듣는지 모른다 (보스 그로기 GA가 끝나고 무력화를 다시 채운다)
	OnBrawlEnded.Broadcast(this);

	// 누적 피해를 체력에 (사용자 11-18: 사망 연출은 대난투를 벗어난 뒤). 그로기가 끝난 뒤라 사망 GA가 정상 흐름으로 켜진다
	ApplyTotalDamageToBoss(Boss);
}

// =============================================================================
// 서버: 진행 (헤더 [진행])
// =============================================================================

void ALOABrawlArena::SubscribeBossDamage(ALOABossCharacter* Boss)
{
	UnsubscribeBossDamage();

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Boss);
	ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(this);
	if (!ASC || !CombatNotice)
	{
		LOA_LOG(this, Warning, TEXT("[Brawl] Cannot watch boss damage (ASC or combat notice missing). Progress will not count."));
		return;
	}

	// 받은 사람별 구독의 열쇠 = 맞은 쪽 ASC의 주인 (전투 알림판 헤더 [받은 사람별 구독])
	BossDamageKey = ASC->GetOwnerActor();
	BossDamagedHandle = CombatNotice->OnDamageReceivedBy(BossDamageKey.Get()).AddUObject(this, &ThisClass::HandleBossDamaged);
}

void ALOABrawlArena::UnsubscribeBossDamage()
{
	if (BossDamagedHandle.IsValid())
	{
		ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(this);
		if (CombatNotice && BossDamageKey.IsValid())
		{
			CombatNotice->OnDamageReceivedBy(BossDamageKey.Get()).Remove(BossDamagedHandle);
		}
	}
	BossDamagedHandle.Reset();
	BossDamageKey.Reset();
}

void ALOABrawlArena::HandleBossDamaged(const FLOADamageAppliedInfo& Info)
{
	// 대난투 입구로 들어온 피해만 센다 (진행 중 = 보스에 State.Brawl.Active)
	//   누적: 협동 저가 패턴 중에도 / 단계 게이지: 남은 시간이 흐를 때만 (apply127 사용자: 협동 저가 결과가 나온 뒤부터)
	//   센 양 = 알림의 깎인 양 = 2배 된 피해 (레이드 기록과 같은 값). 체력은 아직 그대로
	if (RepState.Phase != ELOABrawlPhase::Active || !Info.bBrawlDamage || Info.RemovedAmount <= 0.f)
	{
		return;
	}

	LastHitter = Info.Instigator;
	Progress.TotalDamage += Info.RemovedAmount;

	//   마지막 단계도 게이지를 센다 (목표 = 보스 남은 체력, apply127). 다음 단계가 없으므로 넘기지 않는다 (다 차면 아래에서 끝)
	if (!Progress.bTimerPaused)
	{
		Progress.StageDamage += Info.RemovedAmount;
		if (!IsLastStage() && Progress.StageDamage >= GetStageGoal(Progress.Stage))
		{
			AdvanceStage();
		}
	}

	// 누적이 보스 남은 체력만큼 됨 → 바로 끝 (사용자 11-18). 돌아온 뒤 체력이 0이 되어 사망
	//   예) 남은 체력 2,000, 누적 1,800에서 1,500 → 3,300 ≥ 2,000 → 끝
	if (Progress.TotalDamage >= EntryHealth)
	{
		LOA_LOG(this, Log, TEXT("[Brawl] Total damage %.0f reached the boss health %.0f (last hit: %s). Ending brawl; the boss dies after returning."),
			Progress.TotalDamage, EntryHealth, *GetNameSafe(Info.Instigator));
		EndBrawl();
	}
}

void ALOABrawlArena::AdvanceStage()
{
	// 바로 다음 단계, 게이지는 0부터 (넘친 양은 버림: 대신 정함)
	++Progress.Stage;
	Progress.StageDamage = 0.f;

	// 마지막 단계면 게이지 목표 = 보스 남은 체력 (사용자 A: 이 순간의 누적 기준). 예) 입장 20,000, 누적 9,000 → 11,000
	if (IsLastStage())
	{
		Progress.LastStageGoal = FMath::Max(EntryHealth - Progress.TotalDamage, 0.f);
	}

	// 남은 시간 멈춤 (사용자 11-18: 협동 저가 동안 멈춤). 듣는 쪽이 패턴이 끝나면 FinishCoopJustGuard
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (!Progress.bTimerPaused)
	{
		Progress.PausedRemainingSeconds = TimerManager.GetTimerRemaining(PhaseTimerHandle);
		Progress.bTimerPaused = true;
		TimerManager.PauseTimer(PhaseTimerHandle);
	}

	LOA_LOG(this, Log, TEXT("[Brawl] Stage %d / %d (%.1fs left, paused). Total damage %.0f"),
		Progress.Stage, GetStageCount(), Progress.PausedRemainingSeconds, Progress.TotalDamage);

	OnStageAdvanced.Broadcast(this, Progress.Stage);
}

void ALOABrawlArena::FinishCoopJustGuard(int32 SuccessCount)
{
	if (!HasAuthority() || RepState.Phase != ELOABrawlPhase::Active || !Progress.bTimerPaused)
	{
		return;
	}

	// 성공 인원 → 등급 → 추가 시간. 예) 2명 → Great → +3초
	const ELOAJustGuardGrade Grade = ULOAJustGuardSubsystem::GetCoopJustGuardGrade(SuccessCount);
	const float BonusSeconds = FMath::Max(CoopJustGuardBonusSeconds.FindRef(Grade), 0.f);

	// 결과 복제 → 대난투 UI (번호가 바뀌면 띄운다)
	++CoopJustGuardResult.ResultId;
	CoopJustGuardResult.Grade = Grade;
	CoopJustGuardResult.BonusSeconds = BonusSeconds;

	LOA_LOG(this, Log, TEXT("[Brawl] Co-op just guard: %d success -> %s, +%.1fs"),
		SuccessCount, *UEnum::GetDisplayValueAsText(Grade).ToString(), BonusSeconds);

	ResumeBrawlTimerWithBonus(BonusSeconds);
}

void ALOABrawlArena::ResumeBrawlTimer()
{
	if (!HasAuthority() || RepState.Phase != ELOABrawlPhase::Active || !Progress.bTimerPaused)
	{
		return;
	}
	ResumeBrawlTimerWithBonus(0.f);
}

void ALOABrawlArena::ResumeBrawlTimerWithBonus(float BonusSeconds)
{
	// 멈춘 남은 시간 + 추가 시간으로 타이머를 다시 건다 (멈춘 타이머에 시간을 더하는 함수가 없어서 새로 건다)
	//   예) 12.3초에서 멈춤, +3초 → 15.3초 뒤 EndBrawl
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	const float Remaining = TimerManager.GetTimerRemaining(PhaseTimerHandle) + BonusSeconds;
	TimerManager.SetTimer(PhaseTimerHandle, this, &ThisClass::EndBrawl, FMath::Max(Remaining, KINDA_SMALL_NUMBER), false);

	Progress.bTimerPaused = false;
	Progress.TimerEndServerTime = LOANetTime::GetServerTimeNow(GetWorld()) + Remaining;

	LOA_LOG(this, Log, TEXT("[Brawl] Timer resumed (%.1fs left, +%.1fs)"), Remaining, BonusSeconds);
}

float ALOABrawlArena::GetStageGoal(int32 Stage) const
{
	// 1단계 = StageGoals[0]. 마지막 단계 = 보스 남은 체력 기준 (Progress.LastStageGoal, 복제됨). 범위 밖은 0
	if (Stage == GetStageCount())
	{
		return Progress.LastStageGoal;
	}
	return StageGoals.IsValidIndex(Stage - 1) ? StageGoals[Stage - 1] : 0.f;
}

float ALOABrawlArena::GetStageGaugePercent() const
{
	// 지금 단계 게이지 ÷ 목표 (마지막 단계도 같은 식, 목표만 남은 체력)
	const float Goal = GetStageGoal(Progress.Stage);
	return (Goal > 0.f) ? FMath::Clamp(Progress.StageDamage / Goal, 0.f, 1.f) : 0.f;
}

float ALOABrawlArena::GetRemainingSeconds() const
{
	if (RepState.Phase != ELOABrawlPhase::Active)
	{
		return 0.f;
	}
	if (Progress.bTimerPaused)
	{
		return Progress.PausedRemainingSeconds;
	}
	return FMath::Max(static_cast<float>(Progress.TimerEndServerTime - LOANetTime::GetServerTimeNow(GetWorld())), 0.f);
}

void ALOABrawlArena::BlockPlayerInput()
{
	// 들어가는 중·나오는 중에 새 스킬·이동을 막는다. 태그는 ASC 복제로 주인 컴퓨터에도 간다 (클릭 이동·스킬이 그 태그를 본다)
	FGameplayTagContainer BlockTags;
	BlockTags.AddTag(LOAGameplayTags::State_Block_Move);
	BlockTags.AddTag(LOAGameplayTags::State_Block_Ability);

	for (ALOACharacter* Character : GetPlayerCharacters())
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		if (!ASC || InputBlockHandles.Contains(ASC))
		{
			continue;
		}

		InputBlockHandles.Add(ASC, ULOACombatLibrary::ApplyStateTags(ASC, BlockTags));
	}
}

void ALOABrawlArena::UnblockPlayerInput()
{
	for (const TPair<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle>& Pair : InputBlockHandles)
	{
		if (UAbilitySystemComponent* ASC = Pair.Key.Get())
		{
			ASC->RemoveActiveGameplayEffect(Pair.Value);
		}
	}
	InputBlockHandles.Reset();
}

void ALOABrawlArena::ApplyActiveBossTags()
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(BrawlBoss.Get());
	if (!ASC || ActiveTagsHandle.IsValid())
	{
		return;
	}

	// 약점 공격 (State.Debuff 아래 → 보스가 죽으면 사망 GA가 같이 지움. 아이콘 = DA_StatusInfo 줄) + 대난투 배율
	FGameplayTagContainer BossTags;
	BossTags.AddTag(LOAGameplayTags::State_Debuff_WeakPoint);
	BossTags.AddTag(LOAGameplayTags::State_Brawl_Active);
	ActiveTagsHandle = ULOACombatLibrary::ApplyStateTags(ASC, BossTags);
	BossTagsASC = ASC;
}

void ALOABrawlArena::RemoveActiveBossTags()
{
	if (UAbilitySystemComponent* ASC = BossTagsASC.Get())
	{
		ASC->RemoveActiveGameplayEffect(ActiveTagsHandle);
	}
	ActiveTagsHandle.Invalidate();
}

void ALOABrawlArena::ApplyTotalDamageToBoss(ALOABossCharacter* Boss)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Boss);
	const float HealthLoss = FMath::Min(Progress.TotalDamage, EntryHealth);
	if (!ASC || HealthLoss <= 0.f)
	{
		return;
	}

	// 막타 = 마지막으로 친 사람 (사망 이벤트의 Instigator가 된다)
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	if (AActor* Hitter = LastHitter.Get())
	{
		Context.AddInstigator(Hitter, Hitter);
	}

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ULOAGE_HealthChange::StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_HealthChange, -HealthLoss);

	LOA_LOG(this, Log, TEXT("[Brawl] Applying brawl damage to the boss health: -%.0f (health %.0f, last hit: %s)"),
		HealthLoss, ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute()), *GetNameSafe(LastHitter.Get()));

	// Common 스탯창의 체력 줄: 범위 맞추기 + 처음 0이면 사망 이벤트 (낙사 ULOAGE_Kill과 같은 길)
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	LastHitter.Reset();
}

TArray<ALOACharacter*> ALOABrawlArena::GetPlayerCharacters() const
{
	TArray<ALOACharacter*> Characters;
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return Characters;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ALOACharacter* Character = PlayerState ? PlayerState->GetPawn<ALOACharacter>() : nullptr)
		{
			Characters.Add(Character);
		}
	}
	return Characters;
}

FVector ALOABrawlArena::GetPlayerSpotLocation(int32 Index, const ALOACharacter* Character) const
{
	// 자리 수보다 사람이 많으면 마지막 자리에서 옆(+Y)으로 더 벌린다. 자리가 하나도 없으면 보스 앞 500
	FVector RelativeSpot = FVector(500.f, 0.f, 0.f);
	if (PlayerSpots.Num() > 0)
	{
		const int32 SpotIndex = FMath::Min(Index, PlayerSpots.Num() - 1);
		const int32 ExtraCount = Index - SpotIndex;
		RelativeSpot = PlayerSpots[SpotIndex] + FVector(0.f, ExtraSpotSpacing * ExtraCount, 0.f);
	}

	// 바닥 높이 자리 → 몸 중심 (RelocateBody는 몸 중심 자리를 받는다)
	const float HalfHeight = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	return GetActorTransform().TransformPosition(RelativeSpot) + FVector(0.f, 0.f, HalfHeight);
}

void ALOABrawlArena::SetPhase(ELOABrawlPhase NewPhase)
{
	RepState.Phase = NewPhase;
	RepState.PhaseStartServerTime = LOANetTime::GetServerTimeNow(GetWorld());

	// 리슨 호스트는 OnRep이 안 불린다 → 여기서 같은 일 (연출은 Tick·컨트롤러가 시각으로 계산. UI 바꾸기는 알림으로)
	OnPhaseChanged.Broadcast(this);
}

void ALOABrawlArena::OnRep_RepState()
{
	LOA_LOG(this, Verbose, TEXT("[Brawl] Phase %s"), *UEnum::GetValueAsString(RepState.Phase));
	OnPhaseChanged.Broadcast(this);
}

// =============================================================================
// 모든 컴퓨터: 연출 시각 (헤더 [흐름])
// =============================================================================

double ALOABrawlArena::GetSecondsInPhase() const
{
	if (RepState.PhaseStartServerTime <= 0.0)
	{
		return TNumericLimits<float>::Max();	// 한 번도 시작 안 함 → 연출 없음
	}
	return LOANetTime::GetServerTimeNow(GetWorld()) - RepState.PhaseStartServerTime;
}

float ALOABrawlArena::GetFadeAlpha() const
{
	const double Seconds = GetSecondsInPhase();
	if (Seconds < 0.0)
	{
		return 0.f;
	}

	switch (RepState.Phase)
	{
	case ELOABrawlPhase::Entering:
	case ELOABrawlPhase::Exiting:
		// 예) 슬로우 1.0, 어두워짐 0.5 : 0.8초 → 0 / 1.25초 → 0.5 / 1.6초 → 1 (검은 화면, 순간이동 기다림)
		if (Seconds < SlowSeconds)
		{
			return 0.f;
		}
		return FMath::Clamp(static_cast<float>((Seconds - SlowSeconds) / FadeOutSeconds), 0.f, 1.f);

	case ELOABrawlPhase::Active:
	case ELOABrawlPhase::None:
		// 도착 직후 밝아짐. 예) 0.25초 → 0.5
		return FMath::Clamp(1.f - static_cast<float>(Seconds / FadeInSeconds), 0.f, 1.f);
	}
	return 0.f;
}

float ALOABrawlArena::GetCameraZoomScale() const
{
	if (RepState.Phase != ELOABrawlPhase::Entering && RepState.Phase != ELOABrawlPhase::Exiting)
	{
		return 1.f;
	}

	// 슬로우 동안 1 → Zoom Scale로 가까워지고, 검은 화면까지 그대로. 도착하면(상태가 바뀌면) 원래 거리
	const double Seconds = GetSecondsInPhase();
	const float ZoomAlpha = (SlowSeconds > 0.f) ? FMath::Clamp(static_cast<float>(Seconds / SlowSeconds), 0.f, 1.f) : 1.f;
	return FMath::Lerp(1.f, ZoomScale, ZoomAlpha);
}

void ALOABrawlArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 슬로우 = 들어가는 중·나오는 중의 처음 Slow Seconds + 어두워짐 (검게 덮이면 보이지 않으니 원래 속도로)
	float TargetRate = 1.f;
	if (RepState.Phase == ELOABrawlPhase::Entering || RepState.Phase == ELOABrawlPhase::Exiting)
	{
		const double Seconds = GetSecondsInPhase();
		if (Seconds >= 0.0 && Seconds < GetSecondsUntilTeleport())
		{
			TargetRate = SlowAnimRate;
		}
	}

	ApplyAnimRate(TargetRate);
}

void ALOABrawlArena::ApplyAnimRate(float Rate)
{
	if (FMath::IsNearlyEqual(Rate, AppliedAnimRate))
	{
		return;
	}
	AppliedAnimRate = Rate;

	// 보스 + 플레이어 전원. 클라에서도 PlayerArray·상황판으로 몸을 찾는다
	TArray<ACharacter*> Targets;
	if (const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		if (ALOABossCharacter* Boss = Raid->GetCurrentBoss())
		{
			Targets.Add(Boss);
		}
	}
	if (const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (ACharacter* Character = PlayerState ? PlayerState->GetPawn<ACharacter>() : nullptr)
			{
				Targets.Add(Character);
			}
		}
	}

	for (ACharacter* Target : Targets)
	{
		if (USkeletalMeshComponent* Mesh = Target->GetMesh())
		{
			Mesh->GlobalAnimRateScale = Rate;
		}
	}

	LOA_LOG(this, Log, TEXT("[Brawl] Anim rate %.2f (%d characters)"), Rate, Targets.Num());
}
