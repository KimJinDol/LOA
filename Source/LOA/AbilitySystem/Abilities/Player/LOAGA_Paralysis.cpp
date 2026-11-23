// LOAGA_Paralysis.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_Paralysis.h"
#include "Combat/LOACrowdControlPredictionComponent.h"
#include "AbilitySystem/Effects/LOAGE_Paralysis.h"
#include "Character/Player/LOACharacterMovementComponent.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Paralysis)

ULOAGA_Paralysis::ULOAGA_Paralysis(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버가 발동하고, 맞은 사람 컴퓨터에서도 같이 돈다 (그쪽은 몽타주만). 헤더 [네트워크]
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 발동 조건: 경직 태그가 붙는 순간 (0 → 1). 경직 중 또 맞아 개수만 늘 때는 발동하지 않는다 → 그건 OnEffectAppliedToSelf가
	FAbilityTriggerData ParalysisTrigger;
	ParalysisTrigger.TriggerTag = LOAGameplayTags::State_CrowdControl_Paralysis;
	ParalysisTrigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(ParalysisTrigger);

	// 취소하지 않을 성질 (2026-11-09): 아이덴티티처럼 CC를 맞아도 켜져 있어야 하는 Ability
	AbilityTagsToKeep.AddTag(LOAGameplayTags::Ability_Trait_KeepOnCC);

	SetAssetTags(FGameplayTagContainer(LOAGameplayTags::Ability_Common_Paralysis));

	// 죽어 있으면 발동 불가 (부모 생성자의 State.Body.Dead 그대로)
}

// =============================================================================
// 발동 / 종료
// =============================================================================

void ULOAGA_Paralysis::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility는 부르지 않는다. (띄움·스턴·사망과 같음. 흐름은 여기 한 곳에서만)

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ① 하던 일 멈추기 [둘 다] (B 컴퓨터도 끊어야 내 화면에서 바로 끊긴다)
	CancelOtherAbilities(AbilityTagsToKeep);

	// [B 컴퓨터] 피해자 판정에서 내가 먼저 경직을 예측했으면 이어받는다 (예측 몽타주가 재생 중이면 그 위치부터)
	//   ULOACrowdControlPredictionComponent
	float ResumeTime = -1.f;
	if (!IsServerSide())
	{
		if (ULOACrowdControlPredictionComponent* PredictionComp = GetAvatarActorFromActorInfo()->FindComponentByClass<ULOACrowdControlPredictionComponent>())
		{
			ResumeTime = PredictionComp->ConfirmPrediction(GetMontage());
		}
	}

	// ③ 몽타주 [둘 다]. 서버 → 남의 화면, B 컴퓨터 → 내 화면. 끝나도 Ability는 계속 (경직 시간이 흐름을 정한다)
	PlayParalysisMontage(FMath::Max(0.f, ResumeTime));

	if (!IsServerSide())
	{
		// [B 컴퓨터만] "또 맞음"을 복제 도착으로 알아챈다 → 몽타주 처음부터. 나머지(시간·밀기·끝내기)는 서버가 한다
		bOwnerSawFirstEffect = (FindNewestParalysisEffect() != nullptr);
		EffectAddedHandleOnOwner = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::OnEffectAddedOnOwner);
		return;
	}

	// 여기부터 [서버만]
	// 이동 금지 (GE라 B 컴퓨터에도 복제 → 클릭 이동이 멈춘다)
	BlockMoveEffectHandle = ApplyStateTagsToSelf(FGameplayTagContainer(LOAGameplayTags::State_Block_Move));

	// 알림 구독: 또 맞음 / 경직 태그가 다 빠짐(안전 시간 등)
	EffectAppliedHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ThisClass::OnEffectAppliedToSelf);
	ParalysisTagHandle = ASC->RegisterGameplayTagEvent(
		LOAGameplayTags::State_CrowdControl_Paralysis, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &ThisClass::OnParalysisTagChanged);

	// ② 처음 경직: 방금 붙은 경직 GE (태그가 붙는 순간 발동했으므로 이미 목록에 있다)
	if (const FActiveGameplayEffect* Newest = FindNewestParalysisEffect())
	{
		StartParalysis(Newest->Spec, Newest->Handle, /*bRestart*/ false);
	}
	else
	{
		// 경직 태그를 ULOAGE_Paralysis가 아닌 GE로 붙였다 → 숫자·방향을 모름. 밀지 않고, 태그가 빠질 때 끝난다
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[Paralysis] Tag came from a non-LOAGE_Paralysis effect. No push, ends when the tag is removed."));
	}
}

void ULOAGA_Paralysis::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

		// 알림 해제를 GE 지우기보다 먼저 (지우면서 울리는 태그 알림을 받지 않게)
		if (ASC)
		{
			ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(EffectAppliedHandle);
			ASC->RegisterGameplayTagEvent(LOAGameplayTags::State_CrowdControl_Paralysis, EGameplayTagEventType::AnyCountChange)
				.Remove(ParalysisTagHandle);
		}
		EffectAppliedHandle.Reset();
		ParalysisTagHandle.Reset();

		// [B 컴퓨터] 도착 알림 해제
		if (ASC)
		{
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(EffectAddedHandleOnOwner);
		}

		// [B 컴퓨터] 예측으로 붙인 이동 금지를 푼다 (진짜 이동 금지 GE도 이 GA와 같이 끝난다)
		const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		if (ULOACrowdControlPredictionComponent* PredictionComp = Avatar ? Avatar->FindComponentByClass<ULOACrowdControlPredictionComponent>() : nullptr;
			PredictionComp && !IsServerSide())
		{
			PredictionComp->EndPrediction();
		}
		EffectAddedHandleOnOwner.Reset();
		bOwnerSawFirstEffect = false;

		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ParalysisTimer);
		}

		// 여기부터 [서버만]: 붙인 GE 지우기, 밀림 끝내기. (B 컴퓨터는 결과를 복제로 받는다)
		//   B 컴퓨터에서 하면 안 되는 이유 예) 경직 중 띄움 → B 컴퓨터의 띄움 GA가 이 GA를 끊음 → 여기서 궤적을 지우면 방금 받은 띄움 궤적이 지워진다
		if (IsServerSide())
		{
			RemoveOwnedEffect(BlockMoveEffectHandle);

			// 밀리는 중에 끊겼으면(띄움·사망) 밀림도 지운다. 띄움이면 띄움 궤적이 이미 덮어써서 아무 일 없다
			//   (주의: 띄움이 먼저 궤적을 걸고 나서 이 Ability가 끊기면 띄움 궤적을 지우게 된다 → 띄움은 다른 Ability를 먼저 끊고 나서 발사한다)
			if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
			{
				if (ULOACharacterMovementComponent* Movement = Cast<ULOACharacterMovementComponent>(Character->GetCharacterMovement()))
				{
					Movement->StopCrowdControlMove();
				}
			}

			// 경직 GE를 모두 지운다 (정상 종료든, 띄움·사망으로 끊겼든) → 경직 태그가 빠져 회피·스킬이 다시 된다
			if (ASC)
			{
				ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Paralysis));
			}
		}

		CurrentParalysisHandle.Invalidate();

		LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Paralysis] Ended%s"), bWasCancelled ? TEXT(" (cancelled)") : TEXT(""));
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =============================================================================
// 경직 시작 / 다시 시작
// =============================================================================

void ULOAGA_Paralysis::StartParalysis(const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle EffectHandle, bool bRestart)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		return;
	}

	CurrentParalysisHandle = EffectHandle;

	// 숫자: Payload 값이 있으면 그것, 없으면 GE의 기본값.
	// Spec은 GE를 지우면 사라질 수 있어서, 옛 GE를 지우기 전에 숫자를 먼저 읽고 밀기까지 끝낸다
	const ULOAGE_Paralysis* ParalysisDef = Cast<const ULOAGE_Paralysis>(Spec.Def.Get());
	const float DefaultDuration = ParalysisDef ? ParalysisDef->DefaultDuration : 0.5f;
	const float DefaultDistance = ParalysisDef ? ParalysisDef->DefaultDistance : 0.f;

	const float Duration = FMath::Max(0.f,
		Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, /*WarnIfNotFound*/ false, DefaultDuration));
	const float Distance = FMath::Max(0.f,
		Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_LaunchDistance, /*WarnIfNotFound*/ false, DefaultDistance));

	// ② 밀기 (다시 맞으면 다시 밀림)
	PushFromEffect(Spec, Distance);

	// 옛 경직 GE를 지운다 → 태그는 새 GE 하나로. 태그 개수가 2 → 1이라 OnParalysisTagChanged는 끝내지 않는다
	if (bRestart)
	{
		const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
			FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Paralysis));
		for (const FActiveGameplayEffectHandle& OldHandle : ASC->GetActiveEffects(Query))
		{
			if (OldHandle != CurrentParalysisHandle)
			{
				ASC->RemoveActiveGameplayEffect(OldHandle);
			}
		}

		// 몽타주 처음부터: 되감기(JumpToSection)가 아니라 새로 다시 튼다 (2026-10-21 사용자 보고: 연속으로 맞으면 갱신 안 됨)
		//   되감기는 몽타주가 아직 재생 중일 때만 된다. 경직 모션(예: 0.4초)이 이미 끝난 뒤 또 맞으면 아무 일도 없었다
		PlayParalysisMontage();
	}

	// ④ 경직 시간 (다시 맞으면 처음부터: 같은 타이머를 다시 건다)
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().ClearTimer(ParalysisTimer);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	World->GetTimerManager().SetTimer(ParalysisTimer, this, &ThisClass::OnParalysisTimeFinished, Duration, /*bLoop*/ false);

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Paralysis] %s: %.2fs, push %.0fcm"),
		bRestart ? TEXT("Restart") : TEXT("Start"), Duration, Distance);
}

void ULOAGA_Paralysis::PushFromEffect(const FGameplayEffectSpec& Spec, float Distance)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	ULOACharacterMovementComponent* Movement = Character ? Cast<ULOACharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Movement || Distance <= 0.f)
	{
		return;
	}

	// 밀리는 시간은 고정 (GE의 Push Duration, 사용자 결정). 예) 100cm / 0.15초 → 일정한 속도로 정확히 100cm, 바로 멈춤
	const ULOAGE_Paralysis* ParalysisDef = Cast<const ULOAGE_Paralysis>(Spec.Def.Get());
	const float PushDuration = ParalysisDef ? ParalysisDef->PushDuration : 0.15f;

	// 방향 규칙은 띄움·경직 공용 (ULOAGE_CrowdControl)
	const FVector Direction = ULOAGE_CrowdControl::ComputePushDirection(Spec, Character);
	if (Direction.IsNearlyZero())
	{
		return;
	}

	// 정해진 궤적 (물리 아님, 높이 변화 없음). 맞은 사람 컴퓨터에도 같은 숫자로 걸린다. 때린 쪽 바라보기도 여기서
	Movement->StartCrowdControlMove(Direction, Distance, /*StartHeight*/ 0.f, /*ApexHeight*/ 0.f, PushDuration);

	LOA_LOG(Character, Log, TEXT("[Paralysis] Push: %.0fcm in %.2fs, dir (%.2f, %.2f)"), Distance, PushDuration, Direction.X, Direction.Y);
}

void ULOAGA_Paralysis::PlayParalysisMontage(float StartTimeSeconds)
{
	UAnimMontage* MontageToPlay = GetMontage();
	if (!MontageToPlay)
	{
		return;
	}

	// 이전 재생 작업은 끝낸다 (몽타주는 멈추지 않음 → 새 재생이 같은 몽타주를 처음부터 다시 튼다)
	if (UAbilityTask_PlayMontageAndWait* OldTask = MontageTask.Get())
	{
		OldTask->EndTask();
	}

	// 서버가 새로 틀면 "새로 재생" 신호가 복제돼 다른 화면에서도 처음부터 나온다
	UAbilityTask_PlayMontageAndWait* NewTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f, StartTimeSeconds);
	NewTask->ReadyForActivation();
	MontageTask = NewTask;
}

const FActiveGameplayEffect* ULOAGA_Paralysis::FindNewestParalysisEffect() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return nullptr;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Paralysis));

	const FActiveGameplayEffect* Newest = nullptr;
	for (const FActiveGameplayEffectHandle& EffectHandle : ASC->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(EffectHandle);
		if (!Effect || !Cast<const ULOAGE_Paralysis>(Effect->Spec.Def.Get()))
		{
			continue;
		}
		if (!Newest || Effect->StartWorldTime >= Newest->StartWorldTime)
		{
			Newest = Effect;
		}
	}
	return Newest;
}

// =============================================================================
// 알림
// =============================================================================

void ULOAGA_Paralysis::OnEffectAppliedToSelf(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle ActiveHandle)
{
	// 경직 GE만. 발동 순간 이미 쓴 GE의 알림(같은 번호)은 무시
	if (!Cast<const ULOAGE_Paralysis>(Spec.Def.Get()) || ActiveHandle == CurrentParalysisHandle)
	{
		return;
	}

	// 경직 중 또 경직 → 처음부터 다시 (사용자 결정)
	StartParalysis(Spec, ActiveHandle, /*bRestart*/ true);
}

void ULOAGA_Paralysis::OnParalysisTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 경직 GE가 다 빠졌는데 아직 이 Ability가 돈다 = 안전 시간이 다 됐거나, 다른 곳에서 지웠다 → 끝낸다
	if (NewCount <= 0)
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[Paralysis] Tag removed before the timer. Ending."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULOAGA_Paralysis::OnEffectAddedOnOwner(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle ActiveHandle)
{
	if (!Cast<const ULOAGE_Paralysis>(Spec.Def.Get()))
	{
		return;
	}

	// 발동 RPC가 GE 복제보다 먼저 오면, 이번 발동의 첫 GE가 여기로 온다 → 다시 틀지 않고 기억만
	if (!bOwnerSawFirstEffect)
	{
		bOwnerSawFirstEffect = true;
		return;
	}

	// 경직 중 또 경직 → 몽타주 처음부터 (서버의 StartParalysis(bRestart)와 같은 모습)
	PlayParalysisMontage();
}

bool ULOAGA_Paralysis::IsServerSide() const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info && Info->IsNetAuthority();
}

void ULOAGA_Paralysis::OnParalysisTimeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// =============================================================================
// 도우미
// =============================================================================

void ULOAGA_Paralysis::RemoveOwnedEffect(FActiveGameplayEffectHandle& InOutHandle) const
{
	if (!InOutHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveActiveGameplayEffect(InOutHandle);
	}
	InOutHandle.Invalidate();
}
