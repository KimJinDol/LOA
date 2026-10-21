// LOACursorLibrary.cpp

#include "Library/LOACursorLibrary.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACursorLibrary)

bool ULOACursorLibrary::GetCursorHitResult(const APlayerController* PC, ECollisionChannel TraceChannel, FHitResult& OutHit)
{
	OutHit = FHitResult();
	if (!PC)
	{
		return false;
	}

	// ① 커서 아래 트레이스
	if (PC->GetHitResultUnderCursor(TraceChannel, false, OutHit) && OutHit.bBlockingHit)
	{
		return true;
	}

	// ② 가상 바닥 평면. 높이는 조종 중인 캐릭터 발밑. (캐릭터가 없으면 평면 높이를 정할 수 없음)
	const APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	FVector PlaneLocation;
	if (!GetCursorLocationOnPlane(PC, GetFootZ(Pawn), PlaneLocation))
	{
		return false;
	}

	OutHit = FHitResult();
	OutHit.Location = PlaneLocation;
	OutHit.ImpactPoint = PlaneLocation;
	OutHit.bBlockingHit = false;   // 실제로 맞은 물체는 없음 (평면 대체값 표시)
	return true;
}

bool ULOACursorLibrary::GetCursorLocationOnPlane(const APlayerController* PC, float PlaneZ, FVector& OutLocation)
{
	if (!PC)
	{
		return false;
	}

	// 화면의 커서 위치 → 월드 공간의 광선 (카메라 위치에서 커서 방향으로 뻗는 선)
	FVector RayOrigin;
	FVector RayDirection;
	if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;   // 마우스가 화면 밖 등
	}

	// 광선이 아래를 향해야 바닥 평면과 만난다. (탑쿼터뷰에서는 항상 아래를 향함)
	if (RayDirection.Z > -KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// 광선 위의 점 = RayOrigin + RayDirection × T. 그 점의 Z가 PlaneZ가 되는 T를 구한다.
	const float T = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (T <= 0.f)
	{
		return false;   // 평면이 카메라 뒤쪽 (카메라가 바닥보다 아래에 있는 비정상 상황)
	}

	OutLocation = RayOrigin + RayDirection * T;
	return true;
}

float ULOACursorLibrary::GetFootZ(const AActor* Actor)
{
	if (!Actor)
	{
		return 0.f;
	}

	float FootZ = Actor->GetActorLocation().Z;

	// 캐릭터 위치는 캡슐 중심이다. 발밑은 캡슐 절반 높이만큼 아래.
	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			FootZ -= Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	return FootZ;
}
