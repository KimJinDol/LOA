// LOAGA_Death.cpp

#include "AbilitySystem/Abilities/LOAGA_Death.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Death)

ULOAGA_Death::ULOAGA_Death(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버만 실행한다. 클라이언트가 발동을 요청할 수도 없다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	
	// 발동 조건: Event.Trigger.Death 게임플레이 이벤트 (ULOAAttributeSet_Common이 보냄)
	FAbilityTriggerData DeathTrigger;
	DeathTrigger.TriggerTag = LOAGameplayTags::Event_Trigger_Death;
	DeathTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(DeathTrigger);

	// 부모가 넣은 "죽어 있으면 발동 불가"를 이 Ability만 뺀다.
	ActivationBlockedTags.RemoveTag(LOAGameplayTags::State_Body_Dead);

	// 어떤 Ability인가 (2026-11-09)
	SetAssetTags(FGameplayTagContainer(LOAGameplayTags::Ability_Common_Death));
}

void ULOAGA_Death::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility는 부르지 않는다. (ULOAPlayerAbility와 같음)
	// 엔진 기본 구현은 BP 그래프(K2_ActivateAbility)를 실행하는 일만 한다. 사망 흐름은 여기 한 곳에서만 정한다.

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Death] Activated (killer: %s)"),
		TriggerEventData ? *GetNameSafe(TriggerEventData->Instigator.Get()) : TEXT("unknown"));

	// ① 하던 일 멈추기 (패턴, 스킬, 차지 등. AbilityTagsToKeep은 남김)
	CancelOtherAbilities(AbilityTagsToKeep);

	// ①-1 버프·디버프 지우기 (사용자 결정 2026-11-07: 죽으면 다 지워짐. 보호막도 GE라 같이)
	ClearBuffsOnDeath();

	// ② 죽음 상태 태그. 몽타주보다 먼저 (헤더 [흐름] 참고)
	//    State.Dead는 항상, 그 밖의 태그는 BP 칸에서. 시간 없음 = 지울 때까지 (지우는 쪽 = 부활. 태그로 찾아서 지운다)
	FGameplayTagContainer DeadTags = AdditionalDeadTags;
	DeadTags.AddTag(LOAGameplayTags::State_Body_Dead);
	ApplyStateTagsToSelf(DeadTags);

	// ③ 스킬 모듈 (있으면). 몽타주 노티파이를 기다리는 모듈이 몽타주보다 먼저 등록되도록
	ActivateSkillModules();

	// 모듈 시작 중에 끝났으면(설정 오류 등) 더 하지 않는다.
	if (!IsActive())
	{
		return;
	}

	// ④ 사망 몽타주 → 끝나면 종료. 몽타주가 없으면 바로 종료 (State.Dead는 남는다)
	if (!PlayMontageAndEnd())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void ULOAGA_Death::ClearBuffsOnDeath() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	// State.Buff.* / State.Debuff.* 를 주는 GE 전부 (부모 태그로 찾으면 자식 태그도 걸린다: MakeQuery_MatchAnyOwningTags)
	// 예) 공격력 버프(State.Buff.AttackPower), 보호막(State.Buff.Shield), 낙인(State.Debuff.Mark)
	// 보호막도 보호막 GE의 몫이라 GE가 지워지면 Shield가 저절로 0이 된다 (기본값은 0)
	// State.Body.Dead·CC(State.CrowdControl.*)는 이 아래가 아니라서 남는다. 죽어도 남는 효과도 이 아래에 두지 않는다
	FGameplayTagContainer TagsToRemove;
	TagsToRemove.AddTag(LOAGameplayTags::State_Buff);
	TagsToRemove.AddTag(LOAGameplayTags::State_Debuff);
	const int32 NumRemoved = ASC->RemoveActiveEffectsWithGrantedTags(TagsToRemove);

	if (NumRemoved > 0)
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Death] %d buff/debuff effect(s) removed"), NumRemoved);
	}
}
