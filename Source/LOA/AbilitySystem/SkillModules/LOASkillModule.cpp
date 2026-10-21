// LOASkillModule.cpp

#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "Core/LOALog.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Animation/AnimMontage.h"
#include "Animation/LOAAnimNotify_SendGameplayEvent.h"
#include "GameplayEffect.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"
#include "AbilitySystem/Executions/LOADamageExecution.h"
#include "Core/LOAGameplayTags.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillModule)

void ULOASkillModule::ActivateModule(ULOAGameplayAbility* InOwnerAbility)
{
	if (!InOwnerAbility)
	{
		return;
	}

	// ① 주인을 먼저 기억해야 아래 함수들 안에서 GetOwnerAbility()를 쓸 수 있다.
	OwnerAbility = InOwnerAbility;

	// ② 자식의 발동마다 초기화
	SkillModuleInit();

	// ③ 언제 실행할지 (바로 / 이벤트 대기)
	RunOrWaitByTrigger();
}

void ULOASkillModule::DeactivateModule(bool bWasCancelled)
{
	// 시작하지 않은 모듈(ActivateModule 전에 Ability가 끝남)은 정리할 것도 없다.
	if (!OwnerAbility.IsValid())
	{
		return;
	}

	SkillModuleEnd(bWasCancelled);

	// 스킬이 끝났으니 주인을 잊는다. (다음 발동에서 ActivateModule이 다시 채운다)
	// 잊은 뒤에 늦게 도착한 이벤트는 OnTriggerEventReceived에서 무시된다.
	OwnerAbility.Reset();
}

void ULOASkillModule::RunOrWaitByTrigger()
{
	ULOAGameplayAbility* Owner = GetOwnerAbility();
	if (!Owner)
	{
		return;
	}

	switch (Trigger)
	{
	case ELOASkillModuleTrigger::OnSkillStart:
		// 기다릴 것 없이 바로 1번
		SkillModuleTriggered(nullptr);
		break;

	case ELOASkillModuleTrigger::OnEvent:
	{
		// 태그를 깜빡 비움 → 즉시 실행하지 않고 알리기만 한다. (몽타주는 나가고 이 모듈만 동작 안 함)
		if (!TriggerEventTag.IsValid())
		{
			LOA_LOG(Owner->GetAvatarActorFromActorInfo(), Warning,
				TEXT("[%s] %s: Trigger is On Event but Trigger Event Tag is empty. Module does nothing."),
				*GetNameSafe(Owner->GetClass()), *GetNameSafe(GetClass()));
			break;
		}

		// 이벤트 대기. Task 주인 = Owner Ability → Ability가 끝나면 엔진이 이 Task를 정리한다.
		//   OnlyTriggerOnce = false : 같은 태그 이벤트가 여러 번 오면(다단 히트) 올 때마다 호출
		//   OnlyMatchExact  = true  : 정확히 이 태그만 (하위 태그는 무시)
		UAbilityTask_WaitGameplayEvent* WaitTask =
			UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(Owner, TriggerEventTag, nullptr, false, true);
		WaitTask->EventReceived.AddDynamic(this, &ThisClass::OnTriggerEventReceived);
		WaitTask->ReadyForActivation();
		break;
	}

	default:
		break;
	}
}

void ULOASkillModule::OnTriggerEventReceived(FGameplayEventData EventData)
{
	// 이미 꺼진 모듈이면 무시 (Ability 종료와 같은 프레임에 이벤트가 겹친 경우 대비)
	if (!OwnerAbility.IsValid())
	{
		return;
	}

	SkillModuleTriggered(&EventData);
}

ULOAGameplayAbility* ULOASkillModule::GetOwnerAbility() const
{
	return OwnerAbility.Get();
}

bool ULOASkillModule::HasSkillModuleAuthority() const
{
	const ULOAGameplayAbility* Owner = GetOwnerAbility();
	return Owner && Owner->HasSkillModuleAuthority();
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOASkillModule::ValidateSkillModule(
	FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	// 메시지에 쓸 모듈 이름 (드롭다운에 보이는 이름. 예: Apply)
	const FString ModuleName = GetClass()->GetDisplayNameText().ToString();

	if (Trigger != ELOASkillModuleTrigger::OnEvent)
	{
		return Result;
	}

	// ① On Event인데 기다릴 태그가 없음 → 실행 중에는 경고만 뜨고 아무것도 안 함
	if (!TriggerEventTag.IsValid())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s %s : Trigger is On Event but Trigger Event Tag is empty."), *Prefix, *ModuleName)));
		return EDataValidationResult::Invalid;
	}

	// ② 몽타주 노티파이가 보내는 태그(Event.Montage.*)라면, 몽타주에 그 노티파이가 실제로 있는지 확인
	//    GA가 코드로 보내는 태그(Event.Skill.*)는 몽타주에 없는 게 정상이라 건너뛴다.
	const bool bIsMontageEvent = TriggerEventTag.ToString().StartsWith(TEXT("Event.Montage."));
	if (bIsMontageEvent && OwnerMontage)
	{
		bool bFound = false;
		for (const FAnimNotifyEvent& NotifyEvent : OwnerMontage->Notifies)
		{
			const ULOAAnimNotify_SendGameplayEvent* SendEvent = Cast<ULOAAnimNotify_SendGameplayEvent>(NotifyEvent.Notify);
			if (SendEvent && SendEvent->GetEventTag() == TriggerEventTag)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			// 경고: 몽타주를 나중에 고칠 수도 있으니 저장은 막지 않는다.
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s %s : Montage %s has no '%s' notify (LOA Send Gameplay Event). This module will never run."),
				*Prefix, *ModuleName, *GetNameSafe(OwnerMontage), *TriggerEventTag.ToString())));
		}
	}

	return Result;
}

EDataValidationResult ULOASkillModule::ValidateSkillModulePayloads(
	FDataValidationContext& Context, const TArray<FLOAEffectPayload>& Payloads, const FString& Name)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	for (int32 Index = 0; Index < Payloads.Num(); ++Index)
	{
		const FLOAEffectPayload& Payload = Payloads[Index];

		if (!Payload.Effect)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Payloads[%d] Effect is not set."), *Name, Index)));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		// 태그를 안 고른 칸 (적용할 때 건너뛰므로 동작은 하지만 실수일 가능성이 큼)
		bool bHasDamageRate = false;
		for (const FLOASetByCallerValue& Value : Payload.Values)
		{
			if (!Value.Tag.IsValid())
			{
				Context.AddWarning(FText::FromString(FString::Printf(
					TEXT("%s : Payloads[%d] has a Value with no tag. It will be ignored."), *Name, Index)));
			}
			else if (Value.Tag == LOAGameplayTags::SetByCaller_DamageRate)
			{
				bHasDamageRate = true;
			}
		}

		// 데미지 계산식을 쓰는 GE인데 배율이 없으면 데미지가 0이 된다.
		const UGameplayEffect* EffectCDO = Payload.Effect->GetDefaultObject<UGameplayEffect>();
		bool bUsesDamageExecution = false;
		if (EffectCDO)
		{
			for (const FGameplayEffectExecutionDefinition& Execution : EffectCDO->Executions)
			{
				if (Execution.CalculationClass && Execution.CalculationClass->IsChildOf(ULOADamageExecution::StaticClass()))
				{
					bUsesDamageExecution = true;
					break;
				}
			}
		}

		if (bUsesDamageExecution && !bHasDamageRate)
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s : Payloads[%d] %s uses LOADamageExecution but has no SetByCaller.DamageRate. Damage will be 0."),
				*Name, Index, *GetNameSafe(Payload.Effect))));
		}
	}

	return Result;
}
#endif
