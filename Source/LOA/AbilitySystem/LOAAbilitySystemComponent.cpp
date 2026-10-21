// LOAAbilitySystemComponent.cpp

#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "TimerManager.h"

ULOAAbilitySystemComponent::ULOAAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 복제 모드(Mixed/Minimal)는 여기서 정하지 않는다.
	// 이 컴포넌트를 "소유하는 쪽"(PlayerState / 보스)이 자기 상황에 맞게 정한다.
}

void ULOAAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// 발동할 스킬을 먼저 모아두고, 반복문이 끝난 뒤에 발동한다.
	// (반복 도중 스킬 목록이 바뀌면 크래시가 날 수 있기 때문)
	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;

	{
		// 반복하는 동안 스킬 목록이 바뀌지 않도록 잠금
		ABILITYLIST_SCOPE_LOCK();

		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			// 스킬을 부여할 때 붙여둔 입력 태그와 비교
			if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}

			if (Spec.IsActive())
			{
				// 이미 실행 중 → 그 스킬에 "눌림"만 알린다. (이 컴퓨터 안에서만. 아래 AbilitySpecInputPressed 참고)
				AbilitySpecInputPressed(Spec);
			}
			else
			{
				AbilitiesToActivate.AddUnique(Spec.Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitiesToActivate)
	{
		// 클라이언트에서 호출하면 GAS가 "로컬 예측 + 서버 요청"을 알아서 처리한다.
		TryActivateAbility(Handle);
	}
}

void ULOAAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		// 뗌은 스킬을 새로 발동시키지 않는다. 실행 중인 스킬에만 알린다.
		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}

void ULOAAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	// 부모: 내 컴퓨터의 스킬에 "눌림" 상태를 기록
	Super::AbilitySpecInputPressed(Spec);

	if (Spec.IsActive())
	{
		// 보관함 칸의 이름표 = (스킬 Handle + 이번 발동의 예측키).
		// Task도 같은 이름표로 구독하므로, 같은 값을 넣어야 "이번 발동"의 Task가 받는다.
		// (예전 발동의 Task나 다른 스킬의 Task는 이름표가 달라서 받지 않는다)
PRAGMA_DISABLE_DEPRECATION_WARNINGS // 구버전 코드인거 알겠으니 이 이하 코드부터 컴파일 시 warning 그만 띄우라는 매크로
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		const FPredictionKey OriginalPredictionKey = Instance
			? Instance->GetCurrentActivationInfo().GetActivationPredictionKey()
			: Spec.ActivationInfo.GetActivationPredictionKey();
PRAGMA_ENABLE_DEPRECATION_WARNINGS // 다시 warning 띄우라는 매크로

		// 이 컴퓨터의 보관함 칸에 "눌림" 기록 + 구독 중인 Task 호출. 서버로는 보내지 않는다.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, OriginalPredictionKey);
	}
}

void ULOAAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		const FPredictionKey OriginalPredictionKey = Instance
			? Instance->GetCurrentActivationInfo().GetActivationPredictionKey()
			: Spec.ActivationInfo.GetActivationPredictionKey();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// 이 컴퓨터의 보관함 칸에 "뗌" 기록 + 구독 중인 Task 호출. 서버로는 보내지 않는다.
		// (서버가 알아야 하면 그 Task가 직접 보낸다. 예: WaitChargeRelease는 누른 시간을 TargetData로 보냄)
		// 이름표(예측키)를 구하는 방법은 위 AbilitySpecInputPressed와 같다.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, OriginalPredictionKey);
	}
}

// =============================================================================
// 스킬 목록 바뀜 알림
// =============================================================================

void ULOAAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	RequestAbilitiesChangedBroadcast();
}

void ULOAAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnRemoveAbility(AbilitySpec);
	RequestAbilitiesChangedBroadcast();   // 지금은 아직 목록에 있다 → 다음 프레임에 알려야 빠진 뒤의 목록이 보인다
}

void ULOAAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	RequestAbilitiesChangedBroadcast();
}

void ULOAAbilitySystemComponent::RequestAbilitiesChangedBroadcast()
{
	// 듣는 사람이 없으면 할 일 없음 (보스, 다른 플레이어의 ASC)
	// 이미 예약했으면 그 한 번으로 충분
	if (!OnAbilitiesChanged.IsBound() || bAbilitiesChangedPending)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bAbilitiesChangedPending = true;

	// CreateWeakLambda: 다음 프레임 전에 이 컴포넌트가 사라지면 부르지 않는다
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bAbilitiesChangedPending = false;
		OnAbilitiesChanged.Broadcast();
	}));
}
