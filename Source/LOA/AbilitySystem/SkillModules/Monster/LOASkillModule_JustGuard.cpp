// LOASkillModule_JustGuard.cpp

#include "AbilitySystem/SkillModules/Monster/LOASkillModule_JustGuard.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "Combat/LOAJustGuardSubsystem.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillModule_JustGuard)

ULOASkillModule_JustGuard::ULOASkillModule_JustGuard()
{
	// 패턴이 시작하자마자 듣기 시작한다. 이 모듈은 이 설정으로만 동작한다 (바꾸면 저장 시 경고)
	Trigger = ELOASkillModuleTrigger::OnSkillStart;
}

void ULOASkillModule_JustGuard::SkillModuleInit()
{
	// 모듈은 GA와 함께 재사용된다 → 지난 발동 값을 지운다
	NumAttacks = 0;
	NumGuarded = 0;
	NumHit = 0;
	GuardCounts.Reset();
}

void ULOASkillModule_JustGuard::SkillModuleTriggered(const FGameplayEventData* EventData)
{
	if (!HasSkillModuleAuthority())
	{
		return;
	}

	ULOAGameplayAbility* Owner = GetOwnerAbility();
	ULOAJustGuardSubsystem* JustGuardSubsystem = Owner ? ULOAJustGuardSubsystem::Get(Owner->GetAvatarActorFromActorInfo()) : nullptr;
	if (!JustGuardSubsystem)
	{
		return;
	}

	StopListening();
	ResolvedHandle = JustGuardSubsystem->OnJustGuardResolved.AddUObject(this, &ThisClass::OnJustGuardResolved);
	ListenedJustGuardSubsystem = JustGuardSubsystem;
}

void ULOASkillModule_JustGuard::SkillModuleEnd(bool bWasCancelled)
{
	const ULOAGameplayAbility* Owner = GetOwnerAbility();
	const bool bWasListening = ListenedJustGuardSubsystem.IsValid();
	if (bWasListening && Owner)
	{
		LOA_LOG(Owner->GetAvatarActorFromActorInfo(), Log, TEXT("[JustGuard] %s finished%s: %d attack(s), %d guard(s), %d hit(s)"),
			*GetNameSafe(Owner->GetClass()), bWasCancelled ? TEXT(" (cancelled)") : TEXT(""), NumAttacks, NumGuarded, NumHit);
	}
	StopListening();

	// 들었던 발동(서버)만 알린다. 끊겨도 그때까지 센 것으로 (듣는 쪽이 기다리다 멈추지 않게)
	if (bWasListening)
	{
		SendPatternResolved();
	}
}

void ULOASkillModule_JustGuard::SendPatternResolved() const
{
	const ULOAGameplayAbility* Owner = GetOwnerAbility();
	AActor* Boss = Owner ? Owner->GetAvatarActorFromActorInfo() : nullptr;
	if (!Boss)
	{
		return;
	}

	// 성공 = 이 패턴의 저가 타격을 전부 막은 사람. 타격이 없었으면 아무도 성공이 아니다
	int32 SuccessCount = 0;
	if (NumAttacks > 0)
	{
		for (const TPair<TWeakObjectPtr<APawn>, int32>& Pair : GuardCounts)
		{
			if (Pair.Key.IsValid() && Pair.Value >= NumAttacks)
			{
				++SuccessCount;
			}
		}
	}

	LOA_LOG(Boss, Log, TEXT("[JustGuard] %s resolved: %d player(s) guarded all %d attack(s)"),
		*GetNameSafe(Owner->GetClass()), SuccessCount, NumAttacks);

	FGameplayEventData Payload;
	Payload.EventTag = LOAGameplayTags::Event_JustGuard_PatternResolved;
	Payload.Instigator = Boss;
	Payload.Target = Boss;
	Payload.EventMagnitude = static_cast<float>(SuccessCount);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Boss, LOAGameplayTags::Event_JustGuard_PatternResolved, Payload);
}

void ULOASkillModule_JustGuard::OnJustGuardResolved(const FLOAJustGuardResolved& Result)
{
	// 다른 보스·다른 패턴의 공격은 거른다 (내 몸이 친 공격만)
	const ULOAGameplayAbility* Owner = GetOwnerAbility();
	if (!Owner || Result.Caster.Get() != Owner->GetAvatarActorFromActorInfo())
	{
		return;
	}

	++NumAttacks;
	NumGuarded += Result.Guarded.Num();
	NumHit += Result.Hit.Num();

	// 사람마다 막은 타격 수 (패턴 끝에 타격 수와 비교)
	for (const TWeakObjectPtr<APawn>& Guarder : Result.Guarded)
	{
		if (Guarder.IsValid())
		{
			++GuardCounts.FindOrAdd(Guarder);
		}
	}

	LOA_LOG(Owner->GetAvatarActorFromActorInfo(), Log, TEXT("[JustGuard] %s attack #%d: %d guarded, %d hit"),
		*GetNameSafe(Owner->GetClass()), NumAttacks, Result.Guarded.Num(), Result.Hit.Num());
}

void ULOASkillModule_JustGuard::StopListening()
{
	if (ULOAJustGuardSubsystem* JustGuardSubsystem = ListenedJustGuardSubsystem.Get())
	{
		JustGuardSubsystem->OnJustGuardResolved.Remove(ResolvedHandle);
	}
	ResolvedHandle.Reset();
	ListenedJustGuardSubsystem.Reset();
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOASkillModule_JustGuard::ValidateSkillModule(
	FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const
{
	EDataValidationResult Result = Super::ValidateSkillModule(Context, OwnerMontage, Prefix);
	const FString Name = FString::Printf(TEXT("%s Just Guard"), *Prefix);

	// Trigger를 바꾸면 패턴 시작에 구독하지 못한다
	if (Trigger != ELOASkillModuleTrigger::OnSkillStart)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Trigger should be On Skill Start. Otherwise the module never listens."), *Name)));
	}

	// 주인 GA(이 모듈을 담은 BP의 기본값)에 저스트 가드 속성이 없으면 막을 수 있는 공격이 없다 → 셀 것도 없다
	if (const UGameplayAbility* OwnerDefaults = Cast<UGameplayAbility>(GetOuter()))
	{
		if (!OwnerDefaults->GetAssetTags().HasTag(LOAGameplayTags::Skill_Attribute_JustGuard))
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s : the ability has no Skill.Attribute.JustGuard in Asset Tags. Its attacks cannot be just guarded."), *Name)));
		}
	}

	return Result;
}
#endif
