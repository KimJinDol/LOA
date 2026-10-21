// LOAAbilityTask_ComboInput.cpp

#include "AbilitySystem/Tasks/LOAAbilityTask_ComboInput.h"
#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAbilityTask_ComboInput)

ULOAAbilityTask_ComboInput* ULOAAbilityTask_ComboInput::ListenComboInput(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<ULOAAbilityTask_ComboInput>(OwningAbility);
}

void ULOAAbilityTask_ComboInput::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;

	// 키 입력은 조종하는 사람 컴퓨터에만 있다. 서버(원격 플레이어)에서는 할 일이 없다.
	if (!ActorInfo || !ActorInfo->IsLocallyControlled() || !AbilitySystemComponent.IsValid())
	{
		EndTask();
		return;
	}

	const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
	const FPredictionKey ActivationKey = GetActivationPredictionKey();

	// 발동 키를 누른 상태로 시작한다.
	bInputHeld = true;

	// 이 스킬, 이번 발동의 "누름 / 뗌" 보관함 칸을 구독한다.
	InputPressedHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(
		EAbilityGenericReplicatedEvent::InputPressed, SpecHandle, ActivationKey)
		.AddUObject(this, &ThisClass::OnInputPressed);

	InputReleasedHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(
		EAbilityGenericReplicatedEvent::InputReleased, SpecHandle, ActivationKey)
		.AddUObject(this, &ThisClass::OnInputReleased);

	// Task가 시작되기 전에 이미 뗐거나 다시 눌렀으면 기록이 남아 있다 → 지금 처리한다.
	// 순서: 뗌 먼저, 누름 나중 (뗐다가 다시 누른 경우 최종 상태가 "누르는 중"이 되도록)
	AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputReleased, SpecHandle, ActivationKey);
	AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputPressed, SpecHandle, ActivationKey);
}

void ULOAAbilityTask_ComboInput::OnInputPressed()
{
	bInputHeld = true;

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnPressed.Broadcast();
	}
}

void ULOAAbilityTask_ComboInput::OnInputReleased()
{
	bInputHeld = false;
}

void ULOAAbilityTask_ComboInput::OnDestroy(bool bInOwnerFinished)
{
	if (AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
		const FPredictionKey ActivationKey = GetActivationPredictionKey();

		if (InputPressedHandle.IsValid())
		{
			AbilitySystemComponent->AbilityReplicatedEventDelegate(
				EAbilityGenericReplicatedEvent::InputPressed, SpecHandle, ActivationKey).Remove(InputPressedHandle);
		}
		if (InputReleasedHandle.IsValid())
		{
			AbilitySystemComponent->AbilityReplicatedEventDelegate(
				EAbilityGenericReplicatedEvent::InputReleased, SpecHandle, ActivationKey).Remove(InputReleasedHandle);
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}
