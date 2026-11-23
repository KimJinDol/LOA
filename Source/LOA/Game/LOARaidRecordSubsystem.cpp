// LOARaidRecordSubsystem.cpp

#include "Game/LOARaidRecordSubsystem.h"
#include "Game/LOARaidSubsystem.h"
#include "Combat/LOACombatNoticeSubsystem.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Character/Player/LOACharacter.h"
#include "Library/LOACombatLibrary.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "AbilitySystem/Executions/LOADamageExecution.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARaidRecordSubsystem)

ULOARaidRecordSubsystem* ULOARaidRecordSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOARaidRecordSubsystem>() : nullptr;
}

bool ULOARaidRecordSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOARaidRecordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 레이드 상황판이 먼저 만들어지게 한다 → 보스가 바뀔 때 카운터 이벤트를 새 보스에 건다
	if (ULOARaidSubsystem* Raid = Collection.InitializeDependency<ULOARaidSubsystem>())
	{
		Raid->OnBossChanged.AddDynamic(this, &ThisClass::HandleBossChanged);
	}

	// 전투 알림판을 듣는다 (피해·보호막·회복). 알림은 서버에서만 온다
	if (ULOACombatNoticeSubsystem* CombatNotice = Collection.InitializeDependency<ULOACombatNoticeSubsystem>())
	{
		DamageAppliedHandle = CombatNotice->OnDamageApplied.AddUObject(this, &ThisClass::HandleDamageApplied);
		ShieldAbsorbedHandle = CombatNotice->OnShieldAbsorbed.AddUObject(this, &ThisClass::HandleShieldAbsorbed);
		HealAppliedHandle = CombatNotice->OnHealApplied.AddUObject(this, &ThisClass::HandleHealApplied);
	}
}

void ULOARaidRecordSubsystem::Deinitialize()
{
	UnbindCounterEvent();

	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->OnBossChanged.RemoveDynamic(this, &ThisClass::HandleBossChanged);
	}

	if (ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(this))
	{
		CombatNotice->OnDamageApplied.Remove(DamageAppliedHandle);
		CombatNotice->OnShieldAbsorbed.Remove(ShieldAbsorbedHandle);
		CombatNotice->OnHealApplied.Remove(HealAppliedHandle);
	}

	Super::Deinitialize();
}

// =============================================================================
// 기록 시작 / 멈춤 [서버]
// =============================================================================

void ULOARaidRecordSubsystem::BeginRecording()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	Records.Reset();
	PartyDamageTaken = 0.f;
	bRecording = true;
	LOA_LOG(World, Log, TEXT("[RaidRecord] Recording started."));
}

void ULOARaidRecordSubsystem::StopRecording()
{
	if (bRecording)
	{
		bRecording = false;
		LOA_LOG(GetWorld(), Log, TEXT("[RaidRecord] Recording stopped."));
	}
}

// =============================================================================
// 전투 알림판 듣기 [서버]
// =============================================================================

const APlayerState* ULOARaidRecordSubsystem::ResolvePlayerState(const AActor* Actor)
{
	if (const APlayerState* PlayerState = Cast<APlayerState>(Actor))
	{
		return PlayerState;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn->GetPlayerState();
	}
	if (const AController* Controller = Cast<AController>(Actor))
	{
		return Controller->PlayerState;
	}
	return nullptr;
}

ULOARaidRecordSubsystem::FPlayerRecord* ULOARaidRecordSubsystem::FindOrAddRecord(const AActor* Instigator)
{
	if (!bRecording)
	{
		return nullptr;
	}

	const APlayerState* PlayerState = ResolvePlayerState(Instigator);
	return PlayerState ? &Records.FindOrAdd(PlayerState) : nullptr;
}

void ULOARaidRecordSubsystem::HandleDamageApplied(const FLOADamageAppliedInfo& Info)
{
	const float Amount = Info.RemovedAmount;
	if (!bRecording || Amount <= 0.f || !Info.Target)
	{
		return;
	}

	// 플레이어가 맞음 → 파티가 맞은 양 (방어의 분모)
	if (ULOACombatLibrary::IsPlayerSide(Info.Target))
	{
		PartyDamageTaken += Amount;
		return;
	}

	// 적이 맞음 → 때린 사람의 준 피해
	const APlayerState* AttackerState = ResolvePlayerState(Info.Instigator);
	if (!AttackerState)
	{
		return;	// 플레이어가 아닌 쪽이 적을 때림 (지금은 없음)
	}
	Records.FindOrAdd(AttackerState).Damage += Amount;

	// 버프 때문에 더 들어간 몫 → 버프 건 사람의 조력
	RecordAssist(Info, AttackerState);
}

void ULOARaidRecordSubsystem::RecordAssist(const FLOADamageAppliedInfo& Info, const APlayerState* AttackerState)
{
	// 조력 = 버프 때문에 더 들어간 피해를 버프 건 사람에게 (사용자 결정 2026-11-15, 계산은 전부 이 함수 하나)
	//   예) 버서커가 보스를 깎은 양 200. 걸린 버프: 공증 +0.15(세라스A, 버서커에게) · 낙인 +0.1(세라스B, 보스에게)
	//       ① 버프 GE 모으기 → [세라스A 0.15 (주는 쪽), 세라스B 0.1 (받는 쪽)]
	//       ② 버프가 없었으면 = 200 / (1.15 × 1.1) = 158.1 → 늘어난 몫 41.9
	//       ③ 크기 비율로 나눔 → 세라스A 41.9 × 0.15/0.25 = 25.1, 세라스B 16.8
	//   공식 = ULOADamageExecution::CalcBuffMultiplier (데미지 계산과 같은 함수, apply119)
	//   시점: 데미지 계산이 버프를 맞는 순간 값으로 읽는다(Snapshot = false, apply119) → 지금 켜진 GE = 데미지 계산이 본 GE
	//         도트 틱 · 투사체도 같다 (전에는 때린 쪽이 Spec을 만든 순간 값이라 틱 사이에 붙은 공증에 없는 조력이 갔다)
	//   버프 GE = 켜져 있고(inhibited 아님) 그 칸을 더하기로 올리는(양수) Modifier를 가진 GE (CollectAssistBuffs)
	//   자기 자신이 건 버프는 세지 않는다 (그 몫은 이미 내 준 피해에 들어 있음)

	// ① 버프 GE 모으기: 때린 쪽의 주는 피해 증가 + 맞은 쪽의 받는 피해 증가
	TArray<FAssistBuff> Buffs;
	const float OutgoingBuffSum = CollectAssistBuffs(Info.InstigatorASC, ULOAAttributeSet_Combat::GetOutgoingDamageBonusAttribute(), Buffs);
	const float IncomingBuffSum = CollectAssistBuffs(Info.TargetASC, ULOAAttributeSet_Combat::GetIncomingDamageBonusAttribute(), Buffs);

	const float TotalBuff = OutgoingBuffSum + IncomingBuffSum;
	if (TotalBuff <= 0.f)
	{
		return;
	}

	// ② 버프가 있을 때 / 없을 때의 배율 (데미지 계산과 같은 함수)
	const float OutgoingNow = Info.InstigatorASC ? Info.InstigatorASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetOutgoingDamageBonusAttribute()) : 0.f;
	const float IncomingNow = Info.TargetASC ? Info.TargetASC->GetNumericAttribute(ULOAAttributeSet_Combat::GetIncomingDamageBonusAttribute()) : 0.f;
	const float WithBuff = ULOADamageExecution::CalcBuffMultiplier(OutgoingNow, IncomingNow);
	const float WithoutBuff = ULOADamageExecution::CalcBuffMultiplier(OutgoingNow - OutgoingBuffSum, IncomingNow - IncomingBuffSum);
	if (WithBuff <= 0.f)
	{
		return;
	}
	const float Extra = Info.RemovedAmount * (1.f - WithoutBuff / WithBuff);

	// ③ 크기 비율로 나눔. 자기 버프는 그 몫을 버린다
	for (const FAssistBuff& Buff : Buffs)
	{
		if (Buff.Giver && Buff.Giver != AttackerState)
		{
			Records.FindOrAdd(Buff.Giver).Assist += Extra * (Buff.Bonus / TotalBuff);
		}
	}
}

float ULOARaidRecordSubsystem::CollectAssistBuffs(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, TArray<FAssistBuff>& OutBuffs)
{
	if (!ASC)
	{
		return 0.f;
	}

	float Sum = 0.f;
	FGameplayEffectQuery Query;
	Query.ModifyingAttribute = Attribute;	// 그 칸을 바꾸는 GE만
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		// 꺼진 GE(Ongoing Tag Requirements 등으로 inhibited)는 데미지 계산에 안 들어간다 → 조력도 아님 (apply115 리뷰 지적 1)
		if (!Effect || !Effect->Spec.Def || Effect->bIsInhibited)
		{
			continue;
		}

		float Bonus = 0.f;
		const TArray<FGameplayModifierInfo>& Modifiers = Effect->Spec.Def->Modifiers;
		for (int32 Index = 0; Index < Modifiers.Num(); ++Index)
		{
			// 더하기만: 역산 공식(1 + 합)은 더하기를 가정한다. 곱하기 버프가 생기면 공식과 같이 다시 본다
			if (Modifiers[Index].Attribute == Attribute && Modifiers[Index].ModifierOp == EGameplayModOp::Additive)
			{
				Bonus += Effect->Spec.GetModifierMagnitude(Index, true);	// 중첩 수까지 곱한 양
			}
		}

		if (Bonus > 0.f)
		{
			OutBuffs.Add({ ResolvePlayerState(Effect->Spec.GetContext().GetInstigator()), Bonus });
			Sum += Bonus;
		}
	}
	return Sum;
}

void ULOARaidRecordSubsystem::HandleShieldAbsorbed(const FLOAShieldAbsorbedInfo& Info)
{
	if (Info.Amount <= 0.f)
	{
		return;
	}

	if (FPlayerRecord* Record = FindOrAddRecord(Info.ShieldGiver))
	{
		Record->Protect += Info.Amount;
	}
}

void ULOARaidRecordSubsystem::HandleHealApplied(const FLOAHealAppliedInfo& Info)
{
	if (Info.HealedAmount <= 0.f)
	{
		return;
	}

	if (FPlayerRecord* Record = FindOrAddRecord(Info.Healer))
	{
		Record->Heal += Info.HealedAmount;
	}
}

// =============================================================================
// 카운터 [서버]
// =============================================================================

void ULOARaidRecordSubsystem::HandleBossChanged(ALOABossCharacter* NewBoss)
{
	UnbindCounterEvent();

	// 카운터 판정은 서버에서만 일어난다 (ULOASkillModule_Counter) → 서버만 듣는다
	const UWorld* World = GetWorld();
	if (!NewBoss || !World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	UAbilitySystemComponent* BossASC = NewBoss->GetAbilitySystemComponent();
	if (!BossASC)
	{
		return;
	}

	CounterEventASC = BossASC;
	CounterEventHandle = BossASC->GenericGameplayEventCallbacks.FindOrAdd(LOAGameplayTags::Event_Combat_Counter)
		.AddUObject(this, &ThisClass::HandleCounterEvent);
}

void ULOARaidRecordSubsystem::UnbindCounterEvent()
{
	if (UAbilitySystemComponent* ASC = CounterEventASC.Get())
	{
		if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(LOAGameplayTags::Event_Combat_Counter))
		{
			Delegate->Remove(CounterEventHandle);
		}
	}
	CounterEventASC.Reset();
	CounterEventHandle.Reset();
}

void ULOARaidRecordSubsystem::HandleCounterEvent(const FGameplayEventData* EventData)
{
	// Instigator = 카운터를 친 사람 (ULOASkillModule_Counter가 "맞았다" 신호의 때린 사람을 그대로 넣음)
	if (FPlayerRecord* Record = FindOrAddRecord(EventData ? EventData->Instigator.Get() : nullptr))
	{
		++Record->Counters;
	}
}

// =============================================================================
// 결과 [서버]
// =============================================================================

FLOARaidClearResult ULOARaidRecordSubsystem::BuildClearResult(const TArray<TObjectPtr<APlayerState>>& Players) const
{
	FLOARaidClearResult Result;

	// ① 파티 합계 (결과에 들어가는 사람만)
	float TotalDamage = 0.f;
	int32 TotalCounters = 0;
	float TotalHeal = 0.f;
	for (const APlayerState* PlayerState : Players)
	{
		if (const FPlayerRecord* Record = Records.Find(PlayerState))
		{
			TotalDamage += Record->Damage;
			TotalCounters += Record->Counters;
			TotalHeal += Record->Heal;
		}
	}

	// 분모가 0이면 그 칸은 모두 0
	auto Share = [](float Mine, float Total) { return Total > 0.f ? FMath::Clamp(Mine / Total, 0.f, 1.f) : 0.f; };

	// ② 사람마다 몫 · 점수
	for (const APlayerState* PlayerState : Players)
	{
		if (!PlayerState)
		{
			continue;
		}

		const FPlayerRecord Record = Records.FindRef(PlayerState);

		FLOARaidPlayerResult& Entry = Result.Players.AddDefaulted_GetRef();
		Entry.PlayerName = PlayerState->GetPlayerName();
		if (const ALOACharacter* Character = PlayerState->GetPawn<ALOACharacter>())
		{
			Entry.ClassData = Character->GetCharacterClassData();
		}

		Entry.Shares.SetNumZeroed(static_cast<int32>(ELOARaidStat::Count));
		Entry.Shares[static_cast<int32>(ELOARaidStat::Damage)]  = Share(Record.Damage, TotalDamage);
		Entry.Shares[static_cast<int32>(ELOARaidStat::Counter)] = Share(static_cast<float>(Record.Counters), static_cast<float>(TotalCounters));
		Entry.Shares[static_cast<int32>(ELOARaidStat::Assist)]  = Share(Record.Assist, TotalDamage);
		Entry.Shares[static_cast<int32>(ELOARaidStat::Protect)] = Share(Record.Protect, PartyDamageTaken);
		Entry.Shares[static_cast<int32>(ELOARaidStat::Heal)]    = Share(Record.Heal, TotalHeal);

		for (const float Value : Entry.Shares)
		{
			Entry.Score += Value;
		}

		LOA_LOG(PlayerState, Log, TEXT("[RaidRecord] %s: damage %.0f, counters %d, assist %.0f, protect %.0f, heal %.0f -> score %.2f"),
			*Entry.PlayerName, Record.Damage, Record.Counters, Record.Assist, Record.Protect, Record.Heal, Entry.Score);
	}

	// ③ MVP = 점수가 가장 큰 사람. 같으면 준 피해가 큰 사람 (대신 정함)
	for (int32 Index = 0; Index < Result.Players.Num(); ++Index)
	{
		if (Result.MvpIndex == INDEX_NONE)
		{
			Result.MvpIndex = Index;
			continue;
		}

		const FLOARaidPlayerResult& Best = Result.Players[Result.MvpIndex];
		const FLOARaidPlayerResult& Candidate = Result.Players[Index];
		if (Candidate.Score > Best.Score
			|| (FMath::IsNearlyEqual(Candidate.Score, Best.Score) && Candidate.GetShare(ELOARaidStat::Damage) > Best.GetShare(ELOARaidStat::Damage)))
		{
			Result.MvpIndex = Index;
		}
	}

	LOA_LOG(GetWorld(), Log, TEXT("[RaidRecord] Party totals: damage %.0f, counters %d, heal %.0f, taken %.0f. MVP index %d"),
		TotalDamage, TotalCounters, TotalHeal, PartyDamageTaken, Result.MvpIndex);

	return Result;
}
