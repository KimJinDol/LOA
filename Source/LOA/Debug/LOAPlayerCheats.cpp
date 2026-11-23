// LOAPlayerCheats.cpp

#include "Debug/LOAPlayerCheats.h"
#include "Debug/LOACheatManager.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/LOAGA_JustGuard.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "AbilitySystem/Effects/LOAGE_IdentityGain.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Combat/LOAJustGuardComponent.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Game/LOARaidSubsystem.h"
#include "Player/LOAPlayerController.h"

#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPlayerCheats)

namespace LOAPlayerCheat
{
	/** 내 캐릭터의 우리 ASC. 없으면 nullptr */
	ULOAAbilitySystemComponent* GetMyASC(const APlayerController* PC)
	{
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		return Cast<ULOAAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn));
	}
}

// =============================================================================
// 붙음·떨어짐
// =============================================================================

void ULOAPlayerCheats::AddedToCheatManager_Implementation()
{
	Super::AddedToCheatManager_Implementation();

	// 캐릭터가 바뀌면(부활) 자동 저가를 새 캐릭터에 다시 붙인다. 이 알림은 클라에서도 온다 (엔진 Controller.cpp OnRep_Pawn)
	if (APlayerController* PC = GetPlayerController())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::OnPossessedPawnChanged);
	}
}

void ULOAPlayerCheats::RemovedFromCheatManager_Implementation()
{
	if (APlayerController* PC = GetPlayerController())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessedPawnChanged);
	}
	ReleaseGuard();
	UnbindAutoGuard();
	AutoGuardDelay = -1.f;

	Super::RemovedFromCheatManager_Implementation();
}

ULOACheatManager* ULOAPlayerCheats::GetLOACheatManager() const
{
	return Cast<ULOACheatManager>(GetOuter());
}

void ULOAPlayerCheats::RunOnPlayerComputer(APawn* Player, const FString& LocalCommand) const
{
	ALOAPlayerController* PC = Player ? Cast<ALOAPlayerController>(Player->GetController()) : nullptr;
	if (!PC)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] %s has no player controller."), *GetNameSafe(Player));
		return;
	}

	// 리슨 호스트 본인 = 이 컴퓨터 → 그 PC의 치트 매니저에서 바로 실행
	if (PC->IsLocalController())
	{
		PC->ConsoleCommand(LocalCommand);
		return;
	}

	// 원격 플레이어 → 그 컴퓨터에서 실행
	PC->ClientCheat(LocalCommand);
}

TArray<APawn*> ULOAPlayerCheats::ResolvePlayers(const FString& Target) const
{
	TArray<APawn*> Players;
	const ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return Players;
	}

	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (TargetText.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		return CheatManager->GetPlayersInJoinOrder();
	}

	APawn* Player = nullptr;
	if (!CheatManager->ResolveTarget(TargetText, Player) || !Player)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Unknown player '%s'. Use Me, All, P1, P2 …"), *TargetText);
		return Players;
	}
	Players.Add(Player);
	return Players;
}

// =============================================================================
// 콘솔 명령
// =============================================================================

void ULOAPlayerCheats::LOAAutoGuard(const FString& Target, const FString& Delay)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	// Off·빈 칸 = 끔. 숫자 = 구간이 열린 뒤 몇 초
	const bool bOff = Delay.IsEmpty() || Delay.Equals(TEXT("Off"), ESearchCase::IgnoreCase);
	const float DelaySeconds = bOff ? -1.f : FMath::Max(0.f, FCString::Atof(*Delay));
	const FString DelayText = bOff ? FString(TEXT("Off")) : FString::Printf(TEXT("%.3f"), DelaySeconds);
	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;

	// 서버 권한이 없으면 서버로 보낸다 (대상 찾기는 서버만 정확히 안다)
	if (CheatManager->ForwardToServer(FString::Printf(TEXT("LOAAutoGuard %s %s"), *TargetText, *DelayText)))
	{
		return;
	}

	// [서버] 대상 찾기
	TArray<APawn*> Players;
	if (TargetText.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		Players = CheatManager->GetPlayersInJoinOrder();
	}
	else
	{
		APawn* Player = nullptr;
		if (!CheatManager->ResolveTarget(TargetText, Player) || !Player)
		{
			LOA_LOG(this, Warning, TEXT("[Cheat] Unknown player '%s'. Use Me, All, P1, P2 …"), *TargetText);
			return;
		}
		Players.Add(Player);
	}

	// 그 플레이어 컴퓨터에서 켜고 끈다 (누르기는 그 사람 컴퓨터에서만 할 수 있다)
	const FString LocalCommand = FString::Printf(TEXT("LOAAutoGuardLocal %.3f"), DelaySeconds);
	for (APawn* Player : Players)
	{
		LOA_LOG(this, Display, TEXT("[Cheat] auto guard %s → %s"), *DelayText, *GetNameSafe(Player));
		RunOnPlayerComputer(Player, LocalCommand);
	}
}

void ULOAPlayerCheats::LOAAutoGuardLocal(float Delay)
{
	const APlayerController* PC = GetPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (Delay < 0.f)
	{
		ReleaseGuard();
		UnbindAutoGuard();
		AutoGuardDelay = -1.f;
		LOA_LOG(this, Display, TEXT("[Cheat] auto guard OFF"));
		return;
	}

	AutoGuardDelay = Delay;
	BindAutoGuard();
	LOA_LOG(this, Display, TEXT("[Cheat] auto guard ON: press %.2fs after the window opens"), Delay);
}

void ULOAPlayerCheats::LOAPlayerTester()
{
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// 열려 있으면 닫는다. (위젯은 남겨 두고 다음에 다시 붙인다. ULOACheatManager::LOATester와 같은 방식)
	if (TesterWindow && TesterWindow->IsInViewport())
	{
		TesterWindow->RemoveFromParent();
		return;
	}

	if (!TesterWindow)
	{
		if (!TesterWindowClass)
		{
			LOA_LOG(this, Warning, TEXT("[Cheat] Player Tester Window Class is not set. Set it on B_LOAPlayerCheats and use it as the Player Cheats Class of B_LOACheatManager."));
			return;
		}
		TesterWindow = CreateWidget<UUserWidget>(PC, TesterWindowClass);
	}

	// 붙일 때마다 창의 Event Construct가 불린다 → 창이 RefreshPlayerTesterLists를 불러 목록을 새로 채운다.
	if (TesterWindow)
	{
		TesterWindow->AddToViewport(100);
	}
}

// =============================================================================
// 보호막·회복 테스트 (헤더 [보호막·회복 테스트 규칙])
// =============================================================================

void ULOAPlayerCheats::LOAShield(float Amount, float Duration, int32 Skill, const FString& Target)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	// 서버 권한이 없으면 서버로 보낸다 (GE는 서버가 건다)
	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (CheatManager->ForwardToServer(FString::Printf(TEXT("LOAShield %.1f %.2f %d %s"), Amount, Duration, Skill, *TargetText)))
	{
		return;
	}

	if (Amount <= 0.f || Duration <= 0.f)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] LOAShield needs Amount > 0 and Duration > 0. Example: LOAShield 500 3 1"));
		return;
	}

	for (APawn* Player : ResolvePlayers(TargetText))
	{
		ApplyTestEffect(Player, ShieldEffectClass, LOAGameplayTags::SetByCaller_ShieldRate, Amount, Duration, Skill, TEXT("shield"));
	}
}

void ULOAPlayerCheats::LOAHeal(float Amount, const FString& Target)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (CheatManager->ForwardToServer(FString::Printf(TEXT("LOAHeal %.1f %s"), Amount, *TargetText)))
	{
		return;
	}

	if (Amount <= 0.f)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] LOAHeal needs Amount > 0. Example: LOAHeal 300"));
		return;
	}

	for (APawn* Player : ResolvePlayers(TargetText))
	{
		ApplyTestEffect(Player, HealEffectClass, LOAGameplayTags::SetByCaller_HealRate, Amount, 0.f, 0, TEXT("heal"));
	}
}

void ULOAPlayerCheats::LOAHurt(float Amount, const FString& Target)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (CheatManager->ForwardToServer(FString::Printf(TEXT("LOAHurt %.1f %s"), Amount, *TargetText)))
	{
		return;
	}

	if (Amount <= 0.f)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] LOAHurt needs Amount > 0. Example: LOAHurt 400"));
		return;
	}

	for (APawn* Player : ResolvePlayers(TargetText))
	{
		ApplyTestEffect(Player, HurtEffectClass, LOAGameplayTags::SetByCaller_DamageRate, Amount, 0.f, 0, TEXT("hurt"));
	}
}

void ULOAPlayerCheats::LOAShieldInfo(const FString& Target)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	// 서버가 가진 GE 목록을 본다 (깎인 양·남은 시간이 정확한 쪽)
	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (CheatManager->ForwardToServer(FString::Printf(TEXT("LOAShieldInfo %s"), *TargetText)))
	{
		return;
	}

	for (APawn* Player : ResolvePlayers(TargetText))
	{
		LogShields(Player);
	}
}

bool ULOAPlayerCheats::ApplyTestEffect(APawn* Player, TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& RateTag,
	float Amount, float Duration, int32 Skill, const TCHAR* What) const
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
	if (!ASC)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] %s: %s has no ability system."), What, *GetNameSafe(Player));
		return false;
	}
	if (!EffectClass)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] %s: effect class is not set. Set Shield / Heal / Hurt Effect Class on B_LOAPlayerCheats."), What);
		return false;
	}

	// ① 배율 = 양 ÷ 공격력 (계산식이 공격력 × 배율이라). 공격력이 0이면 무엇을 넣어도 0이다
	const float AttackPower = ASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetAttackPowerAttribute());
	if (AttackPower <= 0.f)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] %s: %s has AttackPower %.1f. Cannot turn %.0f into a rate."), What, *GetNameSafe(Player), AttackPower, Amount);
		return false;
	}
	const float Rate = Amount / AttackPower;

	// ② 시전자 = 대상 자신. 스킬 번호가 있으면 그 스킬을 Effect Context에 (덮어쓰기가 "같은 스킬"을 여기서 읽는다)
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	FString SkillText = TEXT("none");
	if (Skill > 0)
	{
		const TArray<FGameplayAbilitySpec>& Abilities = ASC->GetActivatableAbilities();
		if (!Abilities.IsValidIndex(Skill - 1) || !Abilities[Skill - 1].Ability)
		{
			LOA_LOG(this, Warning, TEXT("[Cheat] %s: skill #%d is out of range. %s has %d skill(s):"), What, Skill, *GetNameSafe(Player), Abilities.Num());
			for (int32 Index = 0; Index < Abilities.Num(); ++Index)
			{
				LOA_LOG(this, Warning, TEXT("[Cheat]   %d = %s"), Index + 1, *GetNameSafe(Abilities[Index].Ability));
			}
			return false;
		}
		Context.SetAbility(Abilities[Skill - 1].Ability);
		SkillText = FString::Printf(TEXT("#%d %s"), Skill, *GetNameSafe(Abilities[Skill - 1].Ability));
	}

	// ③ Spec + 배율·시간 → 자기에게
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(RateTag, Rate);
	if (Duration > 0.f)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, Duration);
	}

	LOA_LOG(this, Display, TEXT("[Cheat] %s %.0f (rate %.3f, %.1fs, skill %s) → %s"),
		What, Amount, Rate, Duration, *SkillText, *GetNameSafe(Player));
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// ④ 결과 한 줄 (보호막은 걸린 목록까지)
	LOA_LOG(this, Display, TEXT("[Cheat]   → health %.0f / %.0f, shield %.0f"),
		ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute()),
		ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute()),
		ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetShieldAttribute()));
	if (EffectClass == ShieldEffectClass)
	{
		LogShields(Player);
	}
	return true;
}

void ULOAPlayerCheats::LogShields(APawn* Player) const
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
	const UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		return;
	}

	const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(LOAGameplayTags::State_Buff_Shield)));
	LOA_LOG(this, Display, TEXT("[Cheat] %s shield %.0f (%d effect(s))"),
		*GetNameSafe(Player), ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetShieldAttribute()), Handles.Num());

	const float WorldTime = World->GetTimeSeconds();
	for (const FActiveGameplayEffectHandle& Handle : Handles)
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (!Effect || !Effect->Spec.Def)
		{
			continue;
		}

		// 지금 몫 = Shield Modifier 값 (MMC가 깎인 양을 빼고 계산한 값)
		float EffectAmount = 0.f;
		const TArray<FGameplayModifierInfo>& Modifiers = Effect->Spec.Def->Modifiers;
		for (int32 Index = 0; Index < Modifiers.Num(); ++Index)
		{
			if (Modifiers[Index].Attribute == ULOAAttributeSet_Common::GetShieldAttribute())
			{
				EffectAmount += Effect->Spec.GetModifierMagnitude(Index, true);
			}
		}

		LOA_LOG(this, Display, TEXT("[Cheat]   skill %s : now %.0f, used %.0f, %.1fs left"),
			*GetNameSafe(Effect->Spec.GetContext().GetAbility()),
			EffectAmount,
			Effect->Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ShieldUsed, false, 0.f),
			Effect->GetTimeRemaining(WorldTime));
	}
}

// =============================================================================
// 낙인·공격력 버프 테스트 (2026-11-07)
// =============================================================================

void ULOAPlayerCheats::LOAMark(float Duration, const FString& Caster)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	const FString CasterText = Caster.IsEmpty() ? FString(TEXT("Me")) : Caster;
	if (CheatManager->ForwardToServer(FString::Printf(TEXT("LOAMark %.2f %s"), Duration, *CasterText)))
	{
		return;
	}

	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this);
	ALOABossCharacter* Boss = Raid ? Raid->GetCurrentBoss() : nullptr;
	UAbilitySystemComponent* BossASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!BossASC)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] mark: no boss in this map."));
		return;
	}

	const TArray<APawn*> Casters = ResolvePlayers(CasterText);
	UAbilitySystemComponent* CasterASC = Casters.Num() > 0 ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Casters[0]) : nullptr;
	if (!CasterASC)
	{
		return;
	}

	// 받는 피해 증가 칸은 2026-11-13 Common → Combat (GE_Mark도 Combat 칸으로 옮겨야 숫자가 바뀐다)
	const float Before = BossASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetIncomingDamageBonusAttribute());
	if (ApplyEffectFromCaster(CasterASC, BossASC, MarkEffectClass, Duration, TEXT("mark")))
	{
		// 기대값: 0.1 × (1 + 낙인력). 예) 낙인력 0.08 → 0.108
		LOA_LOG(this, Display, TEXT("[Cheat]   → %s incoming damage bonus %.3f → %.3f (caster mark power %.3f)"),
			*GetNameSafe(Boss), Before,
			BossASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetIncomingDamageBonusAttribute()),
			CasterASC->GetNumericAttribute(ULOAAttributeSet_Player::GetMarkPowerAttribute()));
	}
}

void ULOAPlayerCheats::LOABuff(float Duration, const FString& Target, const FString& Caster)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	const FString CasterText = Caster.IsEmpty() ? FString(TEXT("Me")) : Caster;
	if (CheatManager->ForwardToServer(FString::Printf(TEXT("LOABuff %.2f %s %s"), Duration, *TargetText, *CasterText)))
	{
		return;
	}

	const TArray<APawn*> Casters = ResolvePlayers(CasterText);
	UAbilitySystemComponent* CasterASC = Casters.Num() > 0 ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Casters[0]) : nullptr;
	if (!CasterASC)
	{
		return;
	}

	for (APawn* Player : ResolvePlayers(TargetText))
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
		if (!TargetASC)
		{
			continue;
		}
		const float Before = TargetASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetAttackPowerAttribute());
		if (ApplyEffectFromCaster(CasterASC, TargetASC, AttackBuffEffectClass, Duration, TEXT("attack buff")))
		{
			// 기대값: (대상 공격력 + 시전자 기본 공격력 × 0.15) × 1.06. 예) 서포터 1000, 딜러 2000 → 2279
			LOA_LOG(this, Display, TEXT("[Cheat]   → %s attack power %.0f → %.0f (base %.0f, caster base %.0f)"),
				*GetNameSafe(Player), Before,
				TargetASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetAttackPowerAttribute()),
				TargetASC->GetNumericAttributeBase(ULOAAttributeSet_Combat::GetAttackPowerAttribute()),
				CasterASC->GetNumericAttributeBase(ULOAAttributeSet_Combat::GetAttackPowerAttribute()));
		}
	}
}

void ULOAPlayerCheats::LOAMarkPower(float Value, const FString& Target)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (!CheatManager || CheatManager->ForwardToServer(FString::Printf(TEXT("LOAMarkPower %.4f %s"), Value, *TargetText)))
	{
		return;
	}
	SetBaseStat(TargetText, ULOAAttributeSet_Player::GetMarkPowerAttribute(), Value, TEXT("mark power"));
}

void ULOAPlayerCheats::LOAAttackPower(float Value, const FString& Target)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (!CheatManager || CheatManager->ForwardToServer(FString::Printf(TEXT("LOAAttackPower %.1f %s"), Value, *TargetText)))
	{
		return;
	}
	SetBaseStat(TargetText, ULOAAttributeSet_Combat::GetAttackPowerAttribute(), Value, TEXT("attack power"));
}

void ULOAPlayerCheats::LOAIdentity(float Value, const FString& Target)
{
	ULOACheatManager* CheatManager = GetLOACheatManager();
	const FString TargetText = Target.IsEmpty() ? FString(TEXT("Me")) : Target;
	if (!CheatManager || CheatManager->ForwardToServer(FString::Printf(TEXT("LOAIdentity %.1f %s"), Value, *TargetText)))
	{
		return;
	}

	for (APawn* Player : ResolvePlayers(TargetText))
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
		if (!ASC)
		{
			continue;
		}

		const float Max = ASC->GetNumericAttribute(ULOAAttributeSet_Player::GetMaxIdentityGaugeAttribute());
		if (Max <= 0.f)
		{
			LOA_LOG(this, Warning, TEXT("[Cheat] identity: %s has no identity gauge (max 0)."), *GetNameSafe(Player));
			continue;
		}

		// 음수 = 가득. 그 외는 0 ~ 최대로 맞춤. 바꾸는 양만큼 진짜 수급 GE로 (스탯창이 범위를 다시 맞춘다)
		const float Before = ASC->GetNumericAttribute(ULOAAttributeSet_Player::GetIdentityGaugeAttribute());
		const float Goal = (Value < 0.f) ? Max : FMath::Clamp(Value, 0.f, Max);
		ULOAGE_IdentityGain::ApplyGaugeDelta(ASC, Goal - Before);

		LOA_LOG(this, Display, TEXT("[Cheat] identity: %s %.1f → %.1f / %.1f"),
			*GetNameSafe(Player), Before,
			ASC->GetNumericAttribute(ULOAAttributeSet_Player::GetIdentityGaugeAttribute()), Max);
	}
}

bool ULOAPlayerCheats::ApplyEffectFromCaster(UAbilitySystemComponent* CasterASC, UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> EffectClass, float Duration, const TCHAR* What) const
{
	if (!CasterASC || !TargetASC)
	{
		return false;
	}
	if (!EffectClass)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] %s: effect class is not set. Set Mark / Attack Buff Effect Class on B_LOAPlayerCheats."), What);
		return false;
	}
	if (Duration <= 0.f)
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] %s needs Duration > 0."), What);
		return false;
	}

	// 시전자의 Spec (시전자 스탯은 여기서 캡처된다) → 대상에게 (대상 스탯은 붙을 때 캡처된다)
	const FGameplayEffectSpecHandle SpecHandle = CasterASC->MakeOutgoingSpec(EffectClass, 1.f, CasterASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, Duration);

	LOA_LOG(this, Display, TEXT("[Cheat] %s %.1fs : %s → %s"),
		What, Duration, *GetNameSafe(CasterASC->GetAvatarActor()), *GetNameSafe(TargetASC->GetAvatarActor()));
	CasterASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

void ULOAPlayerCheats::SetBaseStat(const FString& Target, const FGameplayAttribute& Attribute, float Value, const TCHAR* What) const
{
	for (APawn* Player : ResolvePlayers(Target))
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
		if (!ASC || !ASC->HasAttributeSetForAttribute(Attribute))
		{
			LOA_LOG(this, Warning, TEXT("[Cheat] %s: %s has no %s."), What, *GetNameSafe(Player), *Attribute.GetName());
			continue;
		}
		// 테스트용으로 GE 없이 기본값을 바로 바꾼다 (ULOACheatManager의 체력 치트와 같은 방식)
		ASC->SetNumericAttributeBase(Attribute, Value);
		LOA_LOG(this, Display, TEXT("[Cheat] %s %s base = %.3f (now %.3f)"),
			*GetNameSafe(Player), What, ASC->GetNumericAttributeBase(Attribute), ASC->GetNumericAttribute(Attribute));
	}
}

// =============================================================================
// 플레이어 테스터 창 (이 컴퓨터만)
// =============================================================================

void ULOAPlayerCheats::RefreshPlayerTesterLists()
{
	PlayerTokens.Reset();
	PlayerLabels.Reset();

	PlayerTokens.Add(TEXT("All"));
	PlayerLabels.Add(TEXT("모두"));

	const ULOACheatManager* CheatManager = GetLOACheatManager();
	if (!CheatManager)
	{
		return;
	}

	const APlayerController* PC = GetPlayerController();
	const APawn* MyPawn = PC ? PC->GetPawn() : nullptr;

	// 클라도 서버와 같은 순서(PlayerId)로 센다 → 여기서 고른 P2 = 서버의 P2
	const TArray<APawn*> Players = CheatManager->GetPlayersInJoinOrder();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const APawn* Player = Players[Index];
		const FString ClassName = ULOACheatManager::GetPlayerClassName(Player);
		if (Player == MyPawn)
		{
			PlayerTokens.Add(TEXT("Me"));
			PlayerLabels.Add(FString::Printf(TEXT("나 (P%d · %s)"), Index + 1, *ClassName));
		}
		else
		{
			PlayerTokens.Add(FString::Printf(TEXT("P%d"), Index + 1));
			PlayerLabels.Add(FString::Printf(TEXT("P%d (%s)"), Index + 1, *ClassName));
		}
	}
}

TArray<FString> ULOAPlayerCheats::GetPlayerTesterLabels() const
{
	return PlayerLabels;
}

void ULOAPlayerCheats::SetTesterAutoGuard(int32 PlayerIndex, float Delay, bool bOn)
{
	if (!PlayerTokens.IsValidIndex(PlayerIndex))
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] Player index %d is out of range. Call RefreshPlayerTesterLists first."), PlayerIndex);
		return;
	}
	LOAAutoGuard(PlayerTokens[PlayerIndex], bOn ? FString::SanitizeFloat(FMath::Max(0.f, Delay)) : FString(TEXT("Off")));
}

// =============================================================================
// 자동 저가 (이 컴퓨터)
// =============================================================================

void ULOAPlayerCheats::BindAutoGuard()
{
	const APlayerController* PC = GetPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	ULOAJustGuardComponent* JustGuardComp = Pawn ? Pawn->FindComponentByClass<ULOAJustGuardComponent>() : nullptr;

	// 이미 이 컴포넌트에 붙어 있으면 그대로 (초만 바뀐다)
	if (JustGuardComp && BoundJustGuardComp.Get() == JustGuardComp)
	{
		return;
	}

	UnbindAutoGuard();
	if (!JustGuardComp)
	{
		// 캐릭터가 아직 없으면(부활 대기) 빙의 변경 알림 때 다시 붙는다
		LOA_LOG(this, Warning, TEXT("[Cheat] auto guard: %s has no ULOAJustGuardComponent. Will try again when the character changes."), *GetNameSafe(Pawn));
		return;
	}

	BoundJustGuardComp = JustGuardComp;
	WindowOpenedHandle = JustGuardComp->OnWindowOpened.AddUObject(this, &ThisClass::OnWindowOpened);
	WindowClosedHandle = JustGuardComp->OnWindowClosed.AddUObject(this, &ThisClass::OnWindowClosed);
}

void ULOAPlayerCheats::UnbindAutoGuard()
{
	if (ULOAJustGuardComponent* JustGuardComp = BoundJustGuardComp.Get())
	{
		JustGuardComp->OnWindowOpened.Remove(WindowOpenedHandle);
		JustGuardComp->OnWindowClosed.Remove(WindowClosedHandle);
	}
	BoundJustGuardComp.Reset();
	WindowOpenedHandle.Reset();
	WindowClosedHandle.Reset();

	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : PendingTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}
	PendingTimers.Reset();
	HeldWindowId = 0;
}

void ULOAPlayerCheats::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (AutoGuardDelay < 0.f)
	{
		return;
	}
	UnbindAutoGuard();
	BindAutoGuard();
}

void ULOAPlayerCheats::OnWindowOpened(int32 WindowId)
{
	UWorld* World = GetWorld();
	if (AutoGuardDelay < 0.f || !World)
	{
		return;
	}

	// 끝난 타이머는 목록에서 뺀다 (목록이 계속 길어지지 않게)
	PendingTimers.RemoveAll([World](const FTimerHandle& Timer) { return !World->GetTimerManager().TimerExists(Timer); });

	const FTimerDelegate PressDelegate = FTimerDelegate::CreateWeakLambda(this, [this, WindowId]() { PressGuard(WindowId); });
	FTimerHandle& Timer = PendingTimers.AddDefaulted_GetRef();
	if (AutoGuardDelay <= 0.f)
	{
		// 0초 = 다음 프레임 (구간 알림을 처리하는 도중에 누르지 않게)
		Timer = World->GetTimerManager().SetTimerForNextTick(PressDelegate);
	}
	else
	{
		World->GetTimerManager().SetTimer(Timer, PressDelegate, AutoGuardDelay, false);
	}
}

void ULOAPlayerCheats::OnWindowClosed(int32 WindowId)
{
	if (HeldWindowId != 0 && WindowId == HeldWindowId)
	{
		ReleaseGuard();
	}
}

void ULOAPlayerCheats::PressGuard(int32 WindowId)
{
	ULOAAbilitySystemComponent* ASC = LOAPlayerCheat::GetMyASC(GetPlayerController());
	const FGameplayTag InputTag = FindJustGuardInputTag();
	if (!ASC || !InputTag.IsValid())
	{
		LOA_LOG(this, Warning, TEXT("[Cheat] auto guard: no just guard ability with an InputTag on my character."));
		return;
	}

	// 다른 구간을 아직 누르고 있으면 먼저 뗀다 (진짜 손도 한 번에 한 번만 누른다)
	if (HeldWindowId != 0)
	{
		ReleaseGuard();
	}

	const ULOAJustGuardComponent* JustGuardComp = BoundJustGuardComp.Get();
	const FLOAJustGuardRecord* Record = JustGuardComp ? JustGuardComp->FindRecord(WindowId) : nullptr;
	const ELOAJustGuardMode Mode = Record ? Record->Mode : ELOAJustGuardMode::Tap;
	const bool bHold = (Mode == ELOAJustGuardMode::Hold || Mode == ELOAJustGuardMode::HoldFromMiddle);

	LOA_LOG(this, Display, TEXT("[Cheat] auto guard: press for window #%d (%s)"), WindowId, bHold ? TEXT("hold") : TEXT("tap"));
	ASC->AbilityInputTagPressed(InputTag);
	HeldWindowId = WindowId;

	// 홀딩 = 구간 끝(OnWindowClosed)에 뗌 / 단타 = 잠깐 뒤 뗌
	if (!bHold)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle& Timer = PendingTimers.AddDefaulted_GetRef();
			// 그사이 다른 구간을 눌렀으면 그 누름은 건드리지 않는다
			World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, WindowId]()
			{
				if (HeldWindowId == WindowId)
				{
					ReleaseGuard();
				}
			}), AutoGuardTapTime, false);
		}
	}
}

void ULOAPlayerCheats::ReleaseGuard()
{
	if (HeldWindowId == 0)
	{
		return;
	}
	HeldWindowId = 0;

	ULOAAbilitySystemComponent* ASC = LOAPlayerCheat::GetMyASC(GetPlayerController());
	const FGameplayTag InputTag = FindJustGuardInputTag();
	if (ASC && InputTag.IsValid())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

FGameplayTag ULOAPlayerCheats::FindJustGuardInputTag() const
{
	const ULOAAbilitySystemComponent* ASC = LOAPlayerCheat::GetMyASC(GetPlayerController());
	if (!ASC)
	{
		return FGameplayTag();
	}

	// 꾸러미가 스킬을 줄 때 붙인 InputTag.* (ULOAAbilitySystemComponent::AbilityInputTagPressed가 이 태그로 스킬을 찾는다)
	const FGameplayTag InputTagRoot = FGameplayTag::RequestGameplayTag(TEXT("InputTag"), false);
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->IsA<ULOAGA_JustGuard>())
		{
			continue;
		}
		for (const FGameplayTag& Tag : Spec.GetDynamicSpecSourceTags())
		{
			if (!InputTagRoot.IsValid() || Tag.MatchesTag(InputTagRoot))
			{
				return Tag;
			}
		}
	}
	return FGameplayTag();
}
