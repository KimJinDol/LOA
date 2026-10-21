// LOAProjectile.cpp

#include "Actors/Skill/LOAProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Core/LOALog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAProjectile)

ALOAProjectile::ALOAProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버가 계산한 위치를 클라이언트에 보내 보정한다. (클라도 Movement로 직접 움직이고, 어긋나면 맞춰진다)
	SetReplicateMovement(true);

	// ---------- 판정 구 (루트) ----------
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);		// 물리 시뮬레이션 없음, 겹침/막힘 검사만
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);		// 대상 몸통: 지나치며 알림 (관통 가능)
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);	// 벽: 막힘 (PostInitializeComponents에서 bStopOnWall대로 다시 설정)
	Collision->SetGenerateOverlapEvents(true);
	SetRootComponent(Collision);

	// ---------- 이동 ----------
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;		// 이 구를 움직인다 (이동하며 겹침/막힘 검사)
	Movement->InitialSpeed = 1500.f;
	Movement->MaxSpeed = 1500.f;
	Movement->ProjectileGravityScale = 0.f;		// 직선 비행
	Movement->bRotationFollowsVelocity = true;	// 날아가는 방향을 바라봄
	// 처음 속도 방향 = 액터의 앞 방향 (Velocity를 따로 안 주면 앞 방향 × Initial Speed)
}

void ALOAProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 벽 설정. 생성자에서는 BP 값을 아직 모르므로 여기서 적용한다.
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, bStopOnWall ? ECR_Block : ECR_Ignore);

	// 구독을 BeginPlay가 아니라 여기서 하는 이유:
	// 소환 위치에 이미 겹쳐 있는 대상(바로 앞의 적)의 알림은 BeginPlay 직전에 온다. BeginPlay에서 구독하면 놓친다.
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnCollisionOverlap);
	Movement->OnProjectileStop.AddDynamic(this, &ThisClass::OnMovementStopped);
}

void ALOAProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 사거리 → 수명. 서버만 정한다. (서버가 Destroy하면 복제로 클라의 액터도 사라진다)
	if (HasAuthority() && Range > 0.f && Movement->InitialSpeed > 0.f)
	{
		SetLifeSpan(Range / Movement->InitialSpeed);
	}

	// 설정 누락 알림 (서버만. 클라는 Payloads를 받지 않으므로 항상 비어 있다)
	if (HasAuthority() && GetSkillPayloads().IsEmpty())
	{
		LOA_LOG(this, Warning, TEXT("Projectile has no Payloads. It will hit but do nothing. Check the Spawn module."));
	}
}

void ALOAProjectile::OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 판정은 서버만. 클라에서는 겹쳐도 아무것도 안 한다. (데미지 숫자는 서버의 GE 결과로 뜬다)
	if (!HasAuthority())
	{
		return;
	}

	TryHit(OtherActor);
}

void ALOAProjectile::OnMovementStopped(const FHitResult& ImpactResult)
{
	// 벽에 막혔다. 클라도 같은 벽에서 멈추지만, 사라지는 건 서버가 정한다.
	if (HasAuthority())
	{
		Destroy();
	}
}

void ALOAProjectile::TryHit(AActor* Target)
{
	// 이미 다 맞혀서 사라지는 중이면 무시. (같은 이동에서 겹침 알림이 여러 개 올 수 있음)
	if (IsActorBeingDestroyed() || !Target)
	{
		return;
	}

	// ① 맞을 수 있는 대상인가. ASC가 없는 Pawn(소품 등)과 시체는 그냥 지나친다. (맞힌 수에 세지 않음)
	//    범위 공격(FindTargetsInSphere)과 같은 함수 → 규칙은 한 곳
	if (!ULOACombatLibrary::IsTargetable(Target))
	{
		return;
	}

	// ② 시전자 기준으로 맞힐 대상인가. (시전자 자신, Filter에 안 맞는 편은 여기서 걸러진다)
	//    기준이 투사체가 아니라 시전자인 이유는 LOASkillActor.h의 [복사본의 SourceActor와 Caster]
	AActor* Caster = GetSkillCaster();
	if (!ULOACombatLibrary::PassesFilter(Caster, Target, Filter))
	{
		return;
	}

	// ③ 같은 대상은 한 번만
	if (HitActors.Contains(Target))
	{
		return;
	}
	HitActors.Add(Target);

	// ④ 맞은 지점 = 대상 몸통에서 투사체와 가장 가까운 곳 (데미지 숫자 위치. 범위 공격과 같은 방식)
	FLOATargetHit Hit;
	Hit.Target = Target;
	Hit.bHasHitPoint = ULOACombatLibrary::MakeClosestHitResult(Target, GetActorLocation(), Hit.HitResult);

	LOA_LOG(this, Log, TEXT("[Projectile] hit %s (hit count %d)"), *GetNameSafe(Target), HitActors.Num());

	// ⑤ Payloads 적용 (부모)
	ApplySkillPayloads(TArray<FLOATargetHit>{ Hit });

	// ⑥ 맞힌 수가 다 찼으면 사라짐. 0이면 제한 없음.
	if (MaxHitCount > 0 && HitActors.Num() >= MaxHitCount)
	{
		Destroy();
	}
}
