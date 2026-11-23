// LOAGameplayAbility.cpp

#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "AbilitySystem/Effects/LOAGE_Cooldown.h"
#include "AbilitySystem/Effects/LOAGE_InfiniteTags.h"
#include "AbilitySystem/Effects/LOAGE_TimedTags.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Character/Player/LOACharacter.h"
#include "Data/LOACharacterClassData.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/LOAAnimNotify_SendGameplayEvent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGameplayAbility)

ULOAGameplayAbility::ULOAGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 캐릭터당 인스턴스 하나를 재사용 (Ability 안에 변수를 둘 수 있음)
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 네트워크 실행 정책은 여기서 정하지 않는다.
	//   플레이어 → ULOAPlayerAbility에서 LocalPredicted
	//   몬스터   → ULOAMonsterAbility에서 ServerOnly
	//   사망     → ULOAGA_Death에서 ServerOnly

	// 죽어 있는 동안에는 어떤 Ability도 발동하지 않는다. (플레이어 스킬, 보스 패턴 모두)
	// 예외는 사망 Ability 자신뿐이다. (ULOAGA_Death 생성자에서 뺀다)
	// State.Dead는 GE로 붙어 모든 컴퓨터에 복제되므로, 클라이언트의 예측 발동도 여기서 막힌다.
	ActivationBlockedTags.AddTag(LOAGameplayTags::State_Body_Dead);
}

// =============================================================================
// 도우미
// =============================================================================

ULOAAbilitySystemComponent* ULOAGameplayAbility::GetLOAAbilitySystemComponentFromActorInfo() const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Cast<ULOAAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
}

void ULOAGameplayAbility::FaceLocation(const FVector& TargetLocation) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// 높이 차이는 무시한다. (보스 몸통 위를 가리켜도 위/아래로 기울지 않게)
	FVector Direction = TargetLocation - Avatar->GetActorLocation();
	Direction.Z = 0.f;

	// 발밑을 가리키면 방향이 없으므로 그대로 둔다.
	if (Direction.IsNearlyZero())
	{
		return;
	}

	// 서버에서 돌린 회전은 캐릭터 복제로 다른 플레이어 화면에도 전달된다.
	Avatar->SetActorRotation(Direction.Rotation());
}

void ULOAGameplayAbility::CancelOtherAbilities(const FGameplayTagContainer& TagsToKeep)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	// WithTags = nullptr     : 태그와 상관없이 모두
	// WithoutTags = 남길 태그 : 이 태그를 가진 Ability는 빼고 (비어 있으면 아무것도 빼지 않음)
	// Ignore = this           : 나 자신은 취소하지 않음
	ASC->CancelAbilities(nullptr, TagsToKeep.IsEmpty() ? nullptr : &TagsToKeep, this);
}

FActiveGameplayEffectHandle ULOAGameplayAbility::ApplyStateTagsToSelf(const FGameplayTagContainer& Tags, float Duration)
{
	// 시간이 있으면 TimedTags (스스로 사라짐), 없으면 InfiniteTags (지울 때까지)
	const bool bTimed = Duration > 0.f;
	const TSubclassOf<UGameplayEffect> EffectClass = bTimed
		? TSubclassOf<UGameplayEffect>(ULOAGE_TimedTags::StaticClass())
		: TSubclassOf<UGameplayEffect>(ULOAGE_InfiniteTags::StaticClass());

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Error, TEXT("[Ability] %s : failed to make state tag effect spec (%s)."),
			*GetNameSafe(GetClass()), *Tags.ToStringSimple());
		return FActiveGameplayEffectHandle();
	}

	if (bTimed)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, Duration);
	}
	SpecHandle.Data->DynamicGrantedTags.AppendTags(Tags);

	return ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
}

void ULOAGameplayAbility::StopAvatarMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// 몬스터·보스: AI 길 찾기를 멈춘다. (안 멈추면 이동 컴포넌트를 세워도 다음 틱에 다시 걸어간다)
	if (AAIController* AIController = Cast<AAIController>(Character->GetController()))
	{
		AIController->StopMovement();
	}

	Character->GetCharacterMovement()->StopMovementImmediately();
}

// =============================================================================
// 몽타주
// =============================================================================

bool ULOAGameplayAbility::PlayMontageOnly()
{
	UAnimMontage* MontageToPlay = GetMontage();
	if (!MontageToPlay)
	{
		return false;
	}

	// bStopWhenAbilityEnds = true: Ability가 끝나면 몽타주도 빠진다. 몽타주 끝 알림은 받지 않는다 (끝은 다른 것이 정함)
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds*/ true);
	MontageTask->ReadyForActivation();
	return true;
}

UAnimMontage* ULOAGameplayAbility::GetMontage() const
{
	// GetAvatarActorFromActorInfo는 원본(CDO)에서 부르면 엔진 경고(ensure)가 난다 → 멤버를 직접 본다
	const AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	return GetMontageFor(Avatar);
}

UAnimMontage* ULOAGameplayAbility::GetMontageFor(const AActor* Avatar) const
{
	// ① 시전자 직업 표. 플레이어 캐릭터만 직업 데이터가 있다 (보스·잡몹은 Cast 실패 → ②)
	if (MontageTag.IsValid())
	{
		const ALOACharacter* Character = Cast<ALOACharacter>(Avatar);
		const ULOACharacterClassData* ClassData = Character ? Character->GetCharacterClassData() : nullptr;
		if (UAnimMontage* ClassMontage = ClassData ? ClassData->FindMontage(MontageTag) : nullptr)
		{
			return ClassMontage;
		}
	}

	// ② 이 GA의 기본 몽타주
	return Montage;
}

bool ULOAGameplayAbility::GetTimeUntilMontageEvent(const FGameplayTag& EventTag, float& OutSeconds) const
{
	const UAnimMontage* MontageToPlay = GetMontage();
	if (!MontageToPlay)
	{
		return false;
	}

	// 지금 몽타주 위치. 아직 재생 전이면 0 (처음부터)
	float CurrentPosition = 0.f;
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (const UAnimInstance* AnimInstance = Info ? Info->GetAnimInstance() : nullptr)
	{
		if (AnimInstance->Montage_IsPlaying(MontageToPlay))
		{
			CurrentPosition = AnimInstance->Montage_GetPosition(MontageToPlay);
		}
	}

	// 지금 이후 가장 이른 그 노티파이 (몽타주 타임라인 기준 초)
	float EventTime = 0.f;
	if (!ULOAAnimNotify_SendGameplayEvent::FindEventTimeInMontage(MontageToPlay, EventTag, CurrentPosition, EventTime))
	{
		return false;
	}

	// 실제 재생 속도 = GA의 재생 속도 × 몽타주 에셋 자체의 Rate Scale
	const float EffectiveRate = MontagePlayRate * MontageToPlay->RateScale;
	if (EffectiveRate <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutSeconds = (EventTime - CurrentPosition) / EffectiveRate;
	return true;
}

bool ULOAGameplayAbility::PlayMontageAndEnd()
{
	UAnimMontage* MontageToPlay = GetMontage();
	if (!MontageToPlay)
	{
		return false;
	}

	// ASC를 통해 재생되므로 다른 플레이어 화면에도 자동으로 복제된다.
	// bStopWhenAbilityEnds(기본 true): Ability가 먼저 끝나면(취소 등) 몽타주도 멈춘다.
	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, MontagePlayRate);

	// BlendOut: 몽타주가 끝나기 시작하는 순간(섞이며 빠지는 중).
	//   여기서 종료하면 다음 입력을 조금 더 빨리 받을 수 있어서 조작감이 좋다.
	// Completed: 완전히 끝남. BlendOut 없이 끝나는 경우 대비. (이미 종료했으면 중복 호출은 무시됨)
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);

	// Interrupted: 다른 몽타주가 끼어듦 / Cancelled: Ability가 취소됨
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);

	MontageTask->ReadyForActivation();
	return true;
}

void ULOAGameplayAbility::OnMontageFinished()
{
	// 정상 종료. 조종하는 사람 컴퓨터와 서버가 각자 종료한다.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULOAGameplayAbility::OnMontageCancelled()
{
	// 몽타주가 끊김 (다른 몽타주가 끼어듦, Ability 취소 등) → 할 일을 못 끝냈으므로 취소 종료
	// 자식이 덮어쓸 수 있다. (ULOAPlayerAbility: 후딜 중이면 정상 종료)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// =============================================================================
// 쿨타임
// =============================================================================
// GAS 기본 동작은 CooldownGameplayEffectClass(스킬마다 GE 하나)를 쓴다.
// LOA는 공용 GE 하나 + Ability의 값(CooldownDuration, CooldownTags)으로 대신한다. → 엔진의 쿨타임 GE 칸은 쓰지 않는다.
//
// 두 함수 모두 const: Ability 인스턴스가 없을 때 CDO에서도 불릴 수 있으므로 멤버 변수를 바꾸지 않는다.

const FGameplayTagContainer* ULOAGameplayAbility::GetCooldownTags() const
{
	// GAS가 발동 전에 "이 태그 중 하나라도 ASC에 있으면 쿨타임 중"으로 판단한다. (CheckCooldown)
	// 비어 있으면 쿨타임 검사를 하지 않는 것과 같다.
	return &CooldownTags;
}

void ULOAGameplayAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 쿨타임 없는 Ability (평타, 피격 반응 등). 커브 테이블 행이면 스킬 레벨의 값
	const float Duration = CooldownDuration.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo));
	if (Duration <= 0.f)
	{
		return;
	}

	// 시간은 있는데 태그가 없으면 쿨타임 GE가 붙어도 아무것도 막지 못한다. → BP 설정 누락을 알린다.
	if (CooldownTags.IsEmpty())
	{
		LOA_LOG(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr, Warning,
			TEXT("[%s] CooldownDuration is %.1f but CooldownTags is empty. No cooldown."), *GetNameSafe(GetClass()), Duration);
		return;
	}

	// ① 공용 쿨타임 GE로 Spec 만들기
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, ULOAGE_Cooldown::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// ② 이 Ability의 값 넣기
	//    시간 : GE의 Duration이 SetByCaller.Cooldown을 읽는다.
	//    태그 : GE에 없던 태그를 이번 적용에만 추가로 부여한다. GE가 붙어 있는 동안 ASC가 이 태그를 가진다.
	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Cooldown, Duration);
	SpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);

	// ③ 나에게 적용. 조종하는 사람 컴퓨터에서는 예측으로 적용되고, 서버가 확인하면 그대로 유지된다.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

// =============================================================================
// 스킬 모듈
// =============================================================================

void ULOAGameplayAbility::ActivateSkillModules()
{
	// 이번 발동에서 이미 시작했으면 다시 하지 않는다. (자식 흐름이 실수로 두 번 불러도 Task가 두 벌 생기지 않게)
	if (bSkillModulesActive)
	{
		return;
	}

	// 모듈은 Ability 인스턴스마다 복사본이 있어야 한다. 인스턴스가 없는 Ability(NonInstanced)에서는 쓸 수 없다.
	// (LOA Ability는 모두 InstancedPerActor라 정상이면 여기 걸리지 않는다)
	if (!IsInstantiated())
	{
		if (!Modules.IsEmpty())
		{
			LOA_LOG(GetAvatarActorFromActorInfo(), Warning,
				TEXT("[%s] Skill modules need an instanced ability. Modules are ignored."), *GetNameSafe(GetClass()));
		}
		return;
	}

	bSkillModulesActive = true;

	for (ULOASkillModule* Module : Modules)
	{
		// 디테일 패널에서 [+]만 누르고 종류를 안 고른 칸은 비어 있다. → 건너뛴다.
		if (Module)
		{
			Module->ActivateModule(this);
		}
	}
}

void ULOAGameplayAbility::DeactivateSkillModules(bool bWasCancelled)
{
	// 시작하지 않았거나 이미 끝냈으면 할 일 없음 (EndAbility가 두 번 불리는 경우)
	if (!bSkillModulesActive)
	{
		return;
	}

	bSkillModulesActive = false;

	for (ULOASkillModule* Module : Modules)
	{
		if (Module)
		{
			Module->DeactivateModule(bWasCancelled);
		}
	}
}

void ULOAGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 모듈 종료를 엔진 종료보다 먼저 한다. 모듈의 SkillModuleEnd가 주인 Ability 정보를 아직 쓸 수 있게.
	// (모듈이 만든 Task는 아래 Super에서 엔진이 정리한다)
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		DeactivateSkillModules(bWasCancelled);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =============================================================================
// 모듈 창구
// =============================================================================

int32 ULOAGameplayAbility::GetSkillModuleIndex(const ULOASkillModule* Module) const
{
	// GA 에셋의 칸 순서 (내 컴퓨터와 서버가 같다)
	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		if (Modules[Index] == Module)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool ULOAGameplayAbility::HasSkillModuleAuthority() const
{
	// 서버(데디케이티드, 리슨 호스트, 스탠드얼론)면 true. 클라이언트의 예측 실행이면 false.
	return HasAuthority(&CurrentActivationInfo);
}

FLOASkillModuleContext ULOAGameplayAbility::MakeSkillModuleContext() const
{
	FLOASkillModuleContext Context;
	Context.SourceASC = GetAbilitySystemComponentFromActorInfo();
	Context.SourceActor = GetAvatarActorFromActorInfo();	// 범위 기준 = 내 캐릭터
	Context.Caster = GetAvatarActorFromActorInfo();			// 편 판단 기준 = 내 캐릭터 (소환 액터에 복사돼도 그대로)
	Context.Level = static_cast<float>(GetAbilityLevel());
	Context.ChargeMultiplier = GetChargeMultiplier();
	// 공격 속성: Asset Tags 중 Skill.Attribute 아래 태그만. 예) { Skill.Attribute.Counter }
	Context.AttackTags = GetAssetTags().Filter(FGameplayTagContainer(LOAGameplayTags::Skill_Attribute));
	// 보낸 스킬 (보호막 덮어쓰기가 "같은 스킬"을 알아보는 데 쓴다). 이 함수가 const라 const를 뗀다 (값은 바꾸지 않음)
	Context.SourceAbility = const_cast<ULOAGameplayAbility*>(this);
	return Context;
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOAGameplayAbility::IsDataValid(FDataValidationContext& Context) const
{
	// 엔진 쪽 검사 결과 + 우리 검사. 아무것도 못 찾으면 Valid.
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// 메시지 앞에 붙일 이름 (BP 클래스 이름. 예: GA_Berserker_Whirlwind_C)
	const FString OwnerName = GetNameSafe(GetClass());

	// ① 쿨타임 시간은 있는데 태그가 없으면 쿨타임이 아무것도 막지 못한다. (레벨 1 값으로 확인)
	const float CooldownAtLevel1 = CooldownDuration.GetValueAtLevel(1.f);
	if (CooldownAtLevel1 > 0.f && CooldownTags.IsEmpty())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Cooldown Duration is %.1fs but Cooldown Tags is empty. Cooldown will not work."),
			*OwnerName, CooldownAtLevel1)));
		Result = EDataValidationResult::Invalid;
	}

	// ② 모듈마다 검사
	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		const ULOASkillModule* Module = Modules[Index];
		const FString Prefix = FString::Printf(TEXT("%s : Modules[%d]"), *OwnerName, Index);

		// [+]만 누르고 종류(Apply 등)를 안 고른 칸
		if (!Module)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("%s : module type is not selected. Pick a type from the dropdown or remove the entry."), *Prefix)));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		// 모듈 자기 칸 검사. 노티파이 확인용으로 이 GA의 몽타주를 넘긴다.
		Result = CombineDataValidationResults(Result, Module->ValidateSkillModule(Context, Montage, Prefix));
	}

	return Result;
}
#endif
