// LOASkillModule_Apply.cpp

#include "AbilitySystem/SkillModules/LOASkillModule_Apply.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "Animation/LOAAnimNotify_SendGameplayEvent.h"
#include "Combat/LOACombatJudgeManagerSubsystem.h"
#include "Core/LOALog.h"

#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillModule_Apply)

void ULOASkillModule_Apply::SkillModuleInit()
{
	ULOAGameplayAbility* Owner = GetOwnerAbility();
	if (!Owner)
	{
		return;
	}

	// BP 설정 누락 알림. (스킬은 그대로 진행된다. 몽타주는 나가고 효과만 없음)
	if (!Targeting)
	{
		LOA_LOG(Owner->GetAvatarActorFromActorInfo(), Warning,
			TEXT("[%s] Apply module: Targeting is not set."), *GetNameSafe(Owner->GetClass()));
	}
	if (Payloads.IsEmpty())
	{
		LOA_LOG(Owner->GetAvatarActorFromActorInfo(), Warning,
			TEXT("[%s] Apply module: Payloads is empty."), *GetNameSafe(Owner->GetClass()));
	}

	// 판정 매니저에 스킬 시작을 알린다 (원격 플레이어의 공격자 판정 공격이면 서버가 이 발동의 보고를 듣기 시작. 아니면 아무것도 안 함)
	if (ULOACombatJudgeManagerSubsystem* JudgeManager = ULOACombatJudgeManagerSubsystem::Get(Owner->GetAvatarActorFromActorInfo()))
	{
		JudgeManager->BeginAttackReports(Owner, this, Targeting, Payloads, CountAllowedReports());
	}
}

void ULOASkillModule_Apply::SkillModuleTriggered(const FGameplayEventData* EventData)
{
	// 조종하는 사람 컴퓨터와 서버에서 각각 호출된다. 이 컴퓨터가 할 일(서버 판정 / 보고 / 기다림)은 판정 매니저가 정한다
	ULOAGameplayAbility* Owner = GetOwnerAbility();
	if (!Owner)
	{
		return;
	}
	if (ULOACombatJudgeManagerSubsystem* JudgeManager = ULOACombatJudgeManagerSubsystem::Get(Owner->GetAvatarActorFromActorInfo()))
	{
		JudgeManager->JudgeInstantHit(Owner, this, Targeting, Payloads);
	}
}

void ULOASkillModule_Apply::SkillModuleEnd(bool bWasCancelled)
{
	// 스킬이 끝나면 늦게 온 보고는 받지 않는다 (예: 서버에서 CC를 먼저 맞아 끊김 → 그 뒤 보고는 데미지 없음)
	ULOAGameplayAbility* Owner = GetOwnerAbility();
	if (!Owner)
	{
		return;
	}
	if (ULOACombatJudgeManagerSubsystem* JudgeManager = ULOACombatJudgeManagerSubsystem::Get(Owner->GetAvatarActorFromActorInfo()))
	{
		JudgeManager->EndAttackReports(Owner, this);
	}
}

int32 ULOASkillModule_Apply::CountAllowedReports() const
{
	// On Skill Start = 한 번
	if (Trigger == ELOASkillModuleTrigger::OnSkillStart)
	{
		return 1;
	}

	// 몽타주 노티파이 이벤트면 그 노티파이 수 (콤보는 모든 섹션의 노티파이를 합친 수)
	const ULOAGameplayAbility* Owner = GetOwnerAbility();
	const UAnimMontage* Montage = Owner ? Owner->GetMontage() : nullptr;
	int32 NumNotifies = 0;
	if (Montage)
	{
		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			const ULOAAnimNotify_SendGameplayEvent* SendEvent = Cast<ULOAAnimNotify_SendGameplayEvent>(NotifyEvent.Notify);
			if (SendEvent && SendEvent->GetEventTag() == TriggerEventTag)
			{
				++NumNotifies;
			}
		}
	}

	// GA가 코드로 보내는 이벤트(Event.Skill.*)처럼 몽타주에 없으면 한 번 (대신 정함 2026-11-02)
	return FMath::Max(NumNotifies, 1);
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOASkillModule_Apply::ValidateSkillModule(
	FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const
{
	// 부모: Trigger / 노티파이 검사
	EDataValidationResult Result = Super::ValidateSkillModule(Context, OwnerMontage, Prefix);

	const FString Name = FString::Printf(TEXT("%s Apply"), *Prefix);

	// ① 누구에게. 종류를 골랐으면 그 종류의 칸 검사도 한다.
	if (!Targeting)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Targeting is not set."), *Name)));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		const FString TargetingPrefix = FString::Printf(TEXT("%s Targeting"), *Name);
		Result = CombineDataValidationResults(Result, Targeting->ValidateTargeting(Context, TargetingPrefix));
	}

	// ② 무엇을
	if (Payloads.IsEmpty())
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Payloads is empty."), *Name)));
		Result = EDataValidationResult::Invalid;
	}

	// ③ Payload 칸마다 (Effect 없음 / 태그 없는 값 / 데미지 배율 누락). 규칙은 부모에 한 곳 (Spawn 모듈과 공용)
	Result = CombineDataValidationResults(Result, ValidateSkillModulePayloads(Context, Payloads, Name));

	return Result;
}
#endif
