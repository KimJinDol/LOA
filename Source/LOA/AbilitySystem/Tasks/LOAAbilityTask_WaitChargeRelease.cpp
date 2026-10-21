// LOAAbilityTask_WaitChargeRelease.cpp

#include "AbilitySystem/Tasks/LOAAbilityTask_WaitChargeRelease.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAbilityTask_WaitChargeRelease)

namespace LOAChargeRelease
{
	/** 서버: 최대 시간이 지나고도 클라이언트 값이 이만큼 더 안 오면 서버가 잰 시간으로 끝낸다 (초) */
	constexpr float ServerGraceTime = 0.5f;

	/** 서버: 클라이언트가 보낸 시간이 "서버가 잰 시간 + 이 값"을 넘으면 잘라 낸다 (초). 네트워크 흔들림 허용치 */
	constexpr float ServerTolerance = 0.25f;
}

ULOAAbilityTask_WaitChargeRelease* ULOAAbilityTask_WaitChargeRelease::WaitChargeRelease(UGameplayAbility* OwningAbility, float MaxChargeTime)
{
	ULOAAbilityTask_WaitChargeRelease* Task = NewAbilityTask<ULOAAbilityTask_WaitChargeRelease>(OwningAbility);
	Task->MaxChargeTime = FMath::Max(MaxChargeTime, 0.f);
	return Task;
}

void ULOAAbilityTask_WaitChargeRelease::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UWorld* World = GetWorld();
	if (!ActorInfo || !AbilitySystemComponent.IsValid() || !World)
	{
		EndTask();
		return;
	}

	StartTime = World->GetTimeSeconds();
	bFinished = false;

	const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
	const FPredictionKey ActivationKey = GetActivationPredictionKey();

	if (ActorInfo->IsLocallyControlled())
	{
		// ---------- 조종하는 사람 컴퓨터 (리슨 호스트, 스탠드얼론 포함) ----------

		// 키 뗌 이벤트 대기. (LOA ASC가 뗄 때 InvokeReplicatedEvent(InputReleased)를 부른다)
		InputReleasedHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(
			EAbilityGenericReplicatedEvent::InputReleased, SpecHandle, ActivationKey)
			.AddUObject(this, &ThisClass::OnInputReleased);

		// 최대 시간이 되면 키를 안 떼도 끝낸다.
		World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::FinishLocal, FMath::Max(MaxChargeTime, KINDA_SMALL_NUMBER), false);

		// Task가 시작되기 전에 이미 뗐으면(아주 짧은 탭) 기록이 남아 있다 → 바로 OnInputReleased가 불린다.
		AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputReleased, SpecHandle, ActivationKey);
		return;
	}

	// ---------- 서버 (원격 플레이어의 Ability) ----------
	// 순서 보장: 클라이언트는 커서 데이터(WaitCursorTarget)를 먼저, 누른 시간을 나중에 보낸다.
	// 서버는 커서 데이터를 받아 처리한 직후(같은 흐름)에 이 Task를 시작하므로 두 데이터가 섞이지 않는다.
	TargetDataHandle = AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationKey)
		.AddUObject(this, &ThisClass::OnTargetDataReplicated);

	// 클라이언트 값이 끝내 안 오면(연결 문제) 서버가 잰 시간으로 끝낸다. → 스킬이 영원히 멈추지 않는다.
	World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::OnServerTimeout,
		MaxChargeTime + LOAChargeRelease::ServerGraceTime, false);

	// 이미 와 있으면 바로 OnTargetDataReplicated가 불린다.
	const bool bAlreadyArrived = AbilitySystemComponent->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationKey);
	if (!bAlreadyArrived)
	{
		SetWaitingOnRemotePlayerData();
	}
}

// =============================================================================
// 조종하는 사람 컴퓨터
// =============================================================================

void ULOAAbilityTask_WaitChargeRelease::OnInputReleased()
{
	FinishLocal();
}

void ULOAAbilityTask_WaitChargeRelease::FinishLocal()
{
	if (bFinished)
	{
		return;
	}

	const float HeldTime = FMath::Clamp(GetElapsed(), 0.f, MaxChargeTime);

	// 원격 클라이언트면 서버에 누른 시간을 알린다. (리슨 호스트 / 스탠드얼론은 자기가 서버라 보낼 필요 없음)
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (ActorInfo && !ActorInfo->IsNetAuthority() && AbilitySystemComponent.IsValid())
	{
		FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());

		FLOATargetData_ChargeTime* Data = new FLOATargetData_ChargeTime();
		Data->HeldTime = HeldTime;

		FGameplayAbilityTargetDataHandle DataHandle;
		DataHandle.Add(Data);	// Handle이 메모리 관리를 맡는다 (직접 delete하지 않음)

		AbilitySystemComponent->ServerSetReplicatedTargetData(
			GetAbilitySpecHandle(),
			GetActivationPredictionKey(),
			DataHandle,
			FGameplayTag(),
			AbilitySystemComponent->ScopedPredictionKey);
	}

	// 내 컴퓨터는 서버 답을 기다리지 않고 바로 발사한다.
	// (GAS 예측 키로 되돌릴 것은 없다: 배율 계산과 몽타주 점프는 GE/Cue가 아니라서 서버가 거부해도 되돌릴 대상이 아님)
	BroadcastAndEnd(HeldTime, false);
}

// =============================================================================
// 서버
// =============================================================================

void ULOAAbilityTask_WaitChargeRelease::OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag)
{
	// 받은 데이터를 "처리 완료"로 표시해서 ASC 보관함에서 비운다.
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());

	// 클라이언트가 보낸 값. 형식이 다르면(있을 수 없지만) 서버가 잰 시간을 쓴다.
	float HeldTime = GetElapsed();
	const FGameplayAbilityTargetData* Data = DataHandle.Get(0);
	if (Data && Data->GetScriptStruct() == FLOATargetData_ChargeTime::StaticStruct())
	{
		HeldTime = static_cast<const FLOATargetData_ChargeTime*>(Data)->HeldTime;
	}

	// 범위 제한: 0 ~ min(최대 시간, 서버가 잰 시간 + 허용치)
	const float UpperBound = FMath::Min(MaxChargeTime, GetElapsed() + LOAChargeRelease::ServerTolerance);
	BroadcastAndEnd(FMath::Clamp(HeldTime, 0.f, UpperBound), false);
}

void ULOAAbilityTask_WaitChargeRelease::OnServerTimeout()
{
	// 클라이언트가 보낸 값이 없다 = 클라이언트의 발사(와 몽타주 점프 RPC)도 오지 않을 수 있다 → true로 알린다.
	BroadcastAndEnd(FMath::Clamp(GetElapsed(), 0.f, MaxChargeTime), true);
}

// =============================================================================
// 공용
// =============================================================================

float ULOAAbilityTask_WaitChargeRelease::GetElapsed() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - StartTime) : 0.f;
}

void ULOAAbilityTask_WaitChargeRelease::BroadcastAndEnd(float HeldTime, bool bServerTimedOut)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	// Ability가 이미 끝났으면(취소 등) 알리지 않는다.
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnReleased.Broadcast(HeldTime, bServerTimedOut);
	}

	EndTask();
}

void ULOAAbilityTask_WaitChargeRelease::OnDestroy(bool bInOwnerFinished)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}

	if (AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
		const FPredictionKey ActivationKey = GetActivationPredictionKey();

		if (InputReleasedHandle.IsValid())
		{
			AbilitySystemComponent->AbilityReplicatedEventDelegate(
				EAbilityGenericReplicatedEvent::InputReleased, SpecHandle, ActivationKey).Remove(InputReleasedHandle);
		}
		if (TargetDataHandle.IsValid())
		{
			AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationKey).Remove(TargetDataHandle);
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}
