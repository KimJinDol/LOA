// LOAProjectile.h
// [배관] 투사체. 앞으로 날아가다 부딪힌 대상에게 Payloads(데미지 등)를 준다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP 만드는 법]
//   1) 이 클래스를 부모로 BP를 만든다. (예: BP_Projectile_Arrow)
//   2) 겉모습: 메시/나이아가라를 Collision 아래에 붙인다. (겉모습 컴포넌트의 콜리전은 끈다)
//   3) 크기: Collision(구)의 Sphere Radius
//   4) 속도: Movement(ProjectileMovement)의 Initial Speed / Max Speed (같은 값으로)
//   5) 사거리, 관통, 누구를 맞힐지: Class Defaults의 LOA|Projectile 칸
//   데미지는 여기서 정하지 않는다. → 이 BP를 소환하는 GA의 Spawn 모듈 Payloads (스킬마다 다른 배율)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   [서버] Spawn 모듈이 소환 (InitSkillActor로 Context/Payloads를 받은 뒤 FinishSpawning)
//     ├ PostInitializeComponents : 겹침 알림 구독 + 벽 설정 적용
//     │                             (소환 위치에 이미 겹친 대상도 이때 알림이 온다 → BeginPlay보다 먼저 구독)
//     ├ BeginPlay                 : 수명 = Range / 속도 (사거리만큼 날아가면 사라짐)
//     ├ 대상과 겹침 → OnCollisionOverlap
//     │     시전자 기준 Filter 통과 + 처음 맞는 대상 → 맞은 지점 계산 → Payloads 적용
//     │     맞힌 수가 MaxHitCount에 도달 → 사라짐
//     └ 벽에 막힘 (bStopOnWall) → OnMovementStopped → 사라짐
//
//   [클라] 복제로 생긴 같은 액터. 이동은 각자 계산 + 서버 위치로 보정. 겹침이 와도 아무것도 안 한다.
//          서버가 Destroy하면 클라의 액터도 사라진다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [콜리전]
//   Collision(구) : Query Only. Pawn = Overlap (플레이어/허수아비/보스 몸통)
//                   WorldStatic = Block (bStopOnWall) 또는 Ignore
//                   나머지 = Ignore
//   Pawn을 Block이 아니라 Overlap으로 하는 이유: 관통(MaxHitCount 2 이상)일 때 대상을 지나쳐 계속 날아가야 한다.
//   ※ 바닥도 보통 WorldStatic이다. 바닥에 닿는 높이로 소환하면 바로 멈춘다.
//     → 캐릭터 기준 Offset Z를 0 이상으로 두거나(캐릭터 위치 = 캡슐 중심, 바닥에서 약 90) bStopOnWall을 끈다.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Skill/LOASkillActor.h"
#include "Library/LOACombatLibrary.h"   // ELOATargetFilter를 값으로 가지므로 include
#include "LOAProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS(Blueprintable)
class LOA_API ALOAProjectile : public ALOASkillActor
{
	GENERATED_BODY()

public:
	ALOAProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	//~ End AActor Interface

private:
	/** 겹침 시작 (서버/클라 둘 다 오지만 서버만 처리) */
	UFUNCTION()
	void OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 벽(WorldStatic)에 막혀 이동이 멈춤 (bStopOnWall일 때만 온다) */
	UFUNCTION()
	void OnMovementStopped(const FHitResult& ImpactResult);

	/** [서버] 이 대상을 맞힐지 판단하고, 맞히면 Payloads 적용 */
	void TryHit(AActor* Target);

	// ---------- 컴포넌트 ----------

	/** 판정 구. 루트. 크기는 BP에서 Sphere Radius로 */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Projectile")
	TObjectPtr<USphereComponent> Collision;

	/** 이동. 속도는 BP에서 Initial Speed / Max Speed로. 중력 0, 진행 방향을 바라봄 */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	// ---------- 데이터 (BP의 Class Defaults에서) ----------

	/** 사거리(cm). 이만큼 날아가면 사라진다. 수명 = Range / Initial Speed. 0 이하면 사거리 제한 없음 (벽이나 맞힘으로만 사라짐) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Projectile", meta = (ClampMin = "0"))
	float Range = 1500.f;

	/**
	 * 몇 명까지 맞히나. 1 = 처음 맞은 대상에서 사라짐 (기본). 3 = 3명까지 관통. 0 = 제한 없이 관통 (사거리 끝까지)
	 * 같은 대상은 한 번만 맞는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Projectile", meta = (ClampMin = "0"))
	int32 MaxHitCount = 1;

	/** 누구를 맞히나. 시전자 기준. 기본 Enemy (아군 힐 투사체면 Ally) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Projectile")
	ELOATargetFilter Filter = ELOATargetFilter::Enemy;

	/** 벽(WorldStatic)에 막히면 사라지나. 기본 true. false면 벽을 통과한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Projectile")
	bool bStopOnWall = true;

	// ---------- 런타임 (서버) ----------

	/** 이미 맞힌 대상 (같은 대상 중복 방지) */
	TArray<TWeakObjectPtr<AActor>> HitActors;
};
