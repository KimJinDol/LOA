// LOAGA_Airborne.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_Airborne.h"
#include "Combat/LOACrowdControlPredictionComponent.h"
#include "AbilitySystem/Effects/LOAGE_Airborne.h"
#include "Character/Player/LOACharacterMovementComponent.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Airborne)

ULOAGA_Airborne::ULOAGA_Airborne(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버가 발동하고, 맞은 사람 컴퓨터에서도 같이 돈다 (그쪽은 몽타주만). 헤더 [네트워크]
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 발동 조건: 띄움 태그가 붙는 순간 (0 → 1). 공중에서 또 맞아 개수만 늘 때는 발동하지 않는다 → 그건 OnEffectAppliedToSelf가
	FAbilityTriggerData AirborneTrigger;
	AirborneTrigger.TriggerTag = LOAGameplayTags::State_CrowdControl_Push_Airborne;
	AirborneTrigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(AirborneTrigger);

	// 취소하지 않을 성질 (2026-11-09): 아이덴티티처럼 CC를 맞아도 켜져 있어야 하는 Ability
	AbilityTagsToKeep.AddTag(LOAGameplayTags::Ability_Trait_KeepOnCC);

	// 기상기가 이 태그로 이 Ability를 끊는다 (ULOAGA_WakeUp의 Cancel Abilities with Tag)
	SetAssetTags(FGameplayTagContainer(LOAGameplayTags::Ability_Common_Airborne));

	// 죽어 있으면 발동 불가 (부모 생성자의 State.Body.Dead 그대로)
}

// =============================================================================
// 발동 / 종료
// =============================================================================

void ULOAGA_Airborne::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility는 부르지 않는다. (스턴·사망과 같음. 흐름은 여기 한 곳에서만)

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!ASC || !Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Phase = EPhase::InAir;
	bHasLaunchGround = false;	// 이번 띄움의 "처음 뜬 땅"은 첫 발사 때 정한다

	// ① 하던 일 멈추기 [둘 다] (B 컴퓨터도 끊어야 내 화면에서 바로 끊긴다. 서버도 끊고 결과를 복제한다)
	CancelOtherAbilities(AbilityTagsToKeep);

	if (IsServerSide())
	{
		// [서버만] 이동 금지 (GE라 B 컴퓨터에도 복제 → 클릭 이동이 멈춘다)
		BlockMoveEffectHandle = ApplyStateTagsToSelf(FGameplayTagContainer(LOAGameplayTags::State_Block_Move));

		// [서버만] 알림 구독: 또 맞음 / 띄움 태그가 다 빠짐(안전 시간)
		EffectAppliedHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ThisClass::OnEffectAppliedToSelf);
		AirborneTagHandle = ASC->RegisterGameplayTagEvent(
			LOAGameplayTags::State_CrowdControl_Push_Airborne, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::OnAirborneTagChanged);
	}
	else
	{
		// [B 컴퓨터만] 서버의 단계를 태그로 따라간다: 누움(착지 알림을 놓쳤을 때 대비) / 일어남
		DownTagHandleOnOwner = ASC->RegisterGameplayTagEvent(
			LOAGameplayTags::State_CrowdControl_Push_Down, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnDownTagChangedOnOwner);
		GettingUpTagHandleOnOwner = ASC->RegisterGameplayTagEvent(
			LOAGameplayTags::State_CrowdControl_Push_GettingUp, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnGettingUpTagChangedOnOwner);
	}

	// [둘 다] 착지 알림. 서버: 누움 시작 / B 컴퓨터: 내 화면에서 땅에 닿는 순간 Down 구간
	Character->LandedDelegate.AddUniqueDynamic(this, &ThisClass::OnLanded);

	// [B 컴퓨터] 피해자 판정에서 내가 먼저 띄움을 예측했으면 이어받는다 (예측 몽타주가 재생 중이면 그 위치부터)
	//   ULOACrowdControlPredictionComponent
	UAnimMontage* MontageToPlay = GetMontage();
	float ResumeTime = -1.f;
	if (!IsServerSide())
	{
		if (ULOACrowdControlPredictionComponent* PredictionComp = GetAvatarActorFromActorInfo()->FindComponentByClass<ULOACrowdControlPredictionComponent>())
		{
			ResumeTime = PredictionComp->ConfirmPrediction(MontageToPlay);
		}
	}

	// 몽타주: 공중 섹션부터 (예측을 이어받으면 그 위치부터). 일어나는 섹션이 끝나면(BlendOut/Completed) 종료. Ability가 끝나면 몽타주도 멈춘다
	if (MontageToPlay)
	{
		const bool bResume = ResumeTime >= 0.f;
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, MontageToPlay, MontagePlayRate, bResume ? NAME_None : AirSection, /*bStopWhenAbilityEnds*/ true,
			/*AnimRootMotionTranslationScale*/ 1.f, /*StartTimeSeconds*/ bResume ? ResumeTime : 0.f);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnAirborneMontageEnded);
		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnAirborneMontageEnded);
		MontageTask->ReadyForActivation();
	}

	// ② 처음 발사 [서버만]: 방금 붙은 띄움 GE (태그가 붙는 순간 발동했으므로 이미 목록에 있다)
	//    B 컴퓨터는 서버가 정한 궤적 숫자를 이동 컴포넌트 복제로 받는다.
	if (!IsServerSide())
	{
		return;
	}

	if (const FActiveGameplayEffect* Newest = FindNewestAirborneEffect())
	{
		LastLaunchEffectHandle = Newest->Handle;
		LaunchFromEffect(Newest->Spec);
	}
	else
	{
		// 띄움 태그를 ULOAGE_Airborne이 아닌 GE로 붙였다 → 숫자·방향을 모름. 날리지 않고 그 자리에서 눕기만 한다
		LOA_LOG(Character, Warning, TEXT("[Airborne] Tag came from a non-LOAGE_Airborne effect. No launch."));
	}
}

void ULOAGA_Airborne::EndAbility(
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
			ASC->RegisterGameplayTagEvent(LOAGameplayTags::State_CrowdControl_Push_Airborne, EGameplayTagEventType::AnyCountChange)
				.Remove(AirborneTagHandle);
		}
		EffectAppliedHandle.Reset();
		AirborneTagHandle.Reset();

		// [B 컴퓨터] 예측으로 붙인 이동 금지를 푼다 (진짜 이동 금지 GE도 이 GA와 같이 끝난다)
		const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		if (ULOACrowdControlPredictionComponent* PredictionComp = Avatar ? Avatar->FindComponentByClass<ULOACrowdControlPredictionComponent>() : nullptr;
			PredictionComp && !IsServerSide())
		{
			PredictionComp->EndPrediction();
		}

		// [B 컴퓨터] 태그 따라가기 알림 해제
		if (ASC)
		{
			ASC->RegisterGameplayTagEvent(LOAGameplayTags::State_CrowdControl_Push_Down, EGameplayTagEventType::NewOrRemoved)
				.Remove(DownTagHandleOnOwner);
			ASC->RegisterGameplayTagEvent(LOAGameplayTags::State_CrowdControl_Push_GettingUp, EGameplayTagEventType::NewOrRemoved)
				.Remove(GettingUpTagHandleOnOwner);
		}
		DownTagHandleOnOwner.Reset();
		GettingUpTagHandleOnOwner.Reset();

		if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
		{
			Character->LandedDelegate.RemoveDynamic(this, &ThisClass::OnLanded);
		}

		// 여기부터 [서버만]: 붙인 GE 지우기, 궤적 끝내기. (B 컴퓨터는 결과를 복제로 받는다)
		if (IsServerSide())
		{
			RemoveOwnedEffect(GettingUpEffectHandle);
			RemoveOwnedEffect(DownEffectHandle);
			RemoveOwnedEffect(BlockMoveEffectHandle);

			// 아직 날아가는 중에 끊겼으면(사망 등) 궤적도 지운다. 이미 착지했으면 아무 일 없다
			if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
			{
				if (ULOACharacterMovementComponent* Movement = Cast<ULOACharacterMovementComponent>(Character->GetCharacterMovement()))
				{
					Movement->StopCrowdControlMove();
				}
			}

			// 띄움 GE를 모두 지운다 (정상 종료든, 기상기·사망으로 끊겼든). 띄움 GE의 시간은 안전용이라 끝내는 쪽은 언제나 이 Ability다
			//   → 띄움 태그가 빠져 미끄러짐·때린 쪽 바라보기도 풀린다
			if (ASC)
			{
				ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Push_Airborne));
			}
		}

		LastLaunchEffectHandle.Invalidate();

		LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Airborne] Ended%s"), bWasCancelled ? TEXT(" (cancelled)") : TEXT(""));
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =============================================================================
// 발사
// =============================================================================

void ULOAGA_Airborne::LaunchFromEffect(const FGameplayEffectSpec& Spec)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	ULOACharacterMovementComponent* Movement = Character ? Cast<ULOACharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Movement)
	{
		LOA_LOG(Character, Warning, TEXT("[Airborne] Needs ULOACharacterMovementComponent. No launch."));
		return;
	}

	// 숫자: 높이·공중 시간·보너스는 GE 칸 (고정), 거리·누워 있을 시간은 Payload 값이 있으면 그것
	const ULOAGE_Airborne* AirborneDef = Cast<const ULOAGE_Airborne>(Spec.Def.Get());
	const float ApexHeight = AirborneDef ? AirborneDef->LaunchHeight : 250.f;
	const float AirTime = AirborneDef ? AirborneDef->AirTime : 1.f;
	const float HighHitBonus = AirborneDef ? AirborneDef->HighHitDistanceBonus : 0.f;
	const float DefaultDistance = AirborneDef ? AirborneDef->DefaultDistance : 0.f;
	const float BaseDistance = FMath::Max(0.f,
		Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_LaunchDistance, /*WarnIfNotFound*/ false, DefaultDistance));

	// 착지한 뒤 누워 있을 시간. 마지막으로 날린 GE의 값을 쓴다 (공중에서 또 맞으면 새 GE의 값으로 바뀜)
	const float DefaultDownDuration = AirborneDef ? AirborneDef->DefaultDownDuration : 1.f;
	PendingDownDuration = FMath::Max(0.f,
		Spec.GetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_DownDuration, /*WarnIfNotFound*/ false, DefaultDownDuration));

	// 땅 기준: 처음 뜰 때의 발 높이. 공중에서 또 맞으면 그 땅에서 몇 cm 떠 있는지 = 출발 높이
	//   꼭대기는 언제나 "처음 뜬 땅 + Launch Height" → 연속으로 맞아도 하늘로 솟지 않는다 (사용자 결정)
	const float FeetZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
	if (!bHasLaunchGround)
	{
		LaunchGroundZ = FeetZ;
		bHasLaunchGround = true;
	}
	const float StartHeight = FMath::Clamp(FeetZ - LaunchGroundZ, 0.f, ApexHeight);

	// 높이 맞을수록 멀리 (사용자 결정). 예) 거리 300, 보너스 1, 꼭대기 250에서 맞음 → 300 × (1 + 1 × 250 ÷ 250) = 600
	const float Distance = BaseDistance * (1.f + HighHitBonus * StartHeight / FMath::Max(ApexHeight, 1.f));

	// 방향 규칙은 띄움·경직 공용 (ULOAGE_CrowdControl)
	const FVector Direction = ULOAGE_CrowdControl::ComputePushDirection(Spec, Character);

	// 정해진 궤적 (물리 아님): 옆은 일정한 속도, 높이는 포물선, Air Time 뒤 착지. 맞은 사람 컴퓨터에도 같은 숫자로 걸린다.
	// 때린 쪽 바라보기도 여기서 (이동 컴포넌트가 서버·맞은 사람 컴퓨터에서 각자)
	Movement->StartCrowdControlMove(Direction, Distance, StartHeight, ApexHeight, AirTime);

	LOA_LOG(Character, Log, TEXT("[Airborne] Launch: distance %.0f (base %.0f), from height %.0f to apex %.0f, air %.2fs, dir (%.2f, %.2f), down %.2fs"),
		Distance, BaseDistance, StartHeight, ApexHeight, AirTime, Direction.X, Direction.Y, PendingDownDuration);
}

const FActiveGameplayEffect* ULOAGA_Airborne::FindNewestAirborneEffect() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return nullptr;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Push_Airborne));

	const FActiveGameplayEffect* Newest = nullptr;
	for (const FActiveGameplayEffectHandle& EffectHandle : ASC->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(EffectHandle);
		if (!Effect || !Cast<const ULOAGE_Airborne>(Effect->Spec.Def.Get()))
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

void ULOAGA_Airborne::OnEffectAppliedToSelf(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle ActiveHandle)
{
	// 띄움 GE만. 발동 순간 이미 쓴 GE의 알림(같은 번호)은 무시
	if (!Cast<const ULOAGE_Airborne>(Spec.Def.Get()) || ActiveHandle == LastLaunchEffectHandle)
	{
		return;
	}

	// 누워 있거나 일어나는 중이면 GE의 Ignore 조건(Down)에서 이미 걸러져 여기 오지 않는다. 혹시 조건이 빠진 GE면 여기서 막는다
	if (Phase != EPhase::InAir)
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning,
			TEXT("[Airborne] %s applied while down. Add State.CrowdControl.Push.Down to its Ignore Tags."), *GetNameSafe(Spec.Def.Get()));
		return;
	}

	// 공중에서 또 맞음 → 다시 날린다 (누워 있을 시간도 새 GE의 값으로)
	LastLaunchEffectHandle = ActiveHandle;
	LaunchFromEffect(Spec);
}

void ULOAGA_Airborne::OnAirborneTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 띄움 GE의 안전 시간이 다 됐는데 아직 이 Ability가 돈다 = 착지를 못 하고 있다 (맵 밖으로 떨어지는 중 등).
	// 정상이면 이 Ability가 먼저 끝나면서 GE를 지우므로 여기 오지 않는다. → 경고하고 끝낸다 (영원히 막히지 않게)
	if (NewCount <= 0)
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[Airborne] Safety duration ran out (phase %d). Ending."), static_cast<int32>(Phase));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULOAGA_Airborne::OnLanded(const FHitResult& Hit)
{
	// 누운 채로 낭떠러지에서 떨어져 다시 착지하는 경우 등은 무시
	if (Phase != EPhase::InAir)
	{
		return;
	}

	// 착지 → 눕는다. 누워 있을 시간은 착지한 순간부터 센다 (높이를 바꿔도 누워 있는 시간은 같다)
	EnterDown();
}

void ULOAGA_Airborne::OnAirborneMontageEnded()
{
	// 일어나는 섹션이 끝났을 때만 종료. 공중·누움 섹션이 반복 연결이 안 돼 있어서 끝났으면 경고만 (시간·착지가 흐름을 정한다)
	// B 컴퓨터: 자기 GA만 끝낸다 (서버에 알리지 않음. 알려도 서버는 받지 않는다 — NetSecurityPolicy ServerOnly)
	if (Phase == EPhase::GettingUp)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ IsServerSide(), false);
		return;
	}

	LOA_LOG(GetAvatarActorFromActorInfo(), Warning,
		TEXT("[Airborne] Montage ended before get-up. Make sections %s and %s loop to themselves."),
		*AirSection.ToString(), *DownSection.ToString());
}

// =============================================================================
// 단계
// =============================================================================

void ULOAGA_Airborne::EnterDown()
{
	Phase = EPhase::Down;

	// [둘 다] 누움 구간. 서버 → 남의 화면, B 컴퓨터 → 내 화면
	JumpToSectionIfAny(DownSection);

	// 여기부터 [서버만]: 누움 태그와 누운 시간 재기. B 컴퓨터는 누운 시간을 모른다(SetByCaller) → 일어남 태그를 기다린다
	if (!IsServerSide())
	{
		return;
	}

	// 누움 태그 (복제): 다시 안 뜸 + 기상기 발동 조건 + 회피 막힘. 완전히 일어날 때까지 유지. 지우는 쪽 = EndAbility
	DownEffectHandle = ApplyStateTagsToSelf(FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Push_Down));

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Airborne] Landed → down for %.2fs"), PendingDownDuration);

	// 누워 있을 시간이 끝나면 일어난다. Ability가 먼저 끝나면(기상기·사망) 엔진이 이 대기도 정리한다
	UAbilityTask_WaitDelay* WaitDown = UAbilityTask_WaitDelay::WaitDelay(this, PendingDownDuration);
	WaitDown->OnFinish.AddDynamic(this, &ThisClass::OnDownTimeFinished);
	WaitDown->ReadyForActivation();
}

void ULOAGA_Airborne::OnDownTimeFinished()
{
	if (Phase == EPhase::Down)
	{
		StartGetUp();
	}
}

void ULOAGA_Airborne::StartGetUp()
{
	// 누움 태그는 지우지 않는다: 일어나는 모션 중에도 기상기가 나가고, 회피는 막히고, 맞아도 안 뜬다 (사용자 결정)
	Phase = EPhase::GettingUp;

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Airborne] Getting up"));

	// 일어남 태그 (복제) → B 컴퓨터의 이 GA가 보고 GetUp 구간으로 넘어간다. 지우는 쪽 = EndAbility
	GettingUpEffectHandle = ApplyStateTagsToSelf(FGameplayTagContainer(LOAGameplayTags::State_CrowdControl_Push_GettingUp));

	// 일어나는 섹션이 없으면 바로 끝
	const UAnimMontage* MontageToPlay = GetMontage();
	if (!MontageToPlay || !MontageToPlay->IsValidSectionName(GetUpSection))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	JumpToSectionIfAny(GetUpSection);
}

// =============================================================================
// 도우미
// =============================================================================

// =============================================================================
// B 컴퓨터: 서버의 단계를 태그로 따라가기
// =============================================================================

void ULOAGA_Airborne::OnDownTagChangedOnOwner(const FGameplayTag Tag, int32 NewCount)
{
	// 보통은 내 착지 알림(OnLanded)이 먼저 와서 이미 Down이다. 놓쳤을 때만 여기서 넘어간다
	if (NewCount > 0 && Phase == EPhase::InAir)
	{
		EnterDown();
	}
}

void ULOAGA_Airborne::OnGettingUpTagChangedOnOwner(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0 && Phase != EPhase::GettingUp)
	{
		Phase = EPhase::GettingUp;
		JumpToSectionIfAny(GetUpSection);
	}
}

bool ULOAGA_Airborne::IsServerSide() const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info && Info->IsNetAuthority();
}

void ULOAGA_Airborne::JumpToSectionIfAny(FName Section)
{
	UAnimMontage* MontageToPlay = GetMontage();
	if (!MontageToPlay || !MontageToPlay->IsValidSectionName(Section))
	{
		return;
	}

	// 서버: GAS 함수로 점프 → 남의 화면으로 복제된다
	if (IsServerSide())
	{
		MontageJumpToSection(Section);
		return;
	}

	// B 컴퓨터: 내 화면만 점프한다. GAS 함수(MontageJumpToSection)는 클라에서 부르면 서버 몽타주도 점프시키는 RPC를 보낸다
	//   (UAbilitySystemComponent::CurrentMontageJumpToSection). 그러면 내가 먼저 착지했을 때 서버 몽타주가 미리 Down으로 넘어간다 → 쓰지 않는다
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (UAnimInstance* AnimInstance = Info ? Info->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_JumpToSection(Section, MontageToPlay);
	}
}

void ULOAGA_Airborne::RemoveOwnedEffect(FActiveGameplayEffectHandle& InOutHandle) const
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
