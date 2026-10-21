// LOAGE_CrowdControl.cpp

#include "AbilitySystem/Effects/LOAGE_CrowdControl.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGE_CrowdControl)

FVector ULOAGE_CrowdControl::ComputePushDirection(const FGameplayEffectSpec& Spec, const AActor* Victim)
{
	if (!Victim)
	{
		return FVector::ZeroVector;
	}

	const FGameplayEffectContextHandle& EffectContext = Spec.GetContext();
	const ULOAGE_CrowdControl* CrowdControlDef = Cast<const ULOAGE_CrowdControl>(Spec.Def.Get());

	// 때린 것: 장판·투사체 (ULOACombatLibrary::ApplyPayload가 SourceObject로 기록). 그사이 사라졌으면 보스(EffectCauser)
	const AActor* SourceActor = Cast<AActor>(EffectContext.GetSourceObject());
	const AActor* Causer = EffectContext.GetEffectCauser();
	const AActor* Hitter = SourceActor ? SourceActor : Causer;

	// 때린 것의 앞 방향 (Source Forward, 그리고 기준점에 정확히 서 있을 때의 대체 방향)
	FVector Forward = Hitter ? Hitter->GetActorForwardVector() : -Victim->GetActorForwardVector();
	Forward = Forward.GetSafeNormal2D();

	if (CrowdControlDef && CrowdControlDef->LaunchDirection == ELOALaunchDirection::SourceForward)
	{
		// 예) 보스가 북쪽을 보고 장풍 → 어디 서 있든 북쪽으로. Yaw Offset 90이면 동쪽으로
		return Forward.RotateAngleAxis(CrowdControlDef->YawOffset, FVector::UpVector);
	}

	// Away From Source: 판정 기준점 → 나
	//   기준점 = 맞은 지점을 만들 때 쓴 점 (MakeClosestHitResult의 TraceStart): 장판 중심, 부채꼴 꼭짓점, 투사체 위치
	FVector SourcePoint = FVector::ZeroVector;
	bool bHasSourcePoint = false;
	if (const FHitResult* Hit = EffectContext.GetHitResult())
	{
		SourcePoint = Hit->TraceStart;
		bHasSourcePoint = true;
	}
	else if (Hitter)
	{
		SourcePoint = Hitter->GetActorLocation();
		bHasSourcePoint = true;
	}

	const FVector Away = bHasSourcePoint ? (Victim->GetActorLocation() - SourcePoint).GetSafeNormal2D() : FVector::ZeroVector;

	// 기준점 바로 위에 서 있으면 방향이 없다 → 때린 것의 앞 방향
	return Away.IsNearlyZero() ? Forward : Away;
}
