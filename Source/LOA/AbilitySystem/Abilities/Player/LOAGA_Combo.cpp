// LOAGA_Combo.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_Combo.h"
#include "AbilitySystem/Tasks/LOAAbilityTask_ComboInput.h"
#include "AbilitySystem/Tasks/LOAAbilityTask_WaitCursorTarget.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Animation/LOAAnimNotify_SendGameplayEvent.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Combo)

ULOAGA_Combo::ULOAGA_Combo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 인스턴스 정책, 예측 실행, 이동 금지, 커서 회전은 부모 기본값을 그대로 쓴다.
}

// =============================================================================
// 흐름
// =============================================================================

void ULOAGA_Combo::StartSkill()
{
	// 발동마다 초기화 (인스턴스를 재사용하므로 이전 발동 값이 남지 않게)
	CurrentStageIndex = 0;
	bComboWindowOpen = false;
	bNextBuffered = false;
	InputTask = nullptr;

	if (ComboStages.IsEmpty())
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[%s] Combo: Combo Stages is empty."), *GetNameSafe(GetClass()));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// ① 몽타주 재생 (처음부터 = 1타 섹션). 끝나면(어느 타에서든 섹션 끝까지 가면) 부모가 정상 종료한다.
	if (!PlayMontageAndEnd())
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[%s] Combo: Montage is not set."), *GetNameSafe(GetClass()));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Combo] %s : stage 1/%d (%s)"),
		*GetNameSafe(GetClass()), ComboStages.Num(), *ComboStages[0].SectionName.ToString());

	if (!HasNextStage())
	{
		return;	// 한 타짜리 콤보: 기다릴 것이 없다
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const bool bLocallyControlled = ActorInfo && ActorInfo->IsLocallyControlled();

	if (bLocallyControlled)
	{
		// ② [조종하는 사람 컴퓨터] ComboOpen 대기. 다음 타를 정하는 건 이쪽뿐이다.
		//    OnlyTriggerOnce = false : 타마다 ComboOpen이 오므로 계속 받는다.
		//    OnlyMatchExact  = true  : 정확히 이 태그만
		UAbilityTask_WaitGameplayEvent* OpenTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, LOAGameplayTags::Event_Montage_ComboOpen, nullptr, false, true);
		OpenTask->EventReceived.AddDynamic(this, &ThisClass::OnComboOpen);
		OpenTask->ReadyForActivation();

		// ③ [조종하는 사람 컴퓨터] 입력 Task: 다시 누름 / 누르고 있는지. 주인 = 이 Ability → 끝나면 엔진이 정리
		InputTask = ULOAAbilityTask_ComboInput::ListenComboInput(this);
		InputTask->OnPressed.AddDynamic(this, &ThisClass::OnLocalPressed);
		InputTask->ReadyForActivation();
		return;
	}

	// ② [서버 - 원격 플레이어] "다음 타" 신호 구독. 이 스킬, 이번 발동의 보관함 칸 (키 누름 신호와 같은 방식)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		NextStageSignalHandle = ASC->AbilityReplicatedEventDelegate(
			EAbilityGenericReplicatedEvent::GenericSignalFromClient, CurrentSpecHandle, GetCurrentActivationInfo().GetActivationPredictionKey())
			.AddUObject(this, &ThisClass::OnNextStageSignal);
	}

	// ③ [서버 - 원격 플레이어] 돌아야 하는 콤보면 다음 타 커서도 기다린다.
	//    발동 때 커서는 부모(WaitCursorTarget)가 이미 받아서 보관함에서 비웠다. 이후 오는 커서는 전부 다음 타 커서다.
	if (ShouldFaceCursorEachStage())
	{
		WaitNextCursor();
	}
}

void ULOAGA_Combo::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// [서버] "다음 타" 신호 구독 해제 (두 번 불려도 안전: 번호표가 있을 때만)
	if (NextStageSignalHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->AbilityReplicatedEventDelegate(
				EAbilityGenericReplicatedEvent::GenericSignalFromClient, Handle, ActivationInfo.GetActivationPredictionKey())
				.Remove(NextStageSignalHandle);
		}
		NextStageSignalHandle.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =============================================================================
// 조종하는 사람 컴퓨터
// =============================================================================

void ULOAGA_Combo::OnComboOpen(FGameplayEventData Payload)
{
	if (!IsActive() || !HasNextStage())
	{
		return;	// 마지막 타의 ComboOpen (찍혀 있어도 무시)
	}

	const bool bHeldAndContinue = bContinueWhileHeld && InputTask && InputTask->IsInputHeld();
	if (bNextBuffered || bHeldAndContinue)
	{
		AdvanceLocal();
		return;
	}

	// 아직 입력이 없음 → 지금부터 누르면 바로 다음 타
	bComboWindowOpen = true;
}

void ULOAGA_Combo::OnLocalPressed()
{
	if (!IsActive() || !HasNextStage())
	{
		return;	// 마지막 타 중에 누름 → 무시 (끝나고 다시 누르면 새 발동 = 1타)
	}

	if (bComboWindowOpen)
	{
		AdvanceLocal();
		return;
	}

	// ComboOpen 전에 누름 → 예약. ComboOpen이 오면 바로 다음 타
	bNextBuffered = true;
}

void ULOAGA_Combo::AdvanceLocal()
{
	if (ShouldFaceCursorEachStage())
	{
		// ① 커서 전달. 클라에서는 그 자리에서 커서를 서버로 보내고 OnNextCursorReady(또는 Failed)를 바로 부른다.
		//    → HandleNextCursor → FinishAdvanceLocal (신호 → 회전 → 잠금 → 점프)
		WaitNextCursor();
		return;
	}

	// 돌지 않는 콤보: 커서 없이 바로
	FinishAdvanceLocal(false, FVector::ZeroVector);
}

void ULOAGA_Combo::FinishAdvanceLocal(bool bHasCursor, const FVector& NextCursorLocation)
{
	if (!IsActive() || !HasNextStage())
	{
		return;
	}

	// ② "다음 타" 신호 (커서 다음, 점프 전. 헤더 [순서])
	SendNextStageSignal();

	// ③ 내 쪽: 회전 → 잠금 → 점프 (엔진이 점프 RPC를 서버로 보낸다)
	EnterStage(CurrentStageIndex + 1, bHasCursor, NextCursorLocation, true);
}

void ULOAGA_Combo::SendNextStageSignal()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ActorInfo || !ASC || ActorInfo->IsNetAuthority())
	{
		return;	// 리슨 호스트 / 스탠드얼론: 자기가 서버라 보낼 필요 없음
	}

	// 이 범위 안에서 보내는 신호는 새 예측 키로 묶인다. (WaitCursorTarget이 커서를 보낼 때와 같은 방식)
	FScopedPredictionWindow ScopedPrediction(ASC, true);

	// 데이터 없이 "왔다"만 알리는 GAS 범용 신호. 서버의 같은 보관함 칸(스킬 Handle + 발동 예측키)에 도착한다.
	ASC->ServerSetReplicatedEvent(
		EAbilityGenericReplicatedEvent::GenericSignalFromClient,
		CurrentSpecHandle,
		GetCurrentActivationInfo().GetActivationPredictionKey(),
		ASC->ScopedPredictionKey);
}

// =============================================================================
// 서버 (원격 플레이어)
// =============================================================================

void ULOAGA_Combo::OnNextStageSignal()
{
	// 보관함 칸의 "왔음" 기록을 지운다. (다음 신호도 같은 칸으로 온다)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ConsumeGenericReplicatedEvent(
			EAbilityGenericReplicatedEvent::GenericSignalFromClient, CurrentSpecHandle, GetCurrentActivationInfo().GetActivationPredictionKey());
	}

	if (!IsActive() || !HasNextStage())
	{
		return;
	}

	// 회전은 앞서 도착한 커서가 이미 했다. (돌지 않는 콤보는 회전 없음)
	// 몽타주 점프는 하지 않는다. 클라의 엔진 점프 RPC가 바로 뒤에 도착한다.
	EnterStage(CurrentStageIndex + 1, false, FVector::ZeroVector, false);
}

// =============================================================================
// 다음 타 커서 (양쪽. Face Cursor Each Stage일 때만)
// =============================================================================

void ULOAGA_Combo::WaitNextCursor()
{
	// 발동 때 커서를 보내는 것과 같은 Task. 주인 = 이 Ability → 끝나면 엔진이 정리
	ULOAAbilityTask_WaitCursorTarget* CursorTask = ULOAAbilityTask_WaitCursorTarget::WaitCursorTarget(this, CursorTraceChannel);
	CursorTask->ValidData.AddDynamic(this, &ThisClass::OnNextCursorReady);
	CursorTask->Failed.AddDynamic(this, &ThisClass::OnNextCursorFailed);
	CursorTask->ReadyForActivation();
}

void ULOAGA_Combo::OnNextCursorReady(const FGameplayAbilityTargetDataHandle& DataHandle)
{
	FVector NextCursorLocation = FVector::ZeroVector;
	const bool bHasCursor = ULOAAbilityTask_WaitCursorTarget::GetLocationFromData(DataHandle, NextCursorLocation);
	HandleNextCursor(bHasCursor, NextCursorLocation);
}

void ULOAGA_Combo::OnNextCursorFailed(const FGameplayAbilityTargetDataHandle& DataHandle)
{
	// 커서를 못 구함 (맵 밖은 가상 바닥 평면으로 처리되므로 거의 없음) → 지금 보는 방향 그대로
	HandleNextCursor(false, FVector::ZeroVector);
}

void ULOAGA_Combo::HandleNextCursor(bool bHasCursor, const FVector& NextCursorLocation)
{
	if (!IsActive())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		// [클라] 커서를 보냈다 → 이어서 신호 → 회전 → 잠금 → 점프
		FinishAdvanceLocal(bHasCursor, NextCursorLocation);
		return;
	}

	// [서버] 회전만. 단계 +1과 잠금은 바로 뒤에 오는 "다음 타" 신호가 한다.
	if (bHasCursor)
	{
		FaceLocation(NextCursorLocation);
	}

	// 이 커서는 (지금 + 1)타 것이다. 그다음 타가 또 있으면 그 커서도 기다린다.
	if (ComboStages.IsValidIndex(CurrentStageIndex + 2))
	{
		WaitNextCursor();
	}
}

// =============================================================================
// 공용
// =============================================================================

void ULOAGA_Combo::EnterStage(int32 StageIndex, bool bHasCursor, const FVector& NextCursorLocation, bool bJumpMontage)
{
	// ① 단계 번호
	CurrentStageIndex = StageIndex;
	bComboWindowOpen = false;
	bNextBuffered = false;

	// ② 방향 (타마다 커서 쪽으로)
	if (bHasCursor)
	{
		FaceLocation(NextCursorLocation);
	}

	// ③ 잠금 다시 걸기. 이전 타의 후딜(이동 캔슬 허용)을 끝내고, 이 타의 Unlock까지 다시 잠근다.
	RestartLock();

	// ④ 몽타주 점프 (조종하는 사람 컴퓨터만. 엔진이 서버에도 점프를 전달한다)
	if (bJumpMontage)
	{
		MontageJumpToSection(ComboStages[StageIndex].SectionName);
	}

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Combo] %s : stage %d/%d (%s)"),
		*GetNameSafe(GetClass()), StageIndex + 1, ComboStages.Num(), *ComboStages[StageIndex].SectionName.ToString());
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOAGA_Combo::IsDataValid(FDataValidationContext& Context) const
{
	// 부모: 쿨타임 태그, 모듈 검사
	EDataValidationResult Result = Super::IsDataValid(Context);
	const FString OwnerName = GetNameSafe(GetClass());

	auto AddError = [&Context, &Result, &OwnerName](const FString& Message)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Combo : %s"), *OwnerName, *Message)));
		Result = EDataValidationResult::Invalid;
	};
	auto AddWarning = [&Context, &OwnerName](const FString& Message)
	{
		Context.AddWarning(FText::FromString(FString::Printf(TEXT("%s : Combo : %s"), *OwnerName, *Message)));
	};

	// ① 단계
	if (ComboStages.IsEmpty())
	{
		AddError(TEXT("Combo Stages is empty. Add at least one stage."));
	}

	if (!Montage)
	{
		AddError(TEXT("Montage is not set. A combo needs a montage with one section per stage."));
		return Result;
	}

	const FString MontageName = GetNameSafe(Montage);
	TSet<FName> SeenSections;

	for (int32 Index = 0; Index < ComboStages.Num(); ++Index)
	{
		const FName SectionName = ComboStages[Index].SectionName;
		const int32 SectionIndex = Montage->GetSectionIndex(SectionName);

		// ② 섹션이 몽타주에 있는지 / 중복인지
		if (SectionIndex == INDEX_NONE)
		{
			AddError(FString::Printf(TEXT("Stages[%d] section '%s' does not exist in %s."), Index, *SectionName.ToString(), *MontageName));
			continue;
		}
		if (SeenSections.Contains(SectionName))
		{
			AddError(FString::Printf(TEXT("Stages[%d] section '%s' is used twice."), Index, *SectionName.ToString()));
		}
		SeenSections.Add(SectionName);

		// ③ 섹션 연결은 비어 있어야 한다. 이어져 있으면 입력 없이 다음 섹션이 저절로 재생된다.
		const FName NextSection = Montage->CompositeSections[SectionIndex].NextSectionName;
		if (!NextSection.IsNone())
		{
			AddError(FString::Printf(TEXT("Stages[%d] section '%s' has a next section ('%s'). Clear it in Montage Sections; the code jumps to the next stage."),
				Index, *SectionName.ToString(), *NextSection.ToString()));
		}

		// ④ 마지막이 아닌 타에는 ComboOpen 노티파이가 섹션 안에 있어야 다음 타로 넘어갈 수 있다.
		const bool bIsLastStage = (Index == ComboStages.Num() - 1);
		if (!bIsLastStage)
		{
			const float SectionStart = Montage->CompositeSections[SectionIndex].GetTime();
			const float SectionEnd = SectionStart + Montage->GetSectionLength(SectionIndex);

			bool bHasComboOpen = false;
			for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
			{
				const ULOAAnimNotify_SendGameplayEvent* SendEvent = Cast<ULOAAnimNotify_SendGameplayEvent>(NotifyEvent.Notify);
				const float NotifyTime = NotifyEvent.GetTriggerTime();
				if (SendEvent && SendEvent->GetEventTag() == LOAGameplayTags::Event_Montage_ComboOpen
					&& NotifyTime >= SectionStart && NotifyTime < SectionEnd)
				{
					bHasComboOpen = true;
					break;
				}
			}

			if (!bHasComboOpen)
			{
				// 경고: 몽타주를 나중에 고칠 수도 있으니 저장은 막지 않는다.
				AddWarning(FString::Printf(TEXT("Stages[%d] section '%s' has no Event.Montage.ComboOpen notify. The combo will stop at this stage."),
					Index, *SectionName.ToString()));
			}
		}
	}

	// ⑤ 첫 단계 = 몽타주의 첫 섹션 (몽타주는 처음부터 재생된다)
	if (!ComboStages.IsEmpty())
	{
		const int32 FirstSectionIndex = Montage->GetSectionIndexFromPosition(0.f);
		const FName FirstSection = Montage->GetSectionName(FirstSectionIndex);
		if (FirstSectionIndex != INDEX_NONE && FirstSection != ComboStages[0].SectionName)
		{
			AddError(FString::Printf(TEXT("Stages[0] section '%s' must be the first section of %s (now '%s')."),
				*ComboStages[0].SectionName.ToString(), *MontageName, *FirstSection.ToString()));
		}
	}

	return Result;
}
#endif
