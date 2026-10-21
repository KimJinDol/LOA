// LOACharacterMovementComponent.cpp

#include "Character/Player/LOACharacterMovementComponent.h"
#include "Character/Player/LOARootMotionSource_Arc.h"
#include "Core/LOACollisionChannels.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACharacterMovementComponent)

void ULOACharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// 원래 응답을 기억한다. (ALOACharacter 생성자에서 Block. BP에서 바꿨다면 그 값)
	if (const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
	{
		DefaultBodyBlockResponse = Capsule->GetCollisionResponseToChannel(LOACollision::BodyBlock);
	}
}

void ULOACharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// 죽어서 이동이 꺼졌으면(MOVE_None) 할 일 없음
	if (!CharacterOwner || !UpdatedComponent || MovementMode == MOVE_None)
	{
		return;
	}

	// ① 관통 중 (State.PassThroughBodyBlock): 막지도 밀지도 않는다.
	if (IsPassingThroughBodyBlockers())
	{
		bInsideBodyBlocker = false;
		SetIgnoreBodyBlockers(true);
		return;
	}

	// ② 겹쳤나 + 빠져나갈 방향
	FVector PushDirection = FVector::ZeroVector;
	const bool bInside = FindPushOutDirection(PushDirection);

	if (bInside != bInsideBodyBlocker)
	{
		LOA_LOG(CharacterOwner, Log, TEXT("[BodyBlock] %s"), bInside ? TEXT("inside a body blocker. Pushing out.") : TEXT("out of body blockers."));
		bInsideBodyBlocker = bInside;
	}

	// ③ 겹쳐 있으면 막지 않는다 (= 내 이동이 자유) / 빠져나왔으면 다시 막힘
	SetIgnoreBodyBlockers(bInside);

	// ④ 밀어내기. 벽(월드)에는 막힌다. 막히면 벽을 따라 미끄러진다.
	//    예) 600cm/s, 한 프레임 0.016초 → 약 10cm
	if (bInside && PushOutSpeed > 0.f && DeltaSeconds > 0.f)
	{
		const FVector Delta = PushDirection * PushOutSpeed * DeltaSeconds;
		FHitResult Hit;
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), /*bSweep*/ true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			// 밀어내기는 루트 모션 중(회피 등)이어도 벽을 따라 미끄러져야 한다 → 덮어쓴 버전이 아니라 엔진 원래 함수
			Super::SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		}
	}
}

bool ULOACharacterMovementComponent::FindPushOutDirection(FVector& OutDirection) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 내 캡슐보다 반지름을 Overlap Tolerance만큼 줄여서 찾는다. → 맞닿은 정도는 겹침이 아님
	const FCollisionShape Shape = GetPawnCapsuleCollisionShape(SHRINK_RadiusCustom, OverlapTolerance);
	const FVector MyLocation = UpdatedComponent->GetComponentLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LOABodyBlockOverlap), false, CharacterOwner);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, MyLocation, UpdatedComponent->GetComponentQuat(),
		FCollisionObjectQueryParams(LOACollision::BodyBlock), Shape, Params);

	if (Overlaps.IsEmpty())
	{
		return false;
	}

	// 몸 막이마다 "그 중심 → 나" 방향을 더한다. (몸 막이 두 개 사이에 끼면 둘의 반대쪽 합)
	FVector Sum = FVector::ZeroVector;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (const UPrimitiveComponent* Blocker = Overlap.GetComponent())
		{
			Sum += (MyLocation - Blocker->GetComponentLocation()).GetSafeNormal2D();
		}
	}

	// 정확히 한가운데라 방향이 없으면: 내가 가려는 방향 → 그것도 없으면 내 뒤쪽
	if (Sum.IsNearlyZero())
	{
		Sum = GetCurrentAcceleration().GetSafeNormal2D();
	}
	if (Sum.IsNearlyZero())
	{
		Sum = -UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	}

	OutDirection = Sum.GetSafeNormal2D();
	return true;
}

const UAbilitySystemComponent* ULOACharacterMovementComponent::GetOwnerAbilitySystem() const
{
	// 플레이어 ASC는 PlayerState에 있다. 캐릭터가 IAbilitySystemInterface로 돌려준다. (PlayerState 도착 전이면 없음)
	const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(CharacterOwner);
	return AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
}

bool ULOACharacterMovementComponent::IsPassingThroughBodyBlockers() const
{
	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	return ASC && ASC->HasMatchingGameplayTag(LOAGameplayTags::State_PassThroughBodyBlock);
}

// =============================================================================
// CC로 밀리기·날아가기 (2026-10-21: 물리 대신 정해진 궤적)
// =============================================================================

ULOACharacterMovementComponent::ULOACharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CrowdControlMove를 맞은 사람 컴퓨터로 복제하려면 이 컴포넌트가 복제되어야 한다
	SetIsReplicatedByDefault(true);
}

void ULOACharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 맞은 사람(이 캐릭터를 조종하는 컴퓨터)에게만. 다른 사람 화면은 원래의 위치 복제로 본다
	DOREPLIFETIME_CONDITION(ULOACharacterMovementComponent, CrowdControlMove, COND_AutonomousOnly);
}

void ULOACharacterMovementComponent::StartCrowdControlMove(const FVector& Direction, float Distance, float StartHeight, float ApexHeight, float Duration)
{
	if (!CharacterOwner || GetOwnerRole() != ROLE_Authority || Duration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 숫자를 정해서 복제 칸에 넣는다. Sequence가 바뀌므로 같은 숫자로 또 맞아도 맞은 사람 컴퓨터에 다시 온다
	FLOACrowdControlMove NewMove;
	NewMove.Sequence = static_cast<uint8>(CrowdControlMove.Sequence + 1);
	NewMove.bActive = true;
	NewMove.Direction = Direction.GetSafeNormal2D();
	NewMove.Distance = FMath::Max(0.f, Distance);
	NewMove.StartHeight = FMath::Max(0.f, StartHeight);
	NewMove.ApexHeight = FMath::Max(0.f, ApexHeight);
	NewMove.Duration = Duration;
	CrowdControlMove = NewMove;

	// 서버도 같은 숫자로 건다 (맞은 사람 컴퓨터는 OnRep_CrowdControlMove에서)
	ApplyCrowdControlMoveLocally(CrowdControlMove);
}

void ULOACharacterMovementComponent::StopCrowdControlMove()
{
	if (GetOwnerRole() != ROLE_Authority || !CrowdControlMove.bActive)
	{
		return;
	}

	CrowdControlMove.bActive = false;
	CrowdControlMove.Sequence = static_cast<uint8>(CrowdControlMove.Sequence + 1);
	RemoveCrowdControlMoveLocally();
}

void ULOACharacterMovementComponent::OnRep_CrowdControlMove()
{
	if (CrowdControlMove.bActive)
	{
		ApplyCrowdControlMoveLocally(CrowdControlMove);
	}
	else
	{
		RemoveCrowdControlMoveLocally();
	}
}

void ULOACharacterMovementComponent::ApplyCrowdControlMoveLocally(const FLOACrowdControlMove& Move)
{
	if (!CharacterOwner)
	{
		return;
	}

	// 이전 궤적은 버린다 (공중에서 또 맞음 / 경직 중 또 경직)
	RemoveCrowdControlMoveLocally();

	TSharedPtr<FLOARootMotionSource_Arc> Arc = MakeShared<FLOARootMotionSource_Arc>();
	Arc->InstanceName = TEXT("LOACrowdControl");	// 서버·내 컴퓨터 짝짓기(Matches)에 쓰인다 → 양쪽 같은 이름
	Arc->Direction = Move.Direction;
	Arc->Distance = Move.Distance;
	Arc->StartHeight = Move.StartHeight;
	Arc->ApexHeight = Move.ApexHeight;
	Arc->Duration = Move.Duration;
	ActiveCrowdControlSourceId = ApplyRootMotionSource(Arc);

	// 때린 쪽을 바라본다 = 밀리는 방향의 반대. 서버와 내 컴퓨터가 각자 돌린다
	//   (내 캐릭터의 회전은 내 컴퓨터가 정해서 이동 정보로 서버에 보낸다 → 서버만 돌리면 내 화면에선 안 돈다)
	//   예) 동쪽으로 밀림 → 서쪽(때린 쪽)을 본다
	const FVector Facing = -FVector(Move.Direction).GetSafeNormal2D();
	if (!Facing.IsNearlyZero())
	{
		FRotator NewRotation = CharacterOwner->GetActorRotation();
		NewRotation.Yaw = Facing.Rotation().Yaw;
		CharacterOwner->SetActorRotation(NewRotation);
	}
}

void ULOACharacterMovementComponent::RemoveCrowdControlMoveLocally()
{
	// 이미 끝난(땅에 닿음, 시간 다 됨) 소스면 엔진이 지웠으므로 아무 일 없다
	if (ActiveCrowdControlSourceId != 0)
	{
		RemoveRootMotionSourceByID(ActiveCrowdControlSourceId);
		ActiveCrowdControlSourceId = 0;
	}
}

// =============================================================================
// 벽을 따라 미끄러지는 건 클릭 이동일 때만
// =============================================================================

float ULOACharacterMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	// ① 클릭 이동 중이면 엔진 그대로 (벽을 따라 미끄러지며 걸어감)
	//    클릭 이동 = 입력 가속도가 있음. 이 게임에서 이동 입력은 클릭 이동(AddMovementInput)뿐이고, 루트 모션 중에는 입력이 아니다.
	//    가속도는 엔진이 이동 기록(Saved Move)에 넣어 서버로 보낸다 → 서버와 내 컴퓨터가 같은 판단
	const bool bClickMoving = !CurrentRootMotion.HasActiveRootMotionSources() && !GetCurrentAcceleration().IsNearlyZero();
	if (bClickMoving)
	{
		return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
	}

	// ② 벽처럼 선 면이 아니면(바닥 쪽 경사, 보스 머리 위, 천장) 엔진 그대로 → 머리 위에 떨어져도 미끄러져 내려온다
	//    "벽처럼 선 면" = 수직에서 45도 이내 (면의 방향 Z가 -0.707 ~ 0.707)
	if (FMath::Abs(Normal.Z) > WallSlideNormalZ)
	{
		return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
	}

	// ③ 회피·기상기·돌진·띄움·경직(루트 모션)이 벽이나 보스 몸에 닿음:
	//    옆으로 빠지는 이동은 버리고, 위아래(떨어지는 것)만 남긴다.
	//      땅 위(회피): 위아래 이동이 없으니 닿은 자리에서 멈춤
	//      공중(띄움): 벽에 붙은 채 그대로 떨어짐 (벽에 걸려 허공에 멈추지 않게)
	//    예) 보스 중심에서 30cm 옆을 겨냥해 회피 → 몸 막이에 닿은 자리에서 멈춤 (엔진 원래: 몸을 타고 돌아 옆으로 빠져나감)
	//    땅 위면 위아래가 0이라 엔진이 그대로 0을 돌려준다 (안 움직임). 부딪힘 알림은 엔진이 이미 이 함수를 부르기 전에 보냈다
	return Super::SlideAlongSurface(FVector(0.f, 0.f, Delta.Z), Time, Normal, Hit, bHandleImpact);
}

void ULOACharacterMovementComponent::SetIgnoreBodyBlockers(bool bIgnore)
{
	if (bIgnore == bIgnoringBodyBlockers)
	{
		return;
	}

	UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return;
	}

	Capsule->SetCollisionResponseToChannel(LOACollision::BodyBlock, bIgnore ? ECR_Ignore : DefaultBodyBlockResponse.GetValue());
	bIgnoringBodyBlockers = bIgnore;
}
