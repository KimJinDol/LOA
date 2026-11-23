// LOAGA_JustGuard.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_JustGuard.h"
#include "Combat/LOAJustGuardComponent.h"
#include "Combat/LOACombatRPCComponent.h"
#include "Library/LOACombatSettings.h"
#include "Player/LOAPlayerController.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOANetTime.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_JustGuard)

ULOAGA_JustGuard::ULOAGA_JustGuard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 누를 때 커서 쪽으로 돈다 (다른 스킬과 같음). 공격 기준점이 정면 ±45° 안이어야 막는다 (장부 [판정 규칙] 방향)
	//   조종하는 사람 컴퓨터는 돌자마자 시작 (누른 시각이 늦어지지 않음), 서버는 커서 위치가 도착하면 같은 쪽으로 돈다
	bFaceCursor = true;

	// bBlockMove / bBlockAbility는 부모 기본값(true): 저스트 가드 중에는 이동·다른 스킬 불가
	// 회피는 위 잠금(State.Block.Ability)을 무시하는 Ability라 따로 막는다:
	// Block Abilities with Tag = GAS 기본 기능. 이 GA가 실행 중인 동안(Guard·Success·Fail 전부) 이 태그의 Ability는 발동하지 않는다
	BlockAbilitiesWithTag.AddTag(LOAGameplayTags::Ability_Common_Dodge);
	// 쿨타임 없음 (Cooldown Duration 0 기본값)
}

// =============================================================================
// 언제 쓸 수 있나
// =============================================================================

bool ULOAGA_JustGuard::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 서버(원격 플레이어의 발동)는 검사하지 않는다. 헤더 [언제 쓸 수 있나]
	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return true;
	}

	// 열려 있고 아직 안 누른 저가 구간이 있나 (한 구간에 한 번)
	const AActor* Avatar = ActorInfo->AvatarActor.Get();
	const ULOAJustGuardComponent* JustGuardComp = Avatar ? Avatar->FindComponentByClass<ULOAJustGuardComponent>() : nullptr;
	return JustGuardComp && JustGuardComp->FindPressableWindow() != 0;
}

// =============================================================================
// 시작
// =============================================================================

void ULOAGA_JustGuard::StartSkill()
{
	bSucceeded = false;
	bReleased = false;
	bFinishing = false;
	WindowId = 0;
	WindowMode = ELOAJustGuardMode::None;
	
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ULOAJustGuardComponent* JustGuardComp = GetJustGuardComponent();
	UAnimMontage* MontageToPlay = GetMontage();
	if (!Info || !ASC || !JustGuardComp || !MontageToPlay)
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[%s] JustGuard: no ULOAJustGuardComponent on the character or no montage."),
			*GetNameSafe(GetClass()));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// ① Guard 구간부터 재생. Success / Fail 구간이 끝나면 종료 (부모의 몽타주 결과 처리)
	//    서버 GA는 여기까지만: 구간 점프는 조종하는 사람의 점프가 엔진을 타고 온다 (헤더 [네트워크])
	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, MontagePlayRate, GuardSection);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();

	if (!Info->IsLocallyControlled())
	{
		return;
	}

	// ② 지금 열린 저가 구간(#7)에 누름 기록. 원격이면 서버 사본에도 (같은 PC 채널 → 나중의 "막음" 보고보다 먼저 도착)
	//    발동 조건(CanActivateAbility)을 통과했으니 구간이 있다. 그 사이 닫혔으면(드묾) 실패로 끝난다
	WindowId = JustGuardComp->FindPressableWindow();
	const FLOAJustGuardRecord* Record = JustGuardComp->FindRecord(WindowId);
	const double Now = LOANetTime::GetServerTimeNow(GetWorld());
	if (Record)
	{
		// 방향 = 방금 커서 쪽으로 돈 방향 (부모가 StartSkill 전에 돌았다). 저가 중엔 못 도니까 이게 이번 저가의 정면
		const float Yaw = JustGuardComp->GetOwner()->GetActorRotation().Yaw;
		WindowMode = Record->Mode;
		JustGuardComp->RecordPress(WindowId, Record->Mode, Now, Yaw);
		if (!HasAuthority(&CurrentActivationInfo))
		{
			const ALOAPlayerController* PC = GetLOAPlayerControllerFromActorInfo();
			if (ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr)
			{
				CombatRPC->ServerReportJustGuardPress(WindowId, Now, FRotator::CompressAxisToByte(Yaw));
			}
		}
	}

	// ③ 성공 알림 (내 판정 / 서버 판정 알림 / 리슨 호스트는 서버 판정) + 구간 끝 알림 (홀딩)
	GuardSucceededHandle = JustGuardComp->OnGuardSucceeded.AddUObject(this, &ThisClass::OnGuardSucceeded);
	WindowClosedHandle = JustGuardComp->OnWindowClosed.AddUObject(this, &ThisClass::OnWindowClosed);
	ListenedJustGuardComp = JustGuardComp;

	// ④ 실패 판단 시각 = 막기 유효 시간 + 결과 대기
	const float Window = GetDefault<ULOACombatSettings>()->JustGuardWindow;
	UAbilityTask_WaitDelay* DecisionTask = UAbilityTask_WaitDelay::WaitDelay(this, Window + ResultWaitTime);
	DecisionTask->OnFinish.AddDynamic(this, &ThisClass::OnDecisionTimeReached);
	DecisionTask->ReadyForActivation();

	// ⑤ G 뗌 대기. (LOA ASC가 뗄 때 InvokeReplicatedEvent(InputReleased)를 부른다. 차지 Task와 같은 방식)
	//    이미 뗐으면(아주 짧게 탭) 기록이 남아 있어서 바로 OnInputReleased가 불린다
	InputReleasedPredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey();
	InputReleasedHandle = ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased,
		GetCurrentAbilitySpecHandle(), InputReleasedPredictionKey).AddUObject(this, &ThisClass::OnInputReleased);
	ASC->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputReleased,
		GetCurrentAbilitySpecHandle(), InputReleasedPredictionKey);
}

// =============================================================================
// 뗌 / 성공 / 실패 (조종하는 사람)
// =============================================================================

void ULOAGA_JustGuard::OnInputReleased()
{
	if (bReleased || WindowId == 0 || !IsActive())
	{
		return;
	}
	bReleased = true;
	ReportRelease(/*bCancelled*/ false);

	// 이미 막았으면 (홀딩으로 누르고 있었음) 이제 Success. 아직이면 실패 판단 시각까지 기다린다 (단타는 뗀 뒤에 막히는 게 보통)
	if (bSucceeded)
	{
		FinishWithSection(SuccessSection);
	}
}

void ULOAGA_JustGuard::ReportRelease(bool bCancelled)
{
	// 장부 원본 + 원격이면 서버 사본 (취소 = 그 순간 뗀 것 + 그 뒤 공격은 단타도 못 막음)
	const double Now = LOANetTime::GetServerTimeNow(GetWorld());
	if (ULOAJustGuardComponent* JustGuardComp = GetJustGuardComponent())
	{
		if (bCancelled)
		{
			JustGuardComp->RecordCancel(WindowId, Now);
		}
		else
		{
			JustGuardComp->RecordRelease(WindowId, Now);
		}
	}
	if (!HasAuthority(&CurrentActivationInfo))
	{
		const ALOAPlayerController* PC = GetLOAPlayerControllerFromActorInfo();
		if (ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr)
		{
			CombatRPC->ServerReportJustGuardRelease(WindowId, Now, bCancelled);
		}
	}
}

void ULOAGA_JustGuard::OnGuardSucceeded(int32 InWindowId)
{
	// 다른 구간의 알림은 버린다. 홀딩 연타는 같은 구간으로 여러 번 온다 → 처음 한 번만
	if (!IsActive() || bFinishing || bSucceeded || InWindowId != WindowId)
	{
		return;
	}
	bSucceeded = true;

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[JustGuard] success (window #%d)"), InWindowId);

	if (bReleased)
	{
		FinishWithSection(SuccessSection);
		return;
	}

	// 아직 누르고 있음 (홀딩) → Guard 자세 유지. 뗄 때 Success. 너무 오래 누르면 자동으로 끝냄
	UAbilityTask_WaitDelay* HoldTask = UAbilityTask_WaitDelay::WaitDelay(this, MaxHoldTime);
	HoldTask->OnFinish.AddDynamic(this, &ThisClass::OnMaxHoldTimeReached);
	HoldTask->ReadyForActivation();
}

void ULOAGA_JustGuard::OnWindowClosed(int32 InWindowId)
{
	// 내가 누른 구간 + 홀딩 + 아직 누르고 있음 + 아직 안 끝남
	//   단타는 실패 판단 시각(④)이 끝낸다. 이미 뗐으면 뗄 때 끝났거나 실패 판단 시각을 기다리는 중
	const bool bHold = (WindowMode == ELOAJustGuardMode::Hold || WindowMode == ELOAJustGuardMode::HoldFromMiddle);
	if (!IsActive() || bFinishing || bReleased || !bHold || InWindowId != WindowId)
	{
		return;
	}

	// 마지막 타의 성공 알림은 "닫힘"보다 먼저 온다 (같은 통로로 순서대로) → 지금 막은 적이 없으면 못 막은 것
	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[JustGuard] window #%d closed while holding → %s"),
		InWindowId, bSucceeded ? TEXT("success") : TEXT("fail"));
	FinishWithSection(bSucceeded ? SuccessSection : FailSection);
}

void ULOAGA_JustGuard::OnDecisionTimeReached()
{
	if (!bSucceeded)
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[JustGuard] fail (window #%d, not blocked in time)"), WindowId);
		FinishWithSection(FailSection);
	}
}

void ULOAGA_JustGuard::OnMaxHoldTimeReached()
{
	FinishWithSection(SuccessSection);
}

void ULOAGA_JustGuard::FinishWithSection(FName Section)
{
	if (bFinishing || !IsActive())
	{
		return;
	}
	bFinishing = true;

	// 그 구간으로 넘어간다. 구간이 끝나면 몽타주가 끝나 종료 (OnMontageFinished)
	// 조종하는 사람의 점프는 엔진이 서버 몽타주에 전달 → 서버가 다른 사람 화면에 복제
	const UAnimMontage* MontageToPlay = GetMontage();
	if (MontageToPlay && MontageToPlay->IsValidSectionName(Section))
	{
		MontageJumpToSection(Section);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

// =============================================================================
// 종료
// =============================================================================

void ULOAGA_JustGuard::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 끝나는 중이라 구간 점프는 하지 않게 먼저 표시
	bFinishing = true;
	if (IsActive() && ActorInfo && ActorInfo->IsLocallyControlled() && WindowId != 0)
	{
		if (bWasCancelled)
		{
			// 피격 이상(띄움·경직 등)으로 끊김 → 취소 (단타도 그 뒤 공격은 못 막는다. 이미 뗐어도 기록)
			ReportRelease(/*bCancelled*/ true);
			bReleased = true;
		}
		else
		{
			// 정상 종료인데 아직 누르고 있음 (홀딩 최대 시간 등) → 뗌
			OnInputReleased();
		}
	}

	// 구독 해제 (두 번 불려도 안전)
	if (ULOAJustGuardComponent* JustGuardComp = ListenedJustGuardComp.Get())
	{
		JustGuardComp->OnGuardSucceeded.Remove(GuardSucceededHandle);
		JustGuardComp->OnWindowClosed.Remove(WindowClosedHandle);
	}
	GuardSucceededHandle.Reset();
	WindowClosedHandle.Reset();
	ListenedJustGuardComp.Reset();

	if (InputReleasedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, Handle, InputReleasedPredictionKey)
				.Remove(InputReleasedHandle);
		}
		InputReleasedHandle.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =============================================================================
// 도우미
// =============================================================================

ULOAJustGuardComponent* ULOAGA_JustGuard::GetJustGuardComponent() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->FindComponentByClass<ULOAJustGuardComponent>() : nullptr;
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOAGA_JustGuard::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// C++ 부모 자체는 몽타주가 없는 게 정상 → BP 자식만
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return Result;
	}

	const FString OwnerName = GetNameSafe(GetClass());
	if (!Montage)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Montage is empty. Just guard needs a montage with Guard / Success / Fail sections."), *OwnerName)));
		return EDataValidationResult::Invalid;
	}

	for (const FName& Section : { GuardSection, SuccessSection, FailSection })
	{
		if (!Montage->IsValidSectionName(Section))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s : montage %s has no section '%s'."),
				*OwnerName, *GetNameSafe(Montage), *Section.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
