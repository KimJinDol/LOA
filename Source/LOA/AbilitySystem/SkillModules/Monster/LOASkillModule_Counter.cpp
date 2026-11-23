// LOASkillModule_Counter.cpp

#include "AbilitySystem/SkillModules/Monster/LOASkillModule_Counter.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/LOAGameplayEffectContext.h"
#include "AbilitySystem/Effects/LOAGE_Stun.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Core/LOANetTime.h"

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

void ULOASkillModule_Counter::SkillModuleInit()
{
	// 이전 발동의 기록은 지난 일이다. (모듈은 Ability와 함께 재사용된다)
	CounterWindows.Reset();

	if (!HasSkillModuleAuthority())
	{
		return;
	}
	ULOAGameplayAbility* Owner = GetOwnerAbility();
	UAbilitySystemComponent* MyASC = Owner ? Owner->GetAbilitySystemComponentFromActorInfo() : nullptr;
	if (!MyASC)
	{
		return;
	}

	// 파란 구간이 열리고 닫힌 시각을 적는다. 늦게 도착한 보고를 "B 화면의 시각"에 열려 있었나로 보려고
	CounterableTagChangedHandle = MyASC->RegisterGameplayTagEvent(LOAGameplayTags::State_Window_Counterable, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnCounterableTagChanged);

	// 패턴 시작 때 이미 열려 있으면 지금 열린 것으로 친다
	const int32 Count = MyASC->GetTagCount(LOAGameplayTags::State_Window_Counterable);
	if (Count > 0)
	{
		OnCounterableTagChanged(LOAGameplayTags::State_Window_Counterable, Count);
	}
}

void ULOASkillModule_Counter::SkillModuleEnd(bool bWasCancelled)
{
	if (CounterableTagChangedHandle.IsValid())
	{
		ULOAGameplayAbility* Owner = GetOwnerAbility();
		if (UAbilitySystemComponent* MyASC = Owner ? Owner->GetAbilitySystemComponentFromActorInfo() : nullptr)
		{
			MyASC->UnregisterGameplayTagEvent(CounterableTagChangedHandle, LOAGameplayTags::State_Window_Counterable, EGameplayTagEventType::NewOrRemoved);
		}
		CounterableTagChangedHandle.Reset();
	}
	CounterWindows.Reset();
}

void ULOASkillModule_Counter::OnCounterableTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 서버 시각 (공격 판정 기록의 시각, 보스 위치 기록과 같은 시계)
	const ULOAGameplayAbility* Owner = GetOwnerAbility();
	const AActor* Me = Owner ? Owner->GetAvatarActorFromActorInfo() : nullptr;
	const double Now = LOANetTime::GetServerTimeNow(Me ? Me->GetWorld() : nullptr);
	const bool bLastOpen = CounterWindows.Num() > 0 && CounterWindows.Last().CloseTime < 0.0;

	if (NewCount > 0 && !bLastOpen)
	{
		// 열림
		FLOACounterWindow& Window = CounterWindows.AddDefaulted_GetRef();
		Window.OpenTime = Now;
	}
	else if (NewCount <= 0 && bLastOpen)
	{
		// 닫힘
		CounterWindows.Last().CloseTime = Now;
	}
}

bool ULOASkillModule_Counter::WasCounterableAt(double ServerTime) const
{
	// 열린 시각 이상, 닫힌 시각 미만 (닫힌 프레임에는 이미 태그가 없다)
	for (const FLOACounterWindow& Window : CounterWindows)
	{
		if (Window.OpenTime <= ServerTime && (Window.CloseTime < 0.0 || ServerTime < Window.CloseTime))
		{
			return true;
		}
	}
	return false;
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

	// 공격 판정 기록: "맞았다" 신호의 쪽지에 온다 (데미지 GE와 같은 기록. 계산은 라이브러리가 이미 했다)
	const FLOAGameplayEffectContext* EventContext = FLOAGameplayEffectContext::Get(EventData->ContextHandle);
	if (!EventContext)
	{
		LOA_LOG(Me, Warning, TEXT("[Counter] Hit event from %s has no judge record. Check the Ability System Globals setting."), *GetNameSafe(Attacker));
		return;
	}
	const FLOAAttackJudgeRecord& Record = EventContext->GetAttackJudgeRecord();
	const double JudgeAge = LOANetTime::GetServerTimeNow(Me->GetWorld()) - Record.ServerTime;	// 몇 초 전 기준으로 봤나 (로그)

	// ② 그 시각에 파란 구간이었나 (LOA Grant Tags가 붙인 State.Counterable의 열림 / 닫힘 기록)
	if (!WasCounterableAt(Record.ServerTime))
	{
		LOA_LOG(Me, Log, TEXT("[Counter] %s used a counter attack outside the counter window (judged %.3fs ago). No counter."),
			*GetNameSafe(Attacker), JudgeAge);
		return;
	}

	// ③ 정면에서 쳤나 (공격 방향 Front = 헤드 어택과 같은 범위)
	if (Record.AttackSide != ELOAAttackSide::Front)
	{
		LOA_LOG(Me, Log, TEXT("[Counter] %s hit during the counter window but not from the front (attack side %s, judged %.3fs ago). No counter."),
			*GetNameSafe(Attacker), *UEnum::GetValueAsString(Record.AttackSide), JudgeAge);
		return;
	}

	LOA_LOG(Me, Log, TEXT("[Counter] %s countered by %s (judged %.3fs ago, window open now = %s). Stun %.1fs."),
		*GetNameSafe(Owner->GetClass()), *GetNameSafe(Attacker), JudgeAge,
		MyASC->HasMatchingGameplayTag(LOAGameplayTags::State_Window_Counterable) ? TEXT("yes") : TEXT("no"), StunDuration);

	// 카운터 성공 알림 (지금은 듣는 곳 없음. 나중에 "카운터!" 글씨·소리)
	FGameplayEventData CounterPayload;
	CounterPayload.EventTag = LOAGameplayTags::Event_Combat_Counter;
	CounterPayload.Instigator = Attacker;
	CounterPayload.Target = Me;
	MyASC->HandleGameplayEvent(LOAGameplayTags::Event_Combat_Counter, &CounterPayload);

	// 스턴 GE (2026-11-10: 이벤트 → GE). 스턴 태그 → ULOAGA_Stun이 이 패턴을 취소한다. (그래서 이 모듈도 여기서 끝난다)
	if (!StunEffect)
	{
		LOA_LOG(Me, Warning, TEXT("[Counter] Stun Effect is empty. Set GE_CC_Stun on the Counter module."));
		return;
	}

	FGameplayEffectContextHandle StunContext = MyASC->MakeEffectContext();
	// 이벤트 데이터는 const로 오지만 컨텍스트는 약한 참조로 들고만 있다 (액터를 고치지 않음)
	AActor* StunInstigator = const_cast<AActor*>(Attacker);
	StunContext.AddInstigator(StunInstigator, StunInstigator);
	const FGameplayEffectSpecHandle StunSpec = MyASC->MakeOutgoingSpec(StunEffect, 1.f, StunContext);
	if (!StunSpec.IsValid())
	{
		return;
	}
	StunSpec.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, StunDuration);
	MyASC->ApplyGameplayEffectSpecToSelf(*StunSpec.Data);

	if (!MyASC->HasMatchingGameplayTag(LOAGameplayTags::State_CrowdControl_Status_Stunned))
	{
		LOA_LOG(Me, Warning, TEXT("[Counter] Stun Effect applied but no stun tag. Check its Grant Tags / Ignore Tags."));
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

	// 스턴 GE가 없으면 카운터가 성공해도 스턴이 안 걸린다
	if (!StunEffect)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Stun Effect is empty. Set GE_CC_Stun. Otherwise a successful counter does not stun."), *Name)));
	}

	// 몽타주가 없으면 파란 구간(LOA Grant Tags, State.Window.Counterable)을 칠할 곳이 없다
	if (!OwnerMontage)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : The ability has no montage. Add a montage with a LOA Grant Tags (State.Window.Counterable) window."), *Name)));
	}

	return Result;
}
#endif
