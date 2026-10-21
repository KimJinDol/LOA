// LOARootMotionSource_Arc.h
// [배관] CC로 밀리거나 날아가는 궤적. "이 시간 동안 정확히 이 길로" 움직이는 루트 모션 소스 (엔진 확장).
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 직접 만들었나] (2026-10-21 사용자 결정: 물리처럼 움직이면 안 됨, 공중 시간은 일정, 꼭대기 높이는 고정)
//   엔진의 JumpForce는 출발 높이 = 도착 높이인 대칭 포물선만 된다.
//   공중에서 또 맞으면 "지금 높이 → 꼭대기 → 땅"으로 출발·도착 높이가 다르다 → 직접 만든다.
//
// [궤적] 시간 T 동안 (Alpha = 지난 시간 ÷ T, 0 → 1)
//   옆   : 방향 × 거리 × Alpha  (Lerp, 일정한 속도)
//   높이 : 땅 기준 높이 y(Alpha) = 꼭대기 − A × (Alpha − 꼭대기 시각)²   (포물선)
//          y(0) = 출발 높이 h0, y(1) = 0(땅), 가장 높은 곳 = 꼭대기 H
//          꼭대기 시각 = 1 ÷ (1 + √(H ÷ (H − h0)))     예) h0 0 → 0.5 (가운데) / h0 200, H 250 → 0.31 / h0 = H → 0 (바로 떨어짐)
//   경직(꼭대기 0, 출발 0)이면 높이는 그대로, 옆으로만 일정하게 → "0.15초 동안 정확히 100cm"
//
//   예) 띄움 T 1.0초, H 250, 땅에서 맞음, 거리 300 → 0.5초에 250 높이, 1.0초에 300cm 옆 땅에 닿음
//
// [끝나는 때]
//   ① T가 다 됨 → 엔진이 지움. 끝날 때 속도 0 (미끄러짐 없음. 아직 공중이면 = 낭떠러지 밖 → 그 자리부터 보통 낙하)
//   ② 날아가는 궤적(꼭대기 > 0)인데 땅에 닿음 (땅이 더 높았음) → 스스로 끝남
//   ③ 주인이 지움 (ULOACharacterMovementComponent::StopCrowdControlMove)
//
// [네트워크] 서버와 맞은 사람 컴퓨터가 "같은 숫자"로 각자 건다 (ULOACharacterMovementComponent가 숫자를 복제).
//   엔진이 두 쪽 소스를 Matches로 짝지어 시간을 맞춘다 → 숫자가 같아야 짝이 된다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"
#include "LOARootMotionSource_Arc.generated.h"

USTRUCT()
struct LOA_API FLOARootMotionSource_Arc : public FRootMotionSource
{
	GENERATED_BODY()

	FLOARootMotionSource_Arc();
	virtual ~FLOARootMotionSource_Arc() override {}

	/** 수평 방향 (길이 1, 월드 기준) */
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	/** 수평으로 가는 거리 (cm) */
	UPROPERTY()
	float Distance = 0.f;

	/** 출발할 때 땅 기준 높이 (cm). 땅에서 맞으면 0, 공중에서 또 맞으면 그때 높이 */
	UPROPERTY()
	float StartHeight = 0.f;

	/** 꼭대기 높이 (땅 기준, cm). 0이면 높이 변화 없음 (경직 밀림) */
	UPROPERTY()
	float ApexHeight = 0.f;

	/** 출발점 기준 위치 (Alpha 0 ~ 1). 옆 = Lerp, 높이 = 포물선 */
	FVector GetRelativeLocation(float Alpha) const;

	//~ Begin FRootMotionSource Interface
	virtual FRootMotionSource* Clone() const override;
	virtual bool Matches(const FRootMotionSource* Other) const override;
	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;
	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;
	virtual void PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent) override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	//~ End FRootMotionSource Interface
};

template<>
struct TStructOpsTypeTraits<FLOARootMotionSource_Arc> : public TStructOpsTypeTraitsBase2<FLOARootMotionSource_Arc>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
