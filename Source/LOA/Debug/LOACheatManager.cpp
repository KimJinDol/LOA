// LOACheatManager.cpp

#include "Debug/LOACheatManager.h"
#include "Debug/LOAPlayerCheats.h"
#include "Player/LOAPlayerController.h"
#include "AbilitySystem/Abilities/Monster/LOAMonsterAbility.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Boss.h"
#include "AbilitySystem/Effects/LOAGE_Stagger.h"
#include "AbilitySystemGlobals.h"
#include "AI/LOAMonsterAIController.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Character/Player/LOACharacter.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Data/LOAAbilityDataSet.h"
#include "Data/LOACharacterClassData.h"
#include "Game/LOARaidSubsystem.h"
#include "Actors/LOATerrainZone.h"
#include "Library/LOACombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACheatManager)

namespace LOACheat
{
	/** 배속 범위 (너무 작으면 멈춘 것처럼, 너무 크면 판정이 건너뛴다) */
	constexpr float MinTimeDilation = 0.1f;
	constexpr float MaxTimeDilation = 2.f;

	/** 태그 마지막 부분. Ability.Moss.Downward → "Downward" */
	FString GetLastTagPart(const FGameplayTag& Tag)
	{
		FString Name = Tag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (Name.FindLastChar(TEXT('.'), DotIndex))
		{
			Name.RightChopInline(DotIndex + 1);
		}
		return Name;
	}

	/** 사용자가 친 이름(Downward / ability.moss.downward)을 태그 목록에서 찾는다. 못 찾으면 빈 태그 */
	FGameplayTag FindPatternTag(const FGameplayTagContainer& PatternTags, const FString& Input)
	{
		for (const FGameplayTag& Tag : PatternTags)
		{
			const FString TagName = Tag.ToString();
			if (TagName.Equals(Input, ESearchCase::IgnoreCase) || TagName.EndsWith(TEXT(".") + Input, ESearchCase::IgnoreCase))
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	/** 캐릭터의 직업 이름. 직업 데이터의 Display Name, 비어 있으면 Class 태그 끝 부분. 예) "Berserker" */
	FString GetClassName(const APawn* Pawn)
	{
		const ALOACharacter* Character = Cast<ALOACharacter>(Pawn);
		const ULOACharacterClassData* ClassData = Character ? Character->GetCharacterClassData() : nullptr;
		if (!ClassData)
		{
			return TEXT("?");
		}
		return ClassData->DisplayName.IsEmpty() ? GetLastTagPart(ClassData->ClassTag) : ClassData->DisplayName.ToString();
	}

	/** 이 월드의 보스. 없으면 nullptr */
	ALOABossCharacter* FindBoss(const UObject* WorldContext)
	{
		const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(WorldContext);
		return Raid ? Raid->GetCurrentBoss() : nullptr;
	}
}

// =============================================================================
// 확장 · 공용 도우미
// =============================================================================

void ULOACheatManager::InitCheatManager()
{
	Super::InitCheatManager();

	// 플레이어 테스터 (BP에서 지정 안 했으면 C++ 기본 클래스. 창은 BP 자식에서 지정)
	const TSubclassOf<ULOAPlayerCheats> ExtensionClass = PlayerCheatsClass ? PlayerCheatsClass : TSubclassOf<ULOAPlayerCheats>(ULOAPlayerCheats::StaticClass());
	AddCheatManagerExtension(NewObject<ULOAPlayerCheats>(this, ExtensionClass));
}

FString ULOACheatManager::GetPlayerClassName(const APawn* Pawn)
{
	return LOACheat::GetClassName(Pawn);
}

bool ULOACheatManager::ForwardToServer(const FString& Command) const
{
	ALOAPlayerController* PC = Cast<ALOAPlayerController>(GetOuterAPlayerController());
	if (!PC || PC->HasAuthority())
	{
		return false;	// 스탠드얼론·리슨 호스트 = 여기가 서버 → 바로 실행
	}

	PC->ServerCheat(Command);
	return true;
}

TArray<APawn*> ULOACheatManager::GetPlayersInJoinOrder() const
{
	TArray<APlayerState*> Players;
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetPawn())
			{
				Players.Add(PlayerState);
			}
		}
	}

	// 들어온 순서 = PlayerId 순서 (리슨 서버면 호스트가 가장 먼저). 클라와 서버가 같은 순서로 센다.
	Players.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });

	TArray<APawn*> Pawns;
	for (const APlayerState* PlayerState : Players)
	{
		Pawns.Add(PlayerState->GetPawn());
	}
	return Pawns;
}

bool ULOACheatManager::ResolveTarget(const FString& Target, APawn*& OutPawn) const
{
	OutPawn = nullptr;

	// Me = 이 치트 매니저의 PC = 명령을 보낸 플레이어 (서버에서도 그 플레이어의 PC에서 실행되므로)
	if (Target.IsEmpty() || Target.Equals(TEXT("Me"), ESearchCase::IgnoreCase))
	{
		const APlayerController* PC = GetOuterAPlayerController();
		OutPawn = PC ? PC->GetPawn() : nullptr;
		return true;
	}

	if (Target.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	// P1, P2 … (1부터)
	if (Target.StartsWith(TEXT("P"), ESearchCase::IgnoreCase))
	{
		const int32 Index = FCString::Atoi(*Target.Mid(1)) - 1;
		const TArray<APawn*> Players = GetPlayersInJoinOrder();
		if (Players.IsValidIndex(Index))
		{
			OutPawn = Players[Index];
			return true;
		}
	}
	return false;
}

// =============================================================================
// 콘솔 명령 (서버에서 실행)
// =============================================================================

void ULOACheatManager::LOABossPattern(const FString& PatternName, const FString& Target)
{
	// 서버로 보낼 명령. 이름이 없으면 목록 요청만 (목록은 서버 로그에 찍힌다)
	const FString Command = PatternName.IsEmpty()
		? FString(TEXT("LOABossPattern"))
		: FString::Printf(TEXT("LOABossPattern %s %s"), *PatternName, Target.IsEmpty() ? TEXT("Me") : *Target);
	if (ForwardToServer(Command))
	{
		return;
	}

	ALOABossCharacter* Boss = LOACheat::FindBoss(this);
	UAbilitySystemComponent* ASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] No boss in this level."));
		return;
	}

	FGameplayTagContainer PatternTags;
	ULOAMonsterAbility::GetPatternTags(ASC, PatternTags);

	// 이름 없이 치면 목록만
	if (PatternName.IsEmpty())
	{
		LOA_LOG(this, Display, TEXT("[Cheat] %s patterns: %s"), *GetNameSafe(Boss),
			PatternTags.IsEmpty() ? TEXT("(none)") : *PatternTags.ToStringSimple());
		return;
	}

	const FGameplayTag PatternTag = LOACheat::FindPatternTag(PatternTags, PatternName);
	if (!PatternTag.IsValid())
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Pattern '%s' not found. Available: %s"), *PatternName,
			PatternTags.IsEmpty() ? TEXT("(none)") : *PatternTags.ToStringSimple());
		return;
	}

	APawn* TargetPawn = nullptr;
	if (!ResolveTarget(Target, TargetPawn))
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Unknown target '%s'. Use Me, None, P1, P2 ..."), *Target);
		return;
	}

	// 콘솔·StateTree와 같은 창구. 쿨타임 중 / 다른 패턴 중 / 대상 필수인데 없음 → false
	const bool bActivated = ULOAMonsterAbility::TryActivatePattern(ASC, PatternTag, TargetPawn);
	LOA_LOG(this, Display, TEXT("[Cheat] %s requested %s (target %s) -> %s"),
		*GetNameSafe(GetOuterAPlayerController()), *PatternTag.ToString(), *GetNameSafe(TargetPawn),
		bActivated ? TEXT("activated") : TEXT("not activated (cooldown, another pattern, or target required)"));
}

void ULOACheatManager::LOABossAI(const FString& Mode)
{
	const bool bPause = Mode.Equals(TEXT("Pause"), ESearchCase::IgnoreCase);
	const bool bResume = Mode.Equals(TEXT("Resume"), ESearchCase::IgnoreCase);
	if (!bPause && !bResume)
	{
		LOA_LOG(this, Display, TEXT("[Cheat] Usage: LOABossAI Pause | Resume"));
		return;
	}

	if (ForwardToServer(FString::Printf(TEXT("LOABossAI %s"), *Mode)))
	{
		return;
	}

	ALOABossCharacter* Boss = LOACheat::FindBoss(this);
	ALOAMonsterAIController* BossController = Boss ? Cast<ALOAMonsterAIController>(Boss->GetController()) : nullptr;
	if (!BossController)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] No boss with a LOAMonsterAIController in this level."));
		return;
	}

	// 내가 건 왕복이 있으면 먼저 끝내고 걷던 길도 지운다 (재개는 이동을 멈추지 않아서, 안 지우면 왕복 점까지 마저 걸어감)
	//   다른 플레이어가 건 왕복은 AI가 이동을 끊을 때 그쪽이 스스로 끝난다
	if (SweepController.IsValid())
	{
		StopBossSweep(bPause ? TEXT("AI paused") : TEXT("AI resumed"));
		BossController->StopMovement();
	}

	BossController->SetBrainPaused(bPause);
	LOA_LOG(this, Display, TEXT("[Cheat] %s %s the boss AI."), *GetNameSafe(GetOuterAPlayerController()),
		bPause ? TEXT("paused") : TEXT("resumed"));
}

// =============================================================================
// 보스 왕복 (헤더 [보스 왕복 LOABossSweep])
// =============================================================================

void ULOACheatManager::LOABossSweep(float Distance, float WaitSeconds)
{
	if (ForwardToServer(FString::Printf(TEXT("LOABossSweep %.0f %.2f"), Distance, WaitSeconds)))
	{
		return;
	}

	ALOABossCharacter* Boss = LOACheat::FindBoss(this);
	ALOAMonsterAIController* BossController = Boss ? Cast<ALOAMonsterAIController>(Boss->GetController()) : nullptr;
	const APlayerController* PC = GetOuterAPlayerController();
	const APawn* MyPawn = PC ? PC->GetPawn() : nullptr;
	if (!BossController || !MyPawn)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Boss sweep needs a boss with a LOAMonsterAIController and your character."));
		return;
	}
	if (Distance <= 0.f)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Usage: LOABossSweep [distance > 0] [wait seconds]"));
		return;
	}

	// 전에 건 왕복은 버린다 (알림 구독을 먼저 끊어야 아래 AI 멈춤의 "이동 끊김" 알림을 안 받는다)
	StopBossSweep(TEXT("restarted"));

	// AI 멈춤 = 트리 정지 + 걷던 길 지우기 (LOABossAI Pause와 같음)
	BossController->SetBrainPaused(true);

	// "보스 → 나" 선 (바닥 기준). 보스가 내 자리에 딱 있으면 보스가 보는 방향
	const FVector MyLocation = MyPawn->GetActorLocation();
	FVector Direction = (MyLocation - Boss->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Boss->GetActorForwardVector().GetSafeNormal2D();
	}

	// 0 = 나를 지나 먼 쪽 (먼저 감), 1 = 보스가 있던 쪽
	SweepPoints[0] = MyLocation + Direction * Distance;
	SweepPoints[1] = MyLocation - Direction * Distance;
	SweepTargetIndex = 0;
	SweepWaitSeconds = FMath::Max(0.f, WaitSeconds);
	SweepController = BossController;
	BossController->ReceiveMoveCompleted.AddUniqueDynamic(this, &ThisClass::OnSweepMoveCompleted);

	LOA_LOG(this, Display, TEXT("[Cheat] %s started a boss sweep: %.0fcm each side of %s, wait %.2fs. End with LOABossAI Resume | Pause"),
		*GetNameSafe(PC), Distance, *MyLocation.ToCompactString(), SweepWaitSeconds);

	MoveToNextSweepPoint();
}

void ULOACheatManager::MoveToNextSweepPoint()
{
	ALOAMonsterAIController* BossController = SweepController.Get();
	const APawn* BossPawn = BossController ? BossController->GetPawn() : nullptr;
	if (!BossPawn)
	{
		StopBossSweep(TEXT("no boss"));
		return;
	}

	// 멈춰 있는 동안 AI가 다시 켜졌거나(다른 플레이어의 Resume) 보스가 죽었으면 끝
	const UStateTreeAIComponent* StateTree = BossController->GetStateTreeComponent();
	const ALOABossCharacter* Boss = Cast<ALOABossCharacter>(BossPawn);
	const UAbilitySystemComponent* ASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if ((StateTree && StateTree->IsRunning()) || (ASC && ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead)))
	{
		StopBossSweep(TEXT("AI running or boss dead"));
		return;
	}

	// StateTree의 Move To와 같은 길찾기 걷기.
	//   bStopOnOverlap false = 보스 반지름만큼 앞에서 서지 않고 그 점까지 / 점이 내비메시 밖이면 내비메시 위로 옮김 / 못 가면 갈 수 있는 데까지
	const FVector& Point = SweepPoints[SweepTargetIndex];
	const EPathFollowingRequestResult::Type Request = BossController->MoveToLocation(Point, /*AcceptanceRadius*/ -1.f, /*bStopOnOverlap*/ false,
		/*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
	if (Request == EPathFollowingRequestResult::Failed)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Boss sweep: no path to %s. Stopped."), *Point.ToCompactString());
		StopBossSweep(TEXT("no path"));
		return;
	}

	LOA_LOG(this, Log, TEXT("[Cheat] Boss sweep: walking to point %d %s"), SweepTargetIndex, *Point.ToCompactString());
}

void ULOACheatManager::OnSweepMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	// 성공이 아님 = 누가 이동을 끊었다 (AI 멈춤·재개, 사망, 전멸) → 왕복 끝, 보스는 그대로
	if (Result != EPathFollowingResult::Success)
	{
		StopBossSweep(TEXT("move interrupted"));
		return;
	}

	// 도착 → 반대쪽 점. 멈춘 뒤 출발 (0초여도 다음 프레임: 이동 끝 알림 안에서 새 이동을 걸지 않는다)
	SweepTargetIndex = 1 - SweepTargetIndex;
	if (UWorld* World = GetWorld())
	{
		if (SweepWaitSeconds > 0.f)
		{
			World->GetTimerManager().SetTimer(SweepWaitTimer, this, &ThisClass::MoveToNextSweepPoint, SweepWaitSeconds, false);
		}
		else
		{
			SweepWaitTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::MoveToNextSweepPoint);
		}
	}
}

void ULOACheatManager::StopBossSweep(const TCHAR* Reason)
{
	ALOAMonsterAIController* BossController = SweepController.Get();
	const bool bWasSweeping = SweepController.IsValid();

	if (BossController)
	{
		BossController->ReceiveMoveCompleted.RemoveDynamic(this, &ThisClass::OnSweepMoveCompleted);
	}
	SweepController.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SweepWaitTimer);
	}

	if (bWasSweeping)
	{
		LOA_LOG(this, Display, TEXT("[Cheat] Boss sweep ended (%s)."), Reason);
	}
}

void ULOACheatManager::LOABossHeal()
{
	if (ForwardToServer(TEXT("LOABossHeal")))
	{
		return;
	}

	ALOABossCharacter* Boss = LOACheat::FindBoss(this);
	UAbilitySystemComponent* ASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] No boss in this level."));
		return;
	}

	// 죽은 보스는 살리지 않는다. (사망 처리 GA·충돌·AI가 이미 끝난 상태)
	if (ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead))
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] The boss is dead. Restart PIE to test again."));
		return;
	}

	// 테스트 도구라 GE 없이 체력 기본값을 바로 바꾼다. (복제·체력바 알림은 스탯창이 평소처럼 한다)
	const float MaxHealth = ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute());
	ASC->SetNumericAttributeBase(ULOAAttributeSet_Common::GetHealthAttribute(), MaxHealth);
	LOA_LOG(this, Display, TEXT("[Cheat] %s restored the boss health to %.0f"), *GetNameSafe(GetOuterAPlayerController()), MaxHealth);
}

void ULOACheatManager::LOABossHealth(float Percent)
{
	// 1% 아래는 1%로: 0이면 체력 0 = 죽음 처리가 아니라 값만 0이 되어 이상한 상태가 된다 (LOABossHeal처럼 GE 없이 값만 바꿈)
	const float Clamped = FMath::Clamp(Percent, 1.f, 100.f);
	if (ForwardToServer(FString::Printf(TEXT("LOABossHealth %.2f"), Clamped)))
	{
		return;
	}

	ALOABossCharacter* Boss = LOACheat::FindBoss(this);
	UAbilitySystemComponent* ASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] No boss in this level."));
		return;
	}

	if (ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead))
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] The boss is dead. Restart PIE to test again."));
		return;
	}

	// 예) 최대 100,000, 39% → 39,000. 테스트 도구라 LOABossHeal처럼 기본값을 바로 바꾼다 (복제·체력바 알림은 스탯창이 평소처럼)
	const float MaxHealth = ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute());
	const float NewHealth = MaxHealth * Clamped / 100.f;
	ASC->SetNumericAttributeBase(ULOAAttributeSet_Common::GetHealthAttribute(), NewHealth);
	LOA_LOG(this, Display, TEXT("[Cheat] %s set the boss health to %.0f (%.1f%%)"), *GetNameSafe(GetOuterAPlayerController()), NewHealth, Clamped);
}

void ULOACheatManager::LOABossStagger(const FString& Amount)
{
	if (ForwardToServer(FString::Printf(TEXT("LOABossStagger %s"), *Amount)))
	{
		return;
	}

	ALOABossCharacter* Boss = LOACheat::FindBoss(this);
	UAbilitySystemComponent* BossASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!BossASC)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] No boss in this level."));
		return;
	}

	if (!BossASC->HasAttributeSetForAttribute(ULOAAttributeSet_Boss::GetStaggerAttribute()))
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] The boss has no stagger gauge (LOAAttributeSet_Boss is not in its ability data set)."));
		return;
	}

	// Refill = 다시 가득 (전멸·대난투 끝과 같은 함수)
	if (Amount.Equals(TEXT("Refill"), ESearchCase::IgnoreCase))
	{
		Boss->RefillStagger();
		return;
	}

	// 빈칸 = 남은 만큼 전부. 예) 남은 70 → 70 깎음 → 0 → Event.Trigger.StaggerBroken → 대난투
	const float Remaining = BossASC->GetNumericAttribute(ULOAAttributeSet_Boss::GetStaggerAttribute());
	const float StaggerAmount = Amount.IsEmpty() ? Remaining : FCString::Atof(*Amount);
	if (StaggerAmount <= 0.f)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Nothing to break (remaining %.1f, amount '%s'). Use LOABossStagger Refill first."), Remaining, *Amount);
		return;
	}

	// 스킬과 같은 길: 명령을 친 플레이어가 보스에게 무력화 GE를 건다 (마지막으로 깎은 사람 = 이 플레이어)
	APlayerController* PC = GetOuterAPlayerController();
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC ? PC->GetPawn() : nullptr);
	if (!SourceASC)
	{
		SourceASC = BossASC;	// 몸이 없으면 (관전 등) 보스가 자기에게
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ULOAGE_Stagger::StaticClass(), 1.f, SourceASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Stagger, StaggerAmount);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, BossASC);

	LOA_LOG(this, Display, TEXT("[Cheat] %s staggered the boss by %.1f (remaining was %.1f)"), *GetNameSafe(PC), StaggerAmount, Remaining);
}

void ULOACheatManager::LOATerrainCollapse(const FString& Side)
{
	if (ForwardToServer(FString::Printf(TEXT("LOATerrainCollapse %s"), *Side)))
	{
		return;
	}

	ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this);
	if (!Raid)
	{
		return;
	}

	bool bDone = false;
	if (Side.IsEmpty())
	{
		bDone = Raid->RequestTerrainCollapse();
	}
	else if (Side.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
	{
		bDone = Raid->RequestTerrainCollapse(ELOATerrainSide::Left);
	}
	else if (Side.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
	{
		bDone = Raid->RequestTerrainCollapse(ELOATerrainSide::Right);
	}
	else if (Side.Equals(TEXT("Reset"), ESearchCase::IgnoreCase))
	{
		Raid->ResetTerrain();
		bDone = true;
	}
	else
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] LOATerrainCollapse: unknown side '%s'. Use Left, Right, Reset or nothing."), *Side);
		return;
	}

	LOA_LOG(this, Display, TEXT("[Cheat] %s LOATerrainCollapse %s : %s"),
		*GetNameSafe(GetOuterAPlayerController()), *Side, bDone ? TEXT("done") : TEXT("nothing to do"));
}

void ULOACheatManager::LOASlomo(float TimeDilation)
{
	const float Clamped = FMath::Clamp(TimeDilation, LOACheat::MinTimeDilation, LOACheat::MaxTimeDilation);
	if (ForwardToServer(FString::Printf(TEXT("LOASlomo %.3f"), Clamped)))
	{
		return;
	}

	// 엔진 slomo와 같은 길: World Settings의 배속을 바꾸고, 엔진이 모든 클라에 복제한다.
	UGameplayStatics::SetGlobalTimeDilation(this, Clamped);
	LOA_LOG(this, Display, TEXT("[Cheat] %s set time dilation %.2f"), *GetNameSafe(GetOuterAPlayerController()), Clamped);
}

// =============================================================================
// 콘솔 명령 (이 컴퓨터만)
// =============================================================================

void ULOACheatManager::LOAHitShape(bool bShow)
{
	ULOACombatLibrary::SetHitShapeDebugEnabled(bShow);
	LOA_LOG(this, Display, TEXT("[Cheat] Hit shape drawing %s on this machine."), bShow ? TEXT("ON") : TEXT("OFF"));
}

bool ULOACheatManager::IsHitShapeVisible() const
{
	return ULOACombatLibrary::IsHitShapeDebugEnabled();
}

void ULOACheatManager::LOATester()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// 열려 있으면 닫는다. (위젯은 남겨 두고 다음에 다시 붙인다)
	if (TesterWindow && TesterWindow->IsInViewport())
	{
		TesterWindow->RemoveFromParent();
		return;
	}

	if (!TesterWindow)
	{
		if (!TesterWindowClass)
		{
			LOA_LOG(this, Warning, TEXT("[Cheat] Tester Window Class is not set. Set it on B_LOACheatManager and use it as the Cheat Class of BP_LOAPlayerController."));
			return;
		}
		TesterWindow = CreateWidget<UUserWidget>(PC, TesterWindowClass);
	}

	// 붙일 때마다 창의 Event Construct가 불린다 → 창이 RefreshTesterLists를 불러 목록을 새로 채운다.
	if (TesterWindow)
	{
		TesterWindow->AddToViewport(100);
	}
}

// =============================================================================
// 패턴 테스터 창 (이 컴퓨터만)
// =============================================================================

void ULOACheatManager::RefreshTesterLists()
{
	PatternNames.Reset();
	PatternLabels.Reset();
	TargetTokens.Reset();
	TargetLabels.Reset();

	// ---------- 패턴: 보스 BP의 Ability Data Sets 에셋에서 ----------
	// (클라에는 보스 ASC의 스킬 목록이 복제되지 않는다. 에셋은 어느 컴퓨터에나 있다)
	if (const ALOABossCharacter* Boss = LOACheat::FindBoss(this))
	{
		TArray<TPair<FString, FString>> Found;	// (이름, 보이는 글자)
		for (const ULOAMonsterAbilityDataSet* DataSet : Boss->GetAbilityDataSets())
		{
			if (!DataSet)
			{
				continue;
			}

			for (const FLOAMonsterAbilityDataSet_Ability& Entry : DataSet->GetGrantedAbilities())
			{
				// 패턴 = LOAMonsterAbility. 사망 GA 같은 것은 건너뛴다. (클래스 기본값에서 태그·Target Usage를 읽는다)
				const ULOAMonsterAbility* Pattern = Entry.Ability ? Cast<ULOAMonsterAbility>(Entry.Ability->GetDefaultObject()) : nullptr;
				if (!Pattern)
				{
					continue;
				}

				for (const FGameplayTag& Tag : Pattern->GetAssetTags())
				{
					const FString Name = LOACheat::GetLastTagPart(Tag);
					if (Found.ContainsByPredicate([&Name](const TPair<FString, FString>& Existing) { return Existing.Key == Name; }))
					{
						continue;
					}

					FString Label = Name;
					if (Pattern->GetTargetUsage() == ELOAPatternTargetUsage::Required)
					{
						Label += TEXT(" (대상 필수)");
					}
					else if (Pattern->GetTargetUsage() == ELOAPatternTargetUsage::None)
					{
						Label += TEXT(" (대상 안 씀)");
					}
					Found.Emplace(Name, Label);
				}
			}
		}

		Found.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B) { return A.Key < B.Key; });
		for (const TPair<FString, FString>& Pair : Found)
		{
			PatternNames.Add(Pair.Key);
			PatternLabels.Add(Pair.Value);
		}
	}

	// ---------- 대상: 나 → 다른 플레이어 (들어온 순서) → 없음 ----------
	const APlayerController* PC = GetOuterAPlayerController();
	const APawn* MyPawn = PC ? PC->GetPawn() : nullptr;

	TargetTokens.Add(TEXT("Me"));
	TargetLabels.Add(TEXT("나"));	// 아래에서 내 번호를 찾으면 덮어쓴다

	const TArray<APawn*> Players = GetPlayersInJoinOrder();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const FString ClassName = LOACheat::GetClassName(Players[Index]);
		if (Players[Index] == MyPawn)
		{
			TargetLabels[0] = FString::Printf(TEXT("나 (P%d · %s)"), Index + 1, *ClassName);
			continue;
		}
		TargetTokens.Add(FString::Printf(TEXT("P%d"), Index + 1));
		TargetLabels.Add(FString::Printf(TEXT("P%d (%s)"), Index + 1, *ClassName));
	}

	TargetTokens.Add(TEXT("None"));
	TargetLabels.Add(TEXT("없음"));
}

TArray<FString> ULOACheatManager::GetTesterPatternLabels() const
{
	return PatternLabels;
}

TArray<FString> ULOACheatManager::GetTesterTargetLabels() const
{
	return TargetLabels;
}

void ULOACheatManager::ActivateTesterPattern(int32 PatternIndex, int32 TargetIndex)
{
	if (!PatternNames.IsValidIndex(PatternIndex))
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] No pattern selected (index %d, %d patterns). Is the boss in this level?"),
			PatternIndex, PatternNames.Num());
		return;
	}

	LOABossPattern(PatternNames[PatternIndex], TargetTokens.IsValidIndex(TargetIndex) ? TargetTokens[TargetIndex] : TEXT("Me"));
}
