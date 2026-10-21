// LOACheatManager.cpp

#include "Debug/LOACheatManager.h"
#include "Player/LOAPlayerController.h"
#include "AbilitySystem/Abilities/Monster/LOAMonsterAbility.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AI/LOAMonsterAIController.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Character/Player/LOACharacter.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Data/LOAAbilityDataSet.h"
#include "Data/LOACharacterClassData.h"
#include "Game/LOARaidSubsystem.h"
#include "Library/LOACombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
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
// 서버로 보내기 / 대상 찾기
// =============================================================================

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

	BossController->SetBrainPaused(bPause);
	LOA_LOG(this, Display, TEXT("[Cheat] %s %s the boss AI."), *GetNameSafe(GetOuterAPlayerController()),
		bPause ? TEXT("paused") : TEXT("resumed"));
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
	if (ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Dead))
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] The boss is dead. Restart PIE to test again."));
		return;
	}

	// 테스트 도구라 GE 없이 체력 기본값을 바로 바꾼다. (복제·체력바 알림은 스탯창이 평소처럼 한다)
	const float MaxHealth = ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute());
	ASC->SetNumericAttributeBase(ULOAAttributeSet_Common::GetHealthAttribute(), MaxHealth);
	LOA_LOG(this, Display, TEXT("[Cheat] %s restored the boss health to %.0f"), *GetNameSafe(GetOuterAPlayerController()), MaxHealth);
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
