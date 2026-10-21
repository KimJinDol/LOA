// LOAAbilityTask_WaitCursorTarget.cpp

#include "AbilitySystem/Tasks/LOAAbilityTask_WaitCursorTarget.h"
#include "AbilitySystemComponent.h"
#include "Library/LOACursorLibrary.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAbilityTask_WaitCursorTarget)

ULOAAbilityTask_WaitCursorTarget* ULOAAbilityTask_WaitCursorTarget::WaitCursorTarget(UGameplayAbility* OwningAbility, ECollisionChannel TraceChannel)
{
	ULOAAbilityTask_WaitCursorTarget* Task = NewAbilityTask<ULOAAbilityTask_WaitCursorTarget>(OwningAbility);
	Task->TraceChannel = TraceChannel;
	return Task;
}

void ULOAAbilityTask_WaitCursorTarget::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ActorInfo || !AbilitySystemComponent.IsValid())
	{
		EndTask();
		return;
	}

	if (ActorInfo->IsLocallyControlled())
	{
		// ---------- 조종하는 사람 컴퓨터 (리슨 서버 호스트 포함) ----------
		SendCursorData();
		return;
	}

	// ---------- 서버 (원격 플레이어의 Ability) ----------
	const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
	const FPredictionKey ActivationKey = GetActivationPredictionKey();

	// "이 발동(예측 키)의 커서 데이터가 도착하면 알려 줘" 등록
	TargetDataDelegateHandle = AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationKey)
		.AddUObject(this, &ThisClass::OnTargetDataReplicated);

	// 데이터가 발동 요청보다 먼저 도착했을 수도 있다. 이미 와 있으면 바로 위 함수가 호출된다.
	const bool bAlreadyArrived = AbilitySystemComponent->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationKey);
	if (!bAlreadyArrived)
	{
		// 아직 안 왔다 → 기다린다. (Ability가 "원격 데이터 대기 중" 상태임을 GAS에 알림)
		SetWaitingOnRemotePlayerData();
	}
}

void ULOAAbilityTask_WaitCursorTarget::SendCursorData()
{
	// 이 범위 안에서 만든 데이터는 Ability 발동과 같은 예측 흐름으로 묶인다.
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());

	FGameplayAbilityTargetDataHandle DataHandle;

	// 커서 위치: ① 트레이스 → 실패하면 ② 가상 바닥 평면 (맵 밖을 가리켜도 그 방향을 바라보도록)
	const APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
	FHitResult Hit;
	if (ULOACursorLibrary::GetCursorHitResult(PC, TraceChannel, Hit))
	{
		// HitResult 하나를 담는 GAS 기본 TargetData 형식
		FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
		Data->HitResult = Hit;
		DataHandle.Add(Data);   // Handle이 메모리 관리를 맡는다 (직접 delete하지 않음)
	}

	// 실패(빈 데이터)여도 반드시 보낸다. 안 보내면 서버가 영원히 기다린다.
	AbilitySystemComponent->ServerSetReplicatedTargetData(
		GetAbilitySpecHandle(),
		GetActivationPredictionKey(),
		DataHandle,
		FGameplayTag(),
		AbilitySystemComponent->ScopedPredictionKey);

	// 내 컴퓨터는 서버 답을 기다리지 않고 바로 진행한다. (예측)
	BroadcastResult(DataHandle);
}

void ULOAAbilityTask_WaitCursorTarget::OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag)
{
	// 받은 데이터를 "처리 완료"로 표시해서 ASC 보관함에서 비운다. (다음 발동과 섞이지 않게)
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());

	BroadcastResult(DataHandle);
}

void ULOAAbilityTask_WaitCursorTarget::BroadcastResult(const FGameplayAbilityTargetDataHandle& DataHandle)
{
	// Ability가 이미 끝났으면(취소 등) 방송하지 않는다.
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (DataHandle.Num() > 0)
		{
			ValidData.Broadcast(DataHandle);
		}
		else
		{
			Failed.Broadcast(DataHandle);
		}
	}

	EndTask();
}

void ULOAAbilityTask_WaitCursorTarget::OnDestroy(bool bInOwnerFinished)
{
	// 서버에서 등록한 알림 해제 (Task가 끝났는데 알림이 남아 있으면 안 됨)
	if (AbilitySystemComponent.IsValid() && TargetDataDelegateHandle.IsValid())
	{
		AbilitySystemComponent->AbilityTargetDataSetDelegate(GetAbilitySpecHandle(), GetActivationPredictionKey())
			.Remove(TargetDataDelegateHandle);
	}

	Super::OnDestroy(bInOwnerFinished);
}

bool ULOAAbilityTask_WaitCursorTarget::GetLocationFromData(const FGameplayAbilityTargetDataHandle& DataHandle, FVector& OutLocation)
{
	const FGameplayAbilityTargetData* Data = DataHandle.Get(0);
	if (!Data || !Data->HasHitResult())
	{
		return false;
	}

	OutLocation = Data->GetHitResult()->Location;
	return true;
}