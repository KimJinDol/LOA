// LOASkillModule_Counter.cpp

#include "AbilitySystem/SkillModules/Monster/LOASkillModule_Counter.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillModule_Counter)

ULOASkillModule_Counter::ULOASkillModule_Counter()
{
	// "맞았다" 신호를 기다린다. 이 모듈은 이 설정으로만 동작한다. (바꾸면 저장 시 경고)
	Trigger = ELOASkillModuleTrigger::OnEvent;
	TriggerEventTag = LOAGameplayTags::Event_Combat_Hit;
}

void ULOASkillModule_Counter::SkillModuleTriggered(const FGameplayEventData* EventData)
{
	// 판정과 스턴은 서버만
	if (!EventData || !HasSkillModuleAuthority())
	{
		return;
	}

	ULOAGameplayAbility* Owner = GetOwnerAbility();
	UAbilitySystemComponent* MyASC = Owner ? Owner->GetAbilitySystemComponentFromActorInfo() : nullptr;
	AActor* Me = Owner ? Owner->GetAvatarActorFromActorInfo() : nullptr;
	const AActor* Attacker = EventData->Instigator.Get();
	if (!MyASC || !Me || !Attacker)
	{
		return;
	}

	// ① 카운터 속성의 공격인가 (일반 공격이면 여기서 끝. 대부분의 공격)
	if (!EventData->InstigatorTags.HasTagExact(LOAGameplayTags::Skill_Attribute_Counter))
	{
		return;
	}

	// ② 지금 파란 구간인가 (몽타주 LOA Grant Tags가 붙인 State.Counterable)
	if (!MyASC->HasMatchingGameplayTag(LOAGameplayTags::State_Counterable))
	{
		return;
	}

	// ③ 헤드에서 쳤나 (때린 사람이 서 있는 위치 기준)
	if (!ULOACombatLibrary::IsHeadAttack(Me, Attacker, HeadHalfAngle))
	{
		LOA_LOG(Me, Log, TEXT("[Counter] %s hit during the counter window but not from the front. No counter."), *GetNameSafe(Attacker));
		return;
	}

	LOA_LOG(Me, Log, TEXT("[Counter] %s countered by %s. Stun %.1fs."),
		*GetNameSafe(Owner->GetClass()), *GetNameSafe(Attacker), StunDuration);

	// 카운터 성공 알림 (지금은 듣는 곳 없음. 나중에 "카운터!" 글씨·소리)
	FGameplayEventData CounterPayload;
	CounterPayload.EventTag = LOAGameplayTags::Event_Combat_Counter;
	CounterPayload.Instigator = Attacker;
	CounterPayload.Target = Me;
	MyASC->HandleGameplayEvent(LOAGameplayTags::Event_Combat_Counter, &CounterPayload);

	// 스턴. ULOAGA_Stun이 받아서 이 패턴을 취소한다. (그래서 이 모듈도 여기서 끝난다)
	FGameplayEventData StunPayload;
	StunPayload.EventTag = LOAGameplayTags::Event_Stun;
	StunPayload.Instigator = Attacker;
	StunPayload.Target = Me;
	StunPayload.EventMagnitude = StunDuration;
	if (MyASC->HandleGameplayEvent(LOAGameplayTags::Event_Stun, &StunPayload) == 0)
	{
		LOA_LOG(Me, Warning, TEXT("[Counter] No ability handled Event.Stun. Add a ULOAGA_Stun BP to the boss AbilityDataSet."));
	}
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOASkillModule_Counter::ValidateSkillModule(
	FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const
{
	// 부모: Trigger 검사
	EDataValidationResult Result = Super::ValidateSkillModule(Context, OwnerMontage, Prefix);

	const FString Name = FString::Printf(TEXT("%s Counter"), *Prefix);

	// Trigger를 바꾸면 "맞았다" 신호를 못 받아서 카운터가 안 된다
	if (Trigger != ELOASkillModuleTrigger::OnEvent || TriggerEventTag != LOAGameplayTags::Event_Combat_Hit)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Trigger should be On Event with Event.Combat.Hit. Otherwise counters are never judged."), *Name)));
	}

	// 몽타주가 없으면 파란 구간(LOA Grant Tags, State.Counterable)을 칠할 곳이 없다
	if (!OwnerMontage)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : The ability has no montage. Add a montage with a LOA Grant Tags (State.Counterable) window."), *Name)));
	}

	return Result;
}
#endif
