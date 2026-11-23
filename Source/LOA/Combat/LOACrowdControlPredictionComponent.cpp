// LOACrowdControlPredictionComponent.cpp

#include "Combat/LOACrowdControlPredictionComponent.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACrowdControlPredictionComponent)

namespace LOACrowdControlPrediction
{
	/** 예측 중 이 컴퓨터에서만 붙이는 태그 (복제 안 됨): 이동 금지 + 스킬·회피 금지 */
	FGameplayTagContainer MakeBlockTags()
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(LOAGameplayTags::State_Block_Move);
		Tags.AddTag(LOAGameplayTags::State_Block_Ability);
		return Tags;
	}

	/** 주인 캐릭터의 애니메이션 */
	UAnimInstance* GetAnimInstance(const AActor* Owner)
	{
		const ACharacter* Character = Cast<ACharacter>(Owner);
		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		return Mesh ? Mesh->GetAnimInstance() : nullptr;
	}
}

ULOACrowdControlPredictionComponent::ULOACrowdControlPredictionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 불릴 때만. 복제하지 않는다 (맞은 사람 컴퓨터에서만 쓴다)
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void ULOACrowdControlPredictionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 캐릭터가 사라짐 (죽고 다시 태어남, 맵 이동) → 로컬 태그가 ASC(PlayerState)에 남지 않게
	EndPrediction();
	Super::EndPlay(EndPlayReason);
}

bool ULOACrowdControlPredictionComponent::PredictCrowdControl(TSubclassOf<UGameplayEffect> CrowdControlEffect,
	const FVector& SourcePoint, float Timeout)
{
	// 맞은 사람 컴퓨터(내가 조종하는 원격 캐릭터)만. 리슨 호스트 본인은 피해자 판정 요청을 받지 않는다
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!CrowdControlEffect || !Owner || !World || !ASC || Owner->GetLocalRole() != ROLE_AutonomousProxy || bPredicted)
	{
		return false;
	}

	const UGameplayEffect* EffectDef = CrowdControlEffect->GetDefaultObject<UGameplayEffect>();
	if (!EffectDef)
	{
		return false;
	}

	// ① 이 GE가 붙이는 태그로 발동하는 GA = 진짜 CC GA (예: 경직 GE → State.CrowdControl.Paralysis → GA_Paralysis)
	//   CC 종류를 여기서 하드코딩하지 않는다. GA가 선언한 발동 조건(AbilityTriggers)을 그대로 따라간다
	const FGameplayTagContainer& GrantedTags = EffectDef->GetGrantedTags();
	const ULOAGameplayAbility* CrowdControlAbility = FindAbilityTriggeredByTags(*ASC, GrantedTags);
	if (!CrowdControlAbility)
	{
		return false;	// 이 GE로 발동하는 GA가 없다 (CC가 아님)
	}

	// ② 이미 그 CC 중이면 진짜 GA가 "또 맞음"을 처리한다. 다른 CC 중이면 아래 면역 검사가 거른다
	//   (예: 누움 중 → 경직 GE의 Ignore 조건 State.CrowdControl.Push)
	if (ASC->HasAnyMatchingGameplayTags(GrantedTags))
	{
		return false;
	}

	// ③ 면역: GE 에셋의 적용 조건(Target Tag Requirements)을 내 태그로 본다. 서버의 GE 적용과 같은 검사
	//   (엔진 UTargetTagRequirementsGameplayEffectComponent::CanGameplayEffectApply: Application 조건 + Removal 조건.
	//    Removal 조건은 엔진이 서버에서만 보는데, 서버 결과를 예측하는 것이라 서버 기준으로 본다)
	if (const UTargetTagRequirementsGameplayEffectComponent* Requirements = EffectDef->FindComponent<UTargetTagRequirementsGameplayEffectComponent>())
	{
		const FGameplayTagContainer& OwnedTags = ASC->GetOwnedGameplayTags();
		if (!Requirements->ApplicationTagRequirements.RequirementsMet(OwnedTags)
			|| (!Requirements->RemovalTagRequirements.IsEmpty() && Requirements->RemovalTagRequirements.RequirementsMet(OwnedTags)))
		{
			LOA_LOG(Owner, Log, TEXT("[CrowdControl] predict skipped: immune to %s"), *GetNameSafe(CrowdControlEffect));
			return false;
		}
	}

	// ④ 이동·스킬 금지 (이 컴퓨터만. 로컬 태그는 복제되지 않는다. 서버는 진짜 GE로 막는다)
	ASC->AddLooseGameplayTags(LOACrowdControlPrediction::MakeBlockTags());
	bPredicted = true;
	bConfirmed = false;

	// ⑤ 판정 기준점(때린 쪽)을 바라본다. 내 캐릭터 회전은 이동 정보로 서버에 가므로 남의 화면에도 돈 모습이 간다
	//   예) 장판 중심이 내 서쪽 → 서쪽을 본다 (진짜 CC도 동쪽으로 밀면서 서쪽을 본다. 헤더 [흐름] 바라보기)
	const FVector Facing = (SourcePoint - Owner->GetActorLocation()).GetSafeNormal2D();
	if (!Facing.IsNearlyZero())
	{
		FRotator NewRotation = Owner->GetActorRotation();
		NewRotation.Yaw = Facing.Rotation().Yaw;
		Owner->SetActorRotation(NewRotation);
	}

	// ⑥ 몽타주를 먼저 튼다 (이 컴퓨터만. 남의 화면은 진짜 GA가 복제로)
	PredictedMontage.Reset();
	UAnimInstance* AnimInstance = LOACrowdControlPrediction::GetAnimInstance(Owner);
	// GA 원본(CDO)이라 시전자를 모른다 → 내 캐릭터를 넘겨 직업 몽타주를 고른다 (진짜 GA의 GetMontage와 같은 결과)
	UAnimMontage* PredictMontage = CrowdControlAbility->GetMontageFor(Owner);
	if (AnimInstance && PredictMontage)
	{
		AnimInstance->Montage_Play(PredictMontage, CrowdControlAbility->GetMontagePlayRate());
		const FName StartSection = CrowdControlAbility->GetPredictionStartSection();
		if (!StartSection.IsNone() && PredictMontage->IsValidSectionName(StartSection))
		{
			AnimInstance->Montage_JumpToSection(StartSection, PredictMontage);
		}
		PredictedMontage = PredictMontage;
	}

	// ⑦ 진짜 GA가 안 오면 되돌린다
	World->GetTimerManager().SetTimer(PredictionTimer, this, &ThisClass::OnPredictionTimeout, FMath::Max(Timeout, 0.05f), false);

	LOA_LOG(Owner, Log, TEXT("[CrowdControl] predicted %s (montage %s, timeout %.2fs)"),
		*GetNameSafe(CrowdControlAbility->GetClass()), *GetNameSafe(PredictMontage), Timeout);
	return true;
}

float ULOACrowdControlPredictionComponent::ConfirmPrediction(const UAnimMontage* Montage)
{
	if (!bPredicted || bConfirmed)
	{
		return -1.f;
	}

	// 이어받음: 되돌리기 취소, 스킬 금지는 이제 진짜 CC 태그가 맡는다 (띄움 중 기상기는 되어야 하므로 로컬 태그는 푼다)
	bConfirmed = true;
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PredictionTimer);
	}
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		ASC->RemoveLooseGameplayTag(LOAGameplayTags::State_Block_Ability);
	}

	// 같은 몽타주가 재생 중이면 그 위치 → GA가 거기서부터 튼다 (처음으로 돌아가지 않게)
	const UAnimInstance* AnimInstance = LOACrowdControlPrediction::GetAnimInstance(GetOwner());
	const float Position = (AnimInstance && Montage && PredictedMontage.Get() == Montage && AnimInstance->Montage_IsPlaying(Montage))
		? AnimInstance->Montage_GetPosition(Montage)
		: -1.f;

	if (Position >= 0.f)
	{
		LOA_LOG(GetOwner(), Log, TEXT("[CrowdControl] prediction confirmed (resume montage at %.2fs)"), Position);
	}
	else
	{
		LOA_LOG(GetOwner(), Log, TEXT("[CrowdControl] prediction confirmed (predicted montage not playing, GA plays it from the start)"));
	}
	return Position;
}

void ULOACrowdControlPredictionComponent::EndPrediction()
{
	if (!bPredicted)
	{
		return;
	}

	// 아직 이어받기 전이면 스킬 금지도 붙어 있다
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		ASC->RemoveLooseGameplayTag(LOAGameplayTags::State_Block_Move);
		if (!bConfirmed)
		{
			ASC->RemoveLooseGameplayTag(LOAGameplayTags::State_Block_Ability);
		}
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PredictionTimer);
	}
	bPredicted = false;
	bConfirmed = false;
	PredictedMontage.Reset();
}

void ULOACrowdControlPredictionComponent::OnPredictionTimeout()
{
	if (!bPredicted || bConfirmed)
	{
		return;
	}

	// 진짜 GA가 안 옴 (면역 차이 등) → 되돌린다: 몽타주를 멈추고 로컬 태그를 푼다
	UAnimInstance* AnimInstance = LOACrowdControlPrediction::GetAnimInstance(GetOwner());
	if (UAnimMontage* Montage = PredictedMontage.Get(); AnimInstance && Montage)
	{
		AnimInstance->Montage_Stop(0.2f, Montage);
	}
	LOA_LOG(GetOwner(), Warning, TEXT("[CrowdControl] prediction not confirmed by the server. Undone"));
	EndPrediction();
}

UAbilitySystemComponent* ULOACrowdControlPredictionComponent::GetOwnerAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

const ULOAGameplayAbility* ULOACrowdControlPredictionComponent::FindAbilityTriggeredByTags(const UAbilitySystemComponent& ASC, const FGameplayTagContainer& Tags)
{
	// 부여된 Ability 중 "이 태그가 붙으면 발동"(OwnedTagAdded)인 것. 클라에도 부여된 Ability 목록이 복제돼 있다
	for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
	{
		const ULOAGameplayAbility* Ability = Cast<ULOAGameplayAbility>(Spec.Ability);
		if (!Ability)
		{
			continue;
		}
		for (const FAbilityTriggerData& Trigger : Ability->GetAbilityTriggers())
		{
			if (Trigger.TriggerSource == EGameplayAbilityTriggerSource::OwnedTagAdded && Tags.HasTagExact(Trigger.TriggerTag))
			{
				return Ability;
			}
		}
	}
	return nullptr;
}
