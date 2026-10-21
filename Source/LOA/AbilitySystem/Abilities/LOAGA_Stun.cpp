// LOAGA_Stun.cpp

#include "AbilitySystem/Abilities/LOAGA_Stun.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Stun)

ULOAGA_Stun::ULOAGA_Stun(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버만 실행한다. 클라이언트는 태그·몽타주 복제로 결과만 본다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 발동 조건: Event.Stun 게임플레이 이벤트 (카운터 당한 패턴이 보냄)
	FAbilityTriggerData StunTrigger;
	StunTrigger.TriggerTag = LOAGameplayTags::Event_Stun;
	StunTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(StunTrigger);

	// 스턴 중에는 다른 행동 금지. (보스 패턴, 플레이어 스킬 모두 State.Block.Ability에 막힌다)
	// 이동 금지는 플레이어 클릭 이동용 태그. 보스 이동은 StopMovement로 멈춘다.
	ActivationOwnedTags.AddTag(LOAGameplayTags::State_Block_Ability);
	ActivationOwnedTags.AddTag(LOAGameplayTags::State_Block_Move);
}

void ULOAGA_Stun::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility는 부르지 않는다. (GA_Death와 같음. 흐름은 여기 한 곳에서만 정한다)

	// 스턴 시간: 이벤트가 가져온 값, 없으면 기본값
	const float Duration = (TriggerEventData && TriggerEventData->EventMagnitude > 0.f)
		? TriggerEventData->EventMagnitude
		: DefaultDuration;

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Stun] Activated for %.1fs (by %s)"),
		Duration, TriggerEventData ? *GetNameSafe(TriggerEventData->Instigator.Get()) : TEXT("unknown"));

	// ① 하던 일 멈추기 (카운터 당한 패턴 등. AbilityTagsToKeep은 남김)
	CancelOtherAbilities(AbilityTagsToKeep);

	// ② 스턴 상태 태그. 시간이 있는 GE라 EndAbility를 놓쳐도 시간이 지나면 스스로 사라진다
	StunnedEffectHandle = ApplyStateTagsToSelf(FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Debuff_Stunned), Duration);

	// ③ 그 자리에 선다
	StopMovement();

	// ④ 스턴 몽타주 (없으면 AnimBP가 State.CrowdControl.Debuff.Stunned를 보고 직접 재생해도 된다)
	PlayStunMontage();

	// ⑤ 시간이 다 되면 종료
	UAbilityTask_WaitDelay* WaitDuration = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	WaitDuration->OnFinish.AddDynamic(this, &ULOAGA_Stun::OnStunFinished);
	WaitDuration->ReadyForActivation();
}

void ULOAGA_Stun::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// State.CrowdControl.Debuff.Stunned를 지운다. (정상 종료면 GE도 같은 시각에 끝나고, 사망으로 일찍 끝나면 여기서 지운다)
	if (StunnedEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->RemoveActiveGameplayEffect(StunnedEffectHandle);
		}
		StunnedEffectHandle.Invalidate();
	}

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Stun] Ended%s"), bWasCancelled ? TEXT(" (cancelled)") : TEXT(""));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULOAGA_Stun::OnStunFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULOAGA_Stun::StopMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// 몬스터·보스: AI 길 찾기를 멈춘다. (안 멈추면 이동 컴포넌트를 세워도 다음 틱에 다시 걸어간다)
	if (AAIController* AIController = Cast<AAIController>(Character->GetController()))
	{
		AIController->StopMovement();
	}

	Character->GetCharacterMovement()->StopMovementImmediately();
}

void ULOAGA_Stun::PlayStunMontage()
{
	if (!Montage)
	{
		return;
	}

	// 몽타주가 끝나거나 끊겨도 Ability는 끝내지 않는다. (끝나는 시각은 스턴 시간이 정한다)
	// bStopWhenAbilityEnds = true: 시간이 다 돼서 Ability가 끝나면 몽타주도 빠진다
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds*/ true);
	MontageTask->ReadyForActivation();
}
