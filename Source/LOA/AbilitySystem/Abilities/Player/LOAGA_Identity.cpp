// LOAGA_Identity.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_Identity.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "AbilitySystem/Effects/LOAGE_IdentityDrain.h"
#include "AbilitySystem/Effects/LOAGE_IdentityGain.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Identity)

ULOAGA_Identity::ULOAGA_Identity(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 방향 없음, 이동·다른 스킬을 막지 않음 (켜 둔 채로 계속 싸운다)
	bFaceCursor = false;
	bBlockMove = false;
	bBlockAbility = false;

	// 후딜에 이동해도 끄지 않는다 (켜진 동안 = Ability가 살아 있는 동안. 끝나면 오라도 꺼짐)
	bEndOnRecoveryMove = false;

	// CC(띄움·경직·스턴)를 맞아도 끄지 않는다 (2026-11-09). 공용 CC GA가 AbilityTagsToKeep으로 이 성질을 남긴다.
	// 사망은 그대로 끈다. BP에서 Asset Tags를 덮어쓰면 이 태그도 BP에 넣어야 한다
	FGameplayTagContainer IdentityAssetTags = GetAssetTags();
	IdentityAssetTags.AddTag(LOAGameplayTags::Ability_Trait_KeepOnCC);
	SetAssetTags(IdentityAssetTags);
}

// =============================================================================
// 죽으면 게이지 0 (꺼져 있어도)
// =============================================================================

void ULOAGA_Identity::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// 게이지는 서버만 바꾼다. InstancedPerActor라 이 인스턴스는 Ability를 뺄 때까지 살아 있다
	//   서버: GiveAbility가 인스턴스를 먼저 만들고 그 인스턴스로 이 함수를 부른다 (엔진 AbilitySystemComponent_Abilities.cpp)
	//   클라: 복제된 Spec은 인스턴스 없이 CDO로 불릴 수 있다 → IsInstantiated로 거른다 (CDO에 묶으면 모든 플레이어가 공유)
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && IsInstantiated() && ActorInfo->IsNetAuthority() && !DeadTagChangedHandle.IsValid())
	{
		DeadTagChangedHandle = ASC->RegisterGameplayTagEvent(LOAGameplayTags::State_Body_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnDeadTagChanged);
	}
}

void ULOAGA_Identity::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && DeadTagChangedHandle.IsValid())
	{
		ASC->UnregisterGameplayTagEvent(DeadTagChangedHandle, LOAGameplayTags::State_Body_Dead, EGameplayTagEventType::NewOrRemoved);
	}
	DeadTagChangedHandle.Reset();

	Super::OnRemoveAbility(ActorInfo, Spec);
}

void ULOAGA_Identity::OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 떨어짐(부활) → 할 일 없음
	if (NewCount <= 0)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	// 보통 사망 GA가 먼저 이 Ability를 취소해서 꺼진 뒤다.
	// 켜진 채라도 게이지 0 → OnGaugeChanged가 다음 프레임에 끈다
	bool bFound = false;
	const float Gauge = ASC->GetGameplayAttributeValue(ULOAAttributeSet_Player::GetIdentityGaugeAttribute(), bFound);
	if (bFound && Gauge > 0.f)
	{
		ULOAGE_IdentityGain::ApplyGaugeDelta(ASC, -Gauge);
		LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Identity] Dead -> gauge %.1f -> 0"), Gauge);
	}
}

bool ULOAGA_Identity::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return false;
	}

	// 게이지가 가득일 때만 (최대가 0이면 이 직업은 아이덴티티 게이지가 없음 → 못 켬)
	bool bFoundGauge = false;
	bool bFoundMax = false;
	const float Gauge = ASC->GetGameplayAttributeValue(ULOAAttributeSet_Player::GetIdentityGaugeAttribute(), bFoundGauge);
	const float MaxGauge = ASC->GetGameplayAttributeValue(ULOAAttributeSet_Player::GetMaxIdentityGaugeAttribute(), bFoundMax);
	if (!bFoundGauge || !bFoundMax || MaxGauge <= 0.f || Gauge < MaxGauge)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailCostTag);
		}
		return false;
	}
	return true;
}

void ULOAGA_Identity::StartSkill()
{
	bToggledOff = false;
	ActivatedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// 켜는 몽타주. 끝나도 Ability는 계속 (켜져 있는 동안 = Ability가 살아 있는 동안)
	if (UAnimMontage* MontageToPlay = GetMontage())
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, MontageToPlay, GetMontagePlayRate(), NAME_None, /*bStopWhenAbilityEnds*/ false);
		// 몽타주가 어떻게 끝나든 잠금이 남지 않게 (Unlock 노티파이를 이미 받았으면 아무것도 안 함)
		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnStartMontageEnded);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnStartMontageEnded);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnStartMontageEnded);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnStartMontageEnded);
		MontageTask->ReadyForActivation();

		// 켜는 몽타주 동안만 피격 이상·상태 이상 면역
		AddStartImmunity();
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && HasAuthority(&CurrentActivationInfo))
	{
		// ① 감소 GE: 0.1초마다 −(초당 감소량 × 0.1) + 켜져 있음 태그
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ULOAGE_IdentityDrain::StaticClass(), GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_IdentityGain, -DrainPerSecond * ULOAGE_IdentityDrain::DrainPeriod);
			SpecHandle.Data->DynamicGrantedTags.AddTag(LOAGameplayTags::State_Identity_Active);
			DrainEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
		}

		// ② 게이지 감시 (0이면 끔)
		GaugeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Player::GetIdentityGaugeAttribute())
			.AddUObject(this, &ThisClass::OnGaugeChanged);

		// ③ 직업별 켜진 동안의 일
		OnIdentityOn();

		LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Identity] On (%s)"), *GetNameSafe(GetClass()));
	}

	// ④ 끄기 키 (서버가 처리. 클라는 기다리기만 하고 서버의 종료를 받는다)
	WaitToggleInput();
}

void ULOAGA_Identity::OnStartMontageEnded()
{
	RemoveStartImmunity();
	EnterRecovery();
}

// =============================================================================
// 켜는 몽타주 동안 면역
// =============================================================================

namespace LOAIdentity
{
	/** 켜는 몽타주 동안 붙는 면역 (모든 직업 공통, C++ 고정) */
	static FGameplayTagContainer MakeStartImmuneTags()
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(LOAGameplayTags::State_Immune_Push);	// 피격 이상 (띄움·다운·경직)
		Tags.AddTag(LOAGameplayTags::State_Immune_Status);	// 상태 이상 (기절·빙결·공포)
		return Tags;
	}
}

void ULOAGA_Identity::AddStartImmunity()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bStartImmunityAdded)
	{
		return;
	}

	ASC->AddLooseGameplayTags(LOAIdentity::MakeStartImmuneTags());
	bStartImmunityAdded = true;
}

void ULOAGA_Identity::RemoveStartImmunity()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !bStartImmunityAdded)
	{
		return;
	}

	ASC->RemoveLooseGameplayTags(LOAIdentity::MakeStartImmuneTags());
	bStartImmunityAdded = false;
}

void ULOAGA_Identity::WaitToggleInput()
{
	UAbilityTask_WaitInputPress* PressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, /*bTestAlreadyPressed*/ false);
	PressTask->OnPress.AddDynamic(this, &ThisClass::OnTogglePressed);
	PressTask->ReadyForActivation();
}

void ULOAGA_Identity::OnTogglePressed(float TimeWaited)
{
	if (!IsActive() || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	// 켠 지 너무 이르면 무시하고 다음 누름을 기다림
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : ActivatedTime;
	if (Now - ActivatedTime < ToggleLockTime)
	{
		WaitToggleInput();
		return;
	}

	bToggledOff = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void ULOAGA_Identity::OnGaugeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue > 0.f || !IsActive())
	{
		return;
	}

	// 감소 GE가 적용되는 도중일 수 있다 → 다음 프레임에 끈다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActive())
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
			}
		}));
	}
}

void ULOAGA_Identity::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	// 몽타주 도중에 끝나도 면역이 남지 않게 (서버·내 컴퓨터 모두)
	RemoveStartImmunity();

	// 감소 GE가 있다 = 서버에서 켜져 있었다. 두 번 불려도 한 번만 정리
	if (ASC && DrainEffectHandle.IsValid())
	{
		OnIdentityOff();

		if (GaugeChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Player::GetIdentityGaugeAttribute()).Remove(GaugeChangedHandle);
			GaugeChangedHandle.Reset();
		}

		ASC->RemoveActiveGameplayEffect(DrainEffectHandle);
		DrainEffectHandle.Invalidate();

		// 감소 GE를 뗀 뒤에 게이지 정리 (켜져 있음 태그가 사라진 뒤)
		bool bFound = false;
		const float Gauge = ASC->GetGameplayAttributeValue(ULOAAttributeSet_Player::GetIdentityGaugeAttribute(), bFound);
		// 키로 끔 → 남은 × 환급률. 죽음 → 0은 사망 태그가 붙을 때 (OnDeadTagChanged)
		if (bFound && Gauge > 0.f && bToggledOff)
		{
			ULOAGE_IdentityGain::ApplyGaugeDelta(ASC, Gauge * RefundRate - Gauge);
		}

		LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Identity] Off (%s, gauge %.1f -> %.1f)"),
			bToggledOff ? TEXT("key") : (bWasCancelled ? TEXT("cancelled") : TEXT("empty")),
			Gauge, ASC->GetNumericAttribute(ULOAAttributeSet_Player::GetIdentityGaugeAttribute()));
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
