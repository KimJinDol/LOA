// LOARootMotionSource_Arc.cpp

#include "Character/Player/LOARootMotionSource_Arc.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARootMotionSource_Arc)

namespace
{
	/** 날아가는 궤적에서 "땅에 닿으면 끝"을 보기 시작하는 시각 (초). 출발 순간은 아직 땅 위라서 바로 끝나지 않게 */
	constexpr float LandCheckMinTime = 0.1f;
}

FLOARootMotionSource_Arc::FLOARootMotionSource_Arc()
{
	// 엔진 JumpForce와 같은 설정: 다른 이동을 덮어쓰고, 위로 가는 속도가 조금만 있어도 땅에서 떠오른다
	Priority = 500;
	AccumulateMode = ERootMotionAccumulateMode::Override;
	bInLocalSpace = false;
	Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);

	// 끝나면(시간 다 됨, 땅에 닿음, 주인이 지움) 속도 0 → 미끄러지지 않는다 (2026-10-21 사용자 결정)
	FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	FinishVelocityParams.SetVelocity = FVector::ZeroVector;
}

FVector FLOARootMotionSource_Arc::GetRelativeLocation(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

	// 옆: 일정한 속도 (Lerp)
	const FVector Side = Direction.GetSafeNormal2D() * (Distance * Alpha);

	// 높이: 꼭대기가 없으면(경직) 그대로
	if (ApexHeight <= KINDA_SMALL_NUMBER)
	{
		return Side;
	}

	// 땅 기준 높이 y(a) = H − K × (a − P)², y(0) = h0, y(1) = 0
	//   h0 < H : P = 1 ÷ (1 + √(H ÷ (H − h0))), K = H ÷ (1 − P)²
	//   h0 = H : P = 0 (꼭대기에서 바로 떨어짐), K = H
	//   예) H 250, h0 0 → P 0.5, K 1000 → a 0.25에서 y = 250 − 1000 × 0.0625 = 187.5
	const float H = ApexHeight;
	const float H0 = FMath::Clamp(StartHeight, 0.f, H);
	const float Rise = H - H0;
	const float Peak = Rise > KINDA_SMALL_NUMBER ? 1.f / (1.f + FMath::Sqrt(H / Rise)) : 0.f;
	const float K = H / FMath::Square(1.f - Peak);
	const float HeightAboveGround = H - K * FMath::Square(Alpha - Peak);

	// 출발점 기준이라 출발 높이를 뺀다 (a 0 → 0, a 1 → −h0)
	return Side + FVector(0.f, 0.f, HeightAboveGround - H0);
}

FRootMotionSource* FLOARootMotionSource_Arc::Clone() const
{
	return new FLOARootMotionSource_Arc(*this);
}

bool FLOARootMotionSource_Arc::Matches(const FRootMotionSource* Other) const
{
	// 서버와 내 컴퓨터가 건 소스를 짝짓는다. 숫자는 서버가 정해 복제하므로 같다 (아주 작은 오차만 허용)
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	const FLOARootMotionSource_Arc* OtherArc = static_cast<const FLOARootMotionSource_Arc*>(Other);
	return Direction.Equals(OtherArc->Direction, 0.01f)
		&& FMath::IsNearlyEqual(Distance, OtherArc->Distance, 1.f)
		&& FMath::IsNearlyEqual(StartHeight, OtherArc->StartHeight, 1.f)
		&& FMath::IsNearlyEqual(ApexHeight, OtherArc->ApexHeight, 1.f);
}

bool FLOARootMotionSource_Arc::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	// 숫자 외의 상태(시간 등)는 부모가 비교한다
	return FRootMotionSource::MatchesAndHasSameState(Other) && Matches(Other);
}

bool FLOARootMotionSource_Arc::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	// 시간 등 상태를 서버 것으로 맞춘다 (부모). 우리 숫자는 처음부터 같으니 따로 옮길 것이 없다
	return FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup);
}

void FLOARootMotionSource_Arc::PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();

	if (Duration > UE_SMALL_NUMBER && MovementTickTime > UE_SMALL_NUMBER)
	{
		// 이번 이동 계산 동안 가야 할 만큼 = (다음 시각 위치 − 지금 시각 위치) → 속도로 바꿔 엔진에 넘긴다 (엔진 JumpForce와 같은 방식)
		const float CurrentAlpha = GetTime() / Duration;
		const float TargetAlpha = (GetTime() + SimulationTime) / Duration;
		const FVector Delta = GetRelativeLocation(TargetAlpha) - GetRelativeLocation(CurrentAlpha);
		RootMotionParams.Set(FTransform(Delta / MovementTickTime));

		// 날아가는 궤적인데 땅에 닿았다 (땅이 더 높았음) → 여기서 끝 (속도 0). 서버와 내 컴퓨터가 같은 판단
		if (ApexHeight > KINDA_SMALL_NUMBER && GetTime() >= FMath::Min(LandCheckMinTime, Duration * 0.5f) && MoveComponent.IsMovingOnGround())
		{
			Status.SetFlag(ERootMotionSourceStatusFlags::Finished);
		}
	}

	SetTime(GetTime() + SimulationTime);
}

bool FLOARootMotionSource_Arc::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!FRootMotionSource::NetSerialize(Ar, Map, bOutSuccess))
	{
		return false;
	}

	Ar << Direction;
	Ar << Distance;
	Ar << StartHeight;
	Ar << ApexHeight;

	bOutSuccess = true;
	return true;
}

UScriptStruct* FLOARootMotionSource_Arc::GetScriptStruct() const
{
	return FLOARootMotionSource_Arc::StaticStruct();
}

FString FLOARootMotionSource_Arc::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u] FLOARootMotionSource_Arc %s dist %.0f h0 %.0f apex %.0f T %.2f"),
		LocalID, *InstanceName.GetPlainNameString(), Distance, StartHeight, ApexHeight, Duration);
}

void FLOARootMotionSource_Arc::AddReferencedObjects(FReferenceCollector& Collector)
{
	FRootMotionSource::AddReferencedObjects(Collector);
}
