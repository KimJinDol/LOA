// LOAPositionHistoryComponent.cpp

#include "Character/Monster/LOAPositionHistoryComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPositionHistoryComponent)

ULOAPositionHistoryComponent::ULOAPositionHistoryComponent()
{
	// 이동 계산(CharacterMovement, PrePhysics) 뒤에 이번 프레임의 최종 자리를 적는다
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// 기록은 서버 안에서만 쓴다
	SetIsReplicatedByDefault(false);
}

void ULOAPositionHistoryComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// 클라는 기록하지 않는다
		SetComponentTickEnabled(false);
		return;
	}

	// 위치를 보낼 때 그 서버 시각도 같이 보내게 한다 (기본은 0이 간다. 헤더 [내 화면의 보스가 …])
	//   이 칸은 서버의 PreReplication이 읽으므로 서버에서만 켜면 된다
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->bNetworkAlwaysReplicateTransformUpdateTimestamp = true;
		}
	}
}

void ULOAPositionHistoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	// ① 이번 프레임 자리. 시각은 엔진이 위치 시각에 쓰는 것과 같은 월드 시각
	const double Now = World->GetTimeSeconds();
	FLOAPositionHistorySample& Sample = Samples.AddDefaulted_GetRef();
	Sample.ServerTime = Now;
	Sample.Location = Owner->GetActorLocation();
	Sample.Yaw = Owner->GetActorRotation().Yaw;

	// ② HistoryLength보다 오래된 칸은 버린다 (가장 최근 칸은 남김)
	int32 NumOld = 0;
	while (NumOld < Samples.Num() - 1 && Samples[NumOld].ServerTime < Now - HistoryLength)
	{
		++NumOld;
	}
	if (NumOld > 0)
	{
		Samples.RemoveAt(0, NumOld, EAllowShrinking::No);
	}
}

bool ULOAPositionHistoryComponent::FindAt(double ServerTime, FVector& OutLocation, float& OutYaw) const
{
	if (Samples.IsEmpty())
	{
		return false;
	}

	// 위치 시각은 float로 와서 아주 작은 오차가 있다 (서버 1000초 근처에서 약 0.00006초) → 1ms 여유
	constexpr double TimeTolerance = 0.001;

	// 그 시각 이하 중 가장 늦은 칸. 최근 칸부터 거꾸로 본다 (되감기는 보통 0.1 ~ 0.3초라 앞쪽에서 찾는다)
	for (int32 Index = Samples.Num() - 1; Index >= 0; --Index)
	{
		if (Samples[Index].ServerTime <= ServerTime + TimeTolerance)
		{
			OutLocation = Samples[Index].Location;
			OutYaw = Samples[Index].Yaw;
			return true;
		}
	}

	// 기록보다 옛날 → 가장 오래된 칸 (헤더 [찾는 규칙])
	OutLocation = Samples[0].Location;
	OutYaw = Samples[0].Yaw;
	return true;
}
