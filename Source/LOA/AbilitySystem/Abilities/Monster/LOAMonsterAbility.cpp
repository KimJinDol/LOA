// LOAMonsterAbility.cpp

#include "AbilitySystem/Abilities/Monster/LOAMonsterAbility.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAMonsterAbility)

ULOAMonsterAbility::ULOAMonsterAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버만 실행한다. 클라이언트는 보스 회전·몽타주·범위 액터를 복제로 받는다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 한 번에 패턴 하나. 도는 동안 State.Block.Ability를 붙이고, 그 태그가 있으면 다른 패턴은 발동하지 않는다.
	// (플레이어 스킬의 "행동 금지"와 같은 태그. 보스는 서버에서만 쓰므로 서버에만 붙으면 된다)
	ActivationOwnedTags.AddTag(LOAGameplayTags::State_Block_Ability);
	ActivationBlockedTags.AddTag(LOAGameplayTags::State_Block_Ability);
}

// =============================================================================
// 발동 창구
// =============================================================================

bool ULOAMonsterAbility::TryActivatePattern(UAbilitySystemComponent* ASC, FGameplayTag PatternTag, AActor* Target,
	FGameplayAbilitySpecHandle* OutActivatedHandle)
{
	if (!ASC || !PatternTag.IsValid())
	{
		return false;
	}

	AActor* Boss = ASC->GetAvatarActor();

	// 결과가 남는 일이므로 서버만. (클라에서 부르면 아무 일도 없다)
	if (!ASC->IsOwnerActorAuthoritative())
	{
		LOA_LOG(Boss, Warning, TEXT("[Pattern] TryActivatePattern(%s) called on a client. Patterns run on the server only."), *PatternTag.ToString());
		return false;
	}

	// ① 이 태그를 가진 패턴 찾기 (없으면 FindPatternSpec이 경고)
	const FGameplayAbilitySpec* FoundSpec = FindPatternSpec(ASC, PatternTag);
	if (!FoundSpec)
	{
		return false;
	}

	// ② 대상 실어서 발동. (InstancedPerActor: 대상은 이벤트 데이터로 ActivateAbility에 전달된다)
	FGameplayEventData Payload;
	Payload.EventTag = PatternTag;
	Payload.Instigator = Boss;
	Payload.Target = Target;

	const bool bActivated = ASC->TriggerAbilityFromGameplayEvent(
		FoundSpec->Handle, ASC->AbilityActorInfo.Get(), PatternTag, &Payload, *ASC);

	// 실패 이유는 GAS가 알려 주지 않는다. 흔한 것만 적어 둔다. (쿨타임 중, 다른 패턴 중, 죽음, Required인데 대상 없음)
	LOA_LOG(Boss, Log, TEXT("[Pattern] %s -> target %s : %s"),
		*PatternTag.ToString(), *GetNameSafe(Target),
		bActivated ? TEXT("activated") : TEXT("rejected (cooldown / another pattern running / dead / no target)"));

	if (bActivated && OutActivatedHandle)
	{
		*OutActivatedHandle = FoundSpec->Handle;
	}

	return bActivated;
}

bool ULOAMonsterAbility::CanActivatePattern(const UAbilitySystemComponent* ASC, FGameplayTag PatternTag, const AActor* Target)
{
	// 패턴은 서버에서만 쓰므로 확인도 서버에서만 의미가 있다. (StateTree는 서버에만 있다)
	if (!ASC || !PatternTag.IsValid() || !ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	const FGameplayAbilitySpec* Spec = FindPatternSpec(ASC, PatternTag);
	if (!Spec)
	{
		return false;
	}

	// Spec.Ability = 그 패턴 BP의 기본 객체(CDO). Target Usage 같은 BP 칸 값은 여기서 읽는다.
	const ULOAMonsterAbility* Pattern = CastChecked<ULOAMonsterAbility>(Spec->Ability);

	// Required인데 쓸 대상이 없음 (발동하면 ActivateAbility ①에서 취소될 것)
	if (Pattern->TargetUsage == ELOAPatternTargetUsage::Required && !ULOACombatLibrary::IsTargetable(Target))
	{
		return false;
	}

	// 쿨타임, 막는 태그(State.Block.Ability = 다른 패턴 중, State.Dead = 죽음)를 GAS가 발동 전에 보는 것과 같은 함수로 본다.
	return Pattern->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get());
}

const FGameplayAbilitySpec* ULOAMonsterAbility::FindPatternSpec(const UAbilitySystemComponent* ASC, FGameplayTag PatternTag)
{
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const ULOAMonsterAbility* Pattern = Cast<ULOAMonsterAbility>(Spec.Ability);
		if (Pattern && Pattern->GetAssetTags().HasTagExact(PatternTag))
		{
			return &Spec;
		}
	}

	LOA_LOG(ASC->GetAvatarActor(), Warning, TEXT("[Pattern] %s has no pattern with tag %s. Check the boss AbilityDataSet and the GA Asset Tags."),
		*GetNameSafe(ASC->GetAvatarActor()), *PatternTag.ToString());
	return nullptr;
}

void ULOAMonsterAbility::GetPatternTags(const UAbilitySystemComponent* ASC, FGameplayTagContainer& OutTags)
{
	if (!ASC)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (const ULOAMonsterAbility* Pattern = Cast<ULOAMonsterAbility>(Spec.Ability))
		{
			OutTags.AppendTags(Pattern->GetAssetTags());
		}
	}
}

// =============================================================================
// 발동 흐름
// =============================================================================

void ULOAMonsterAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super::ActivateAbility는 부르지 않는다. (BP 그래프로 흐름을 바꾸지 못하게. 플레이어 스킬과 같음)

	// ① 대상 정하기. Required인데 쓸 대상이 없으면 취소 → 부른 쪽(StateTree)이 대상을 다시 고른다
	PatternTarget = ResolveTarget(TriggerEventData);
	if (TargetUsage == ELOAPatternTargetUsage::Required && !PatternTarget.IsValid())
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[Pattern] %s needs a target but got none (or the target is dead). Cancelled."),
			*GetNameSafe(GetClass()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ② 쿨타임 시작. (쿨타임 중이면 여기까지 오지 않는다. GAS가 발동 전에 막음)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ③ 걷던 중이면 그 자리에 선다
	if (bStopMovementOnStart)
	{
		if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			Character->GetCharacterMovement()->StopMovementImmediately();
		}
	}

	// ④ 대상 쪽으로 회전. 모듈보다 먼저 → Spawn 모듈이 "보는 방향 앞"에 소환할 때 이미 대상을 보고 있다
	if (bFaceTarget && PatternTarget.IsValid())
	{
		FaceLocation(PatternTarget->GetActorLocation());
	}

	// ⑤ 스킬 모듈. 몽타주보다 먼저 (이벤트 대기 등록이 첫 노티파이보다 앞서게)
	ActivateSkillModules();

	// 모듈 시작 중에 끝났으면(설정 오류 등) 몽타주를 틀지 않는다.
	if (!IsActive())
	{
		return;
	}

	// ⑥ 실제 동작 (자식이 덮어쓰는 곳)
	StartSkill();
}

void ULOAMonsterAbility::StartSkill()
{
	// 기본: 몽타주 재생 + 끝나면 종료. 몽타주가 없으면 할 일이 없으므로 바로 정상 종료
	if (!PlayMontageAndEnd())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULOAMonsterAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 대상은 이번 발동에만 쓴다. 다음 발동에 예전 대상이 남지 않게 비운다.
	PatternTarget.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const AActor* ULOAMonsterAbility::ResolveTarget(const FGameplayEventData* TriggerEventData) const
{
	// 대상을 안 쓰는 패턴이거나, 이벤트 없이 발동됐으면 대상 없음
	if (TargetUsage == ELOAPatternTargetUsage::None || !TriggerEventData)
	{
		return nullptr;
	}

	const AActor* Target = TriggerEventData->Target.Get();

	// 죽은 플레이어, 사라진 액터는 "대상 없음"으로 본다. (판정이 대상을 고르는 규칙과 같은 함수)
	return ULOACombatLibrary::IsTargetable(Target) ? Target : nullptr;
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOAMonsterAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Asset Tags가 없으면 TryActivatePattern(태그)로 찾을 수 없다. → StateTree도 콘솔 명령도 이 패턴을 못 쓴다
	if (GetAssetTags().IsEmpty())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Asset Tags is empty. Add the pattern tag (e.g. Ability.Grux.Slam) so it can be activated by tag."),
			*GetNameSafe(GetClass()))));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
