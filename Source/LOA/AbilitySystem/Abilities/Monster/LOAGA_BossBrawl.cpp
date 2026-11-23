// LOAGA_BossBrawl.cpp

#include "AbilitySystem/Abilities/Monster/LOAGA_BossBrawl.h"
#include "AbilitySystem/Abilities/Monster/LOAMonsterAbility.h"
#include "Actors/LOABrawlArena.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Game/LOARaidSubsystem.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_BossBrawl)

ULOAGA_BossBrawl::ULOAGA_BossBrawl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 보스는 주인 컴퓨터가 없다 → 서버만
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 발동 조건: 무력화가 처음 0이 됨 (보스 스탯창이 보내는 이벤트)
	FAbilityTriggerData StaggerTrigger;
	StaggerTrigger.TriggerTag = LOAGameplayTags::Event_Trigger_StaggerBroken;
	StaggerTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(StaggerTrigger);

	// 스턴과 같은 성질: 아이덴티티처럼 남아야 하는 Ability는 끊지 않는다
	AbilityTagsToKeep.AddTag(LOAGameplayTags::Ability_Trait_KeepOnCC);

	// 켜져 있는 동안: 그로기(StateTree가 봄 → 다른 패턴 안 나감) + 이동 금지
	// State.Block.Ability는 붙이지 않는다: 협동 저가 패턴까지 막힌다 (헤더 [흐름], apply128)
	ActivationOwnedTags.AddTag(LOAGameplayTags::State_Body_Groggy);
	ActivationOwnedTags.AddTag(LOAGameplayTags::State_Block_Move);
}

void ULOAGA_BossBrawl::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility는 부르지 않는다 (스턴·사망과 같음. 흐름은 여기 한 곳에서만)

	ALOABossCharacter* Boss = Cast<ALOABossCharacter>(GetAvatarActorFromActorInfo());
	LOA_LOG(Boss, Log, TEXT("[Brawl] Groggy (stagger broken by %s)"),
		TriggerEventData ? *GetNameSafe(TriggerEventData->Instigator.Get()) : TEXT("?"));

	// ① 하던 패턴 끊기 ② 그 자리에 서기 ③ 그로기 몽타주
	CancelOtherAbilities(AbilityTagsToKeep);
	StopAvatarMovement();
	PlayMontageOnly();

	// ④ 경기장에 시작 요청. 없거나 거부하면 바로 끝 (EndAbility에서 무력화를 다시 채움)
	ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(Boss);
	ALOABrawlArena* Arena = Raid ? Raid->GetBrawlArena() : nullptr;
	if (!Arena)
	{
		LOA_LOG(Boss, Warning, TEXT("[Brawl] No brawl arena in the level. Ending groggy right away."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 끝 알림을 먼저 건다 (시작 요청 안에서 바로 끝나는 일은 없지만 순서를 지킨다)
	ActiveArena = Arena;
	BrawlEndedHandle = Arena->OnBrawlEnded.AddUObject(this, &ThisClass::HandleBrawlEnded);
	StageAdvancedHandle = Arena->OnStageAdvanced.AddUObject(this, &ThisClass::HandleStageAdvanced);

	// 협동 저가 패턴: 성공 인원(Just Guard 모듈 이벤트)과 패턴 끝(보스 ASC)을 듣는다 (apply128)
	UAbilityTask_WaitGameplayEvent* ResolvedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, LOAGameplayTags::Event_JustGuard_PatternResolved, nullptr, /*OnlyTriggerOnce*/ false, /*OnlyMatchExact*/ true);
	ResolvedTask->EventReceived.AddDynamic(this, &ThisClass::HandleJustGuardResolved);
	ResolvedTask->ReadyForActivation();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ListenedASC = ASC;
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	}

	if (!Arena->StartBrawl(Boss))
	{
		LOA_LOG(Boss, Warning, TEXT("[Brawl] Arena refused to start (already in a brawl?). Ending groggy right away."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void ULOAGA_BossBrawl::HandleBrawlEnded(ALOABrawlArena* Arena)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULOAGA_BossBrawl::HandleStageAdvanced(ALOABrawlArena* Arena, int32 NewStage)
{
	AActor* Boss = GetAvatarActorFromActorInfo();

	// 패턴 중에는 단계 게이지가 멈춰 있어 여기 오지 않는다 (apply127). 혹시 오면 앞 패턴을 기다린다
	if (bWaitingCoopJustGuard)
	{
		LOA_LOG(Boss, Warning, TEXT("[Brawl] Stage %d advanced while the co-op just guard pattern is still running. Ignored."), NewStage);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!CoopJustGuardPatternTag.IsValid() || !ASC)
	{
		LOA_LOG(Boss, Warning, TEXT("[Brawl] Stage %d: Coop Just Guard Pattern Tag is empty. Resuming the timer without a bonus."), NewStage);
		if (Arena)
		{
			Arena->ResumeBrawlTimer();
		}
		return;
	}

	// 먼저 기다림 표시 (패턴이 발동 안에서 바로 끝나도 HandleAbilityEnded가 받게)
	bWaitingCoopJustGuard = true;
	CoopJustGuardSuccessCount = 0;

	LOA_LOG(Boss, Log, TEXT("[Brawl] Stage %d: co-op just guard pattern %s"), NewStage, *CoopJustGuardPatternTag.ToString());

	// 대상 없음 → 보는 쪽(통로 아래 = 플레이어 쪽) 그대로
	if (!ULOAMonsterAbility::TryActivatePattern(ASC, CoopJustGuardPatternTag, nullptr))
	{
		bWaitingCoopJustGuard = false;
		LOA_LOG(Boss, Warning, TEXT("[Brawl] Co-op just guard pattern %s was rejected. Resuming the timer without a bonus."),
			*CoopJustGuardPatternTag.ToString());
		if (Arena)
		{
			Arena->ResumeBrawlTimer();
		}
	}
}

void ULOAGA_BossBrawl::HandleJustGuardResolved(FGameplayEventData Payload)
{
	// 패턴 끝 알림이 Ability 끝보다 먼저 온다 (모듈 종료가 엔진 종료보다 먼저). 여기선 받아 두기만
	if (bWaitingCoopJustGuard)
	{
		CoopJustGuardSuccessCount = FMath::RoundToInt(Payload.EventMagnitude);
	}
}

void ULOAGA_BossBrawl::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (!bWaitingCoopJustGuard || !IsCoopJustGuardPattern(EndedData.AbilityThatEnded))
	{
		return;
	}
	bWaitingCoopJustGuard = false;

	// 등급 → 추가 시간 → 다시 흐름 (경기장이 진행 중이 아니면 무시)
	if (ALOABrawlArena* Arena = ActiveArena.Get())
	{
		Arena->FinishCoopJustGuard(CoopJustGuardSuccessCount);
	}

	// 다시 그로기 자세
	if (IsActive())
	{
		PlayMontageOnly();
	}
}

bool ULOAGA_BossBrawl::IsCoopJustGuardPattern(const UGameplayAbility* Ability) const
{
	return Ability && CoopJustGuardPatternTag.IsValid() && Ability->GetAssetTags().HasTagExact(CoopJustGuardPatternTag);
}

void ULOAGA_BossBrawl::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 협동 저가 패턴 구독 해제. 아직 도는 중이면 (대난투가 끝나 돌아옴) 그 패턴도 끝낸다
	if (UAbilitySystemComponent* ASC = ListenedASC.Get())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
		if (bWaitingCoopJustGuard && CoopJustGuardPatternTag.IsValid())
		{
			const FGameplayTagContainer PatternTags(CoopJustGuardPatternTag);
			ASC->CancelAbilities(&PatternTags);
		}
	}
	ListenedASC.Reset();
	AbilityEndedHandle.Reset();
	bWaitingCoopJustGuard = false;
	CoopJustGuardSuccessCount = 0;

	if (ALOABrawlArena* Arena = ActiveArena.Get())
	{
		Arena->OnBrawlEnded.Remove(BrawlEndedHandle);
		Arena->OnStageAdvanced.Remove(StageAdvancedHandle);

		// 대난투 중에 끊김 (사망 등) → 원래 자리로 돌아가게 (진행 중이 아니면 경기장이 무시)
		if (bWasCancelled)
		{
			Arena->EndBrawl();
		}
	}
	ActiveArena.Reset();
	BrawlEndedHandle.Reset();
	StageAdvancedHandle.Reset();

	// 무력화 다시 채우기 → 다시 다 깎으면 또 대난투 (사용자 11-18). 죽은 보스는 상관없음
	if (ALOABossCharacter* Boss = Cast<ALOABossCharacter>(GetAvatarActorFromActorInfo()))
	{
		Boss->RefillStagger();
	}

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Brawl] Groggy ended%s"), bWasCancelled ? TEXT(" (cancelled)") : TEXT(""));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
