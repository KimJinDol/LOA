// LOASkillModule_Apply.cpp

#include "AbilitySystem/SkillModules/LOASkillModule_Apply.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "Combat/LOAHitJudgeSubsystem.h"
#include "Core/LOALog.h"

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
}

void ULOASkillModule_Apply::SkillModuleTriggered(const FGameplayEventData* EventData)
{
	// 조종하는 사람 컴퓨터와 서버에서 각각 호출된다. 판정은 ApplyToTargets 안에서 서버만 한다.
	ApplyToTargets();
}

void ULOASkillModule_Apply::ApplyToTargets()
{
	// 결과가 남는 일은 서버만. (조종하는 사람 컴퓨터: 나중에 Cue 모듈이 이펙트만 예측 재생)
	if (!HasSkillModuleAuthority())
	{
		return;
	}

	ULOAGameplayAbility* Owner = GetOwnerAbility();
	if (!Owner || !Targeting || Payloads.IsEmpty())
	{
		return;
	}

	// ① 누가, 몇 레벨로
	const FLOASkillModuleContext Context = Owner->MakeSkillModuleContext();
	if (!Context.IsValid())
	{
		return;
	}

	// ②③ 누구에게 + 무엇을. 판정 방식(서버 / 피해자)은 판정 담당이 Targeting을 보고 정한다.
	//    Apply는 예고가 없다 → 플레이어는 시전자의 모션을 보고 피한다.
	const int32 NumNow = ULOAHitJudgeSubsystem::JudgeAndApply(Targeting, Context, Payloads, ELOAHitJudgeTiming::Animation);

	LOA_LOG(Context.SourceActor.Get(), Log, TEXT("[Apply] %s : %d target(s) now"), *GetNameSafe(Owner->GetClass()), NumNow);
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
