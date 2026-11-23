// LOAGA_Stun.cpp

#include "AbilitySystem/Abilities/LOAGA_Stun.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Stun)

ULOAGA_Stun::ULOAGA_Stun(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버가 발동하고, 플레이어면 그 플레이어 컴퓨터에서도 같이 돈다. 헤더 [네트워크]
	// 보스·몬스터는 주인 컴퓨터가 없어서 서버에서만 돈다
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 발동 조건: 스턴 태그가 있는 동안 (2026-11-10). 붙으면 발동, 사라지면 엔진이 취소한다
	// 태그는 스턴 GE(ULOAGE_Stun)가 준다 → 시간·면역은 GE가 정한다
	FAbilityTriggerData StunTrigger;
	StunTrigger.TriggerTag = LOAGameplayTags::State_CrowdControl_Status_Stunned;
	StunTrigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagPresent;
	AbilityTriggers.Add(StunTrigger);

	// 취소하지 않을 성질 (2026-11-09): 아이덴티티처럼 CC를 맞아도 켜져 있어야 하는 Ability
	AbilityTagsToKeep.AddTag(LOAGameplayTags::Ability_Trait_KeepOnCC);

	// 어떤 Ability인가 (2026-11-09)
	SetAssetTags(FGameplayTagContainer(LOAGameplayTags::Ability_Common_Stun));

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

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Stun] Activated"));

	// ① 하던 일 멈추기 [둘 다] (카운터 당한 패턴, 플레이어 스킬 등. AbilityTagsToKeep은 남김)
	CancelOtherAbilities(AbilityTagsToKeep);

	// ② 그 자리에 선다 [둘 다] (플레이어면 내 컴퓨터에서도 바로 서야 서버 보정으로 끌려가지 않는다)
	StopAvatarMovement();

	// ③ 스턴 몽타주 [둘 다] (없으면 AnimBP가 State.CrowdControl.Status.Stunned를 보고 직접 재생해도 된다)
	PlayMontageOnly();

	// 끝나는 때는 태그가 정한다 (스턴 GE 시간이 끝나 태그가 사라지면 엔진이 취소)
}

void ULOAGA_Stun::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 스턴 GE는 지우지 않는다 (2026-11-10 사용자 결정). 사망·띄움이 이 Ability를 끊어도 스턴 시간은 GE대로 남는다

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Stun] Ended%s"), bWasCancelled ? TEXT(" (cancelled)") : TEXT(""));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
