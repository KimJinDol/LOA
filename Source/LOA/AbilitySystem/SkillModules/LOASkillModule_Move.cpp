// LOASkillModule_Move.cpp

#include "AbilitySystem/SkillModules/LOASkillModule_Move.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "Animation/LOAAnimNotify_SendGameplayEvent.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "Game/LOARaidSubsystem.h"

#include "AbilitySystemComponent.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionJumpForce.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "GameFramework/Character.h"
#include "GameFramework/RootMotionSource.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillModule_Move)

void ULOASkillModule_Move::SkillModuleInit()
{
	// 이전 발동의 기록은 지난 일이다. (모듈은 Ability와 함께 재사용된다)
	bPassThroughActive = false;
}

void ULOASkillModule_Move::SkillModuleTriggered(const FGameplayEventData* EventData)
{
	// 서버로 막지 않는다. GA가 도는 곳(보스 = 서버 / 플레이어 = 내 컴퓨터 + 서버)에서 각자 같은 이동을 한다.
	ULOAGameplayAbility* Owner = GetOwnerAbility();
	ACharacter* Character = Owner ? Cast<ACharacter>(Owner->GetAvatarActorFromActorInfo()) : nullptr;
	if (!Character)
	{
		return;
	}

	// ① 걸린 시간: Arrive Event 노티파이까지 남은 초 > Duration
	//    예) 지금 몽타주 0.3초, 착지 노티파이 1.3초, 재생 속도 1 → 1.0초
	float MoveSeconds = Duration;
	if (ArriveEvent.IsValid())
	{
		float SecondsUntilArrive = 0.f;
		if (Owner->GetTimeUntilMontageEvent(ArriveEvent, SecondsUntilArrive) && SecondsUntilArrive > 0.f)
		{
			MoveSeconds = SecondsUntilArrive;
		}
		else
		{
			LOA_LOG(Character, Warning, TEXT("[%s] Move module: no '%s' notify left in the montage. Using Duration %.2f."),
				*GetNameSafe(Owner->GetClass()), *ArriveEvent.ToString(), Duration);
		}
	}
	MoveSeconds = FMath::Max(MoveSeconds, 0.05f);	// 0초 이동은 Task가 바로 끝나서 순간이동처럼 보인다

	// ② 어디로: 이동 방향과 거리 (바닥 기준. 캐릭터를 직접 돌리지는 않는다)
	//    Forward  : 지금 보는 방향으로 Distance
	//    Location : 위치 표시 쪽으로, 거기까지의 바닥 거리. 예) 보스 (1000, 0), 표시 (0, 0) → -X 방향 1000
	FRotator Facing(0.f, Character->GetActorRotation().Yaw, 0.f);
	float MoveDistance = Distance;
	if (Destination == ELOAMoveDestination::Location)
	{
		const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(Character);
		FVector Goal;
		if (Raid && Raid->FindLocation(LocationTag, Goal))
		{
			const FVector ToGoal(Goal.X - Character->GetActorLocation().X, Goal.Y - Character->GetActorLocation().Y, 0.f);
			MoveDistance = ToGoal.Size();
			if (MoveDistance > KINDA_SMALL_NUMBER)
			{
				Facing = ToGoal.Rotation();
			}
		}
		else
		{
			LOA_LOG(Character, Warning, TEXT("[%s] Move module: no location marker '%s' in this level. Moving Forward %.0f instead."),
				*GetNameSafe(Owner->GetClass()), *LocationTag.ToString(), Distance);
		}
	}

	// ③ 관통: 이동하는 동안 몸 막이를 뚫고 지나가게
	if (bPassThroughBodyBlockers)
	{
		SetPassThrough(true);
	}

	// ④ 이동. 도착하면 멈춘다 (도착 순간 속도 0). Task는 주인 Ability 소속 → Ability가 끝나면 같이 끝난다.
	if (Height > 0.f)
	{
		// 포물선 점프. 착지 판정에 맞춰 정해진 시간에 도착해야 하므로 "땅에 닿으면 끝"은 쓰지 않는다.
		UAbilityTask_ApplyRootMotionJumpForce* JumpTask = UAbilityTask_ApplyRootMotionJumpForce::ApplyRootMotionJumpForce(
			Owner, NAME_None, Facing, MoveDistance, Height, MoveSeconds,
			/*MinimumLandedTriggerTime*/ 0.f, /*bFinishOnLanded*/ false,
			ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f,
			/*PathOffsetCurve*/ nullptr, /*TimeMappingCurve*/ nullptr);
		if (JumpTask)
		{
			JumpTask->OnFinish.AddDynamic(this, &ThisClass::OnMoveFinished);
			JumpTask->ReadyForActivation();
		}
	}
	else
	{
		// 땅 위 이동. 그 방향으로 MoveDistance 지점까지. (높이는 지금 높이 그대로)
		const FVector TargetLocation = Character->GetActorLocation() + Facing.Vector() * MoveDistance;
		UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			Owner, NAME_None, TargetLocation, MoveSeconds,
			/*bSetNewMovementMode*/ false, MOVE_Walking, /*bRestrictSpeedToExpected*/ false,
			/*PathOffsetCurve*/ nullptr,
			ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f);
		if (MoveTask)
		{
			MoveTask->OnTimedOut.AddDynamic(this, &ThisClass::OnMoveFinished);
			MoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &ThisClass::OnMoveFinished);
			MoveTask->ReadyForActivation();
		}
	}

	const FString DestinationText = (Destination == ELOAMoveDestination::Location)
		? FString::Printf(TEXT("to %s"), *LocationTag.ToString()) : FString(TEXT("forward"));
	LOA_LOG(Character, Log, TEXT("[Move] %s : %s %.0fcm, height %.0f, %.2fs%s"),
		*GetNameSafe(Owner->GetClass()), *DestinationText,
		MoveDistance, Height, MoveSeconds, bPassThroughBodyBlockers ? TEXT(", pass through blockers") : TEXT(""));
}

void ULOASkillModule_Move::SkillModuleEnd(bool bWasCancelled)
{
	// Ability가 끝났다 (정상이든 취소든). 이동 Task는 엔진이 정리한다. 관통만 끝낸다.
	SetPassThrough(false);
}

void ULOASkillModule_Move::OnMoveFinished()
{
	SetPassThrough(false);
}

void ULOASkillModule_Move::SetPassThrough(bool bEnable)
{
	// 이미 그 상태면 아무것도 안 함 (도착 알림과 Ability 끝이 둘 다 와도 한 번만 끝낸다)
	if (bEnable == bPassThroughActive)
	{
		return;
	}

	const ULOAGameplayAbility* Owner = GetOwnerAbility();
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponentFromActorInfo() : nullptr;
	if (!ASC)
	{
		bPassThroughActive = false;
		return;
	}

	// 로컬 태그: 복제하지 않는다. 이 GA가 도는 컴퓨터(플레이어 = 내 컴퓨터 + 서버)가 각자 붙이고 뗀다. (State.Block.Move와 같은 방식)
	// GAS 태그는 개수를 센다 → 관통 스킬 두 개가 겹쳐도 먼저 끝난 쪽이 떼는 1개만 줄어든다.
	// 실제 충돌 설정은 플레이어 이동 컴포넌트가 이 태그를 보고 한 곳에서 바꾼다.
	if (bEnable)
	{
		ASC->AddLooseGameplayTag(LOAGameplayTags::State_Body_PassThroughBodyBlock);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(LOAGameplayTags::State_Body_PassThroughBodyBlock);
	}
	bPassThroughActive = bEnable;
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOASkillModule_Move::ValidateSkillModule(
	FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const
{
	// 부모: Trigger / 노티파이 검사
	EDataValidationResult Result = Super::ValidateSkillModule(Context, OwnerMontage, Prefix);

	const FString Name = FString::Printf(TEXT("%s Move"), *Prefix);

	// Destination = Location인데 태그가 비었음
	if (Destination == ELOAMoveDestination::Location && !LocationTag.IsValid())
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Destination is Location but Location Tag is empty."), *Name)));
		Result = EDataValidationResult::Invalid;
	}

	// Arrive Event: 몽타주에 그 노티파이가 있어야 자동 시간이 된다 (없으면 Duration)
	float EventTime = 0.f;
	if (ArriveEvent.IsValid() && OwnerMontage
		&& !ULOAAnimNotify_SendGameplayEvent::FindEventTimeInMontage(OwnerMontage, ArriveEvent, 0.f, EventTime))
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Montage %s has no '%s' notify (LOA Send Gameplay Event). Duration will be used."),
			*Name, *GetNameSafe(OwnerMontage), *ArriveEvent.ToString())));
	}

	return Result;
}
#endif
