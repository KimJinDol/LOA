// LOAServerJudgeSubsystem.cpp

#include "Combat/LOAServerJudgeSubsystem.h"
#include "Combat/Telegraph/LOATelegraphSubsystem.h"
#include "Combat/Telegraph/LOATelegraphTypes.h"
#include "Combat/Telegraph/LOATelegraphSettings.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOANetTime.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAServerJudgeSubsystem)

namespace LOAServerJudge
{
	/**
	 * [클라 예고 장판] CC 먼저 반응을 되돌리기 전 여유 (초). 피해자 판정의 보고 시간 여유(ULOAVictimJudgeSubsystem ReportTimeMargin)와 같은 값.
	 * 되돌리기 = 서버 판정까지(편도 + 예고 여유) + GE가 오는 편도 + 이 값
	 */
	constexpr double CrowdControlTimeoutMargin = 0.3;
}

ULOAServerJudgeSubsystem* ULOAServerJudgeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOAServerJudgeSubsystem>() : nullptr;
}

// =============================================================================
// 서버
// =============================================================================

void ULOAServerJudgeSubsystem::Judge(const FLOAAttackInfo& Attack, const TArray<FLOATargetHit>& Hits, TArray<FLOAJudgeResult>& OutResults) const
{
	// 서버가 찾은 대상 = 서버가 아는 위치로 모양 안 → 전부 "맞을 자리"
	for (const FLOATargetHit& Hit : Hits)
	{
		const AActor* Target = Hit.Target;
		FLOAJudgeResult& Result = OutResults.AddDefaulted_GetRef();
		Result.Hit = Hit;
		Result.bInRange = true;
		Result.TargetLocation = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		Result.SeenTime = Attack.HitServerTime + GetSeenOffset(Cast<APawn>(Target), Attack.Timing);
	}
}

double ULOAServerJudgeSubsystem::GetSeenOffset(const APawn* Target, ELOAHitJudgeTiming Timing)
{
	// 리슨 호스트 본인, 플레이어가 아닌 대상(보스, 허수아비): 서버 화면 = 자기 화면
	if (!Target || Target->IsLocallyControlled())
	{
		return 0.0;
	}

	// 예고 장판: 그 사람 화면의 "다 참" = 판정 시각 − (편도 + 여유). 예고 시계와 같은 값
	//   (ULOATelegraphSubsystem::GetTelegraphTimeNow, 원격 클라 = 편도 + Lead Margin. 자르지 않음)
	if (Timing == ELOAHitJudgeTiming::Telegraph)
	{
		return -(LOANetTime::GetPlayerOneWayLatency(Target) + GetDefault<ULOATelegraphSettings>()->LeadMargin);
	}

	// 보스 모션: 보정 없음 (헤더 [그 사람 기준 시각])
	return 0.0;
}

// =============================================================================
// 클라: 예고 장판이 내 화면에서 다 참
// =============================================================================

void ULOAServerJudgeSubsystem::JudgeTelegraphLocal(const FLOATelegraphData& Data)
{
	// 원격 클라만 (리슨 호스트·서버는 서버 판정이 곧 자기 화면) + 예고가 꽉 차는 장판만 (보스 모션 장판은 피해자 판정 요청이 따로 온다)
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client || Data.bSyncWithBossMotion || !OnLocalHit.IsBound())
	{
		return;
	}

	APlayerController* LocalPC = World->GetFirstPlayerController();
	APawn* MyPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
	if (!MyPawn)
	{
		return;
	}

	// 내 몸이 모양 안인가 (서버 판정과 같은 모양·자리·방향. 서버가 판정 순간 아는 내 위치 = 지금 내 위치)
	const FVector MyLocation = MyPawn->GetActorLocation();
	if (!ULOACombatLibrary::IsBodyInShape(Data.Shape, Data.Location, Data.Yaw, MyLocation,
		MyPawn->GetSimpleCollisionRadius(), MyPawn->GetSimpleCollisionHalfHeight()))
	{
		return;
	}

	// 맞을 자리 → 판정 매니저 (저가 먼저 → 못 막았으면 CC 먼저 반응)
	//   본 시각 = 지금 서버 시각 추정값 (= 서버의 기준: 판정 시각 − (편도 + 여유)), 공격 기준점 = 예고 자리
	//   CC 되돌리기 = 서버 판정까지(편도 + 앞당김 여유) + GE가 오는 편도 + 여유
	FLOALocalHit Hit;
	Hit.Attacker = Data.Attacker;
	Hit.AttackTags = Data.AttackTags;
	Hit.AttackOrigin = Data.Location;
	Hit.HitServerTime = Data.HitServerTime;
	Hit.SeenTime = LOANetTime::GetServerTimeNow(World);
	Hit.MyPawn = MyPawn;
	Hit.MyLocation = MyLocation;
	Hit.CrowdControlEffect = Data.CrowdControlEffect;
	Hit.CrowdControlTimeout = static_cast<float>(LOANetTime::GetOneWayLatency(World) * 2.0
		+ GetDefault<ULOATelegraphSettings>()->LeadMargin + LOAServerJudge::CrowdControlTimeoutMargin);
	OnLocalHit.Execute(Hit);
}

// =============================================================================
// 월드
// =============================================================================

bool ULOAServerJudgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOAServerJudgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 예고 판정 알림. 예고 담당이 먼저 만들어지게 한다
	//   원격 클라인지는 알림 때 본다 (월드가 만들어질 때는 네트워크 역할이 아직 정해지지 않았을 수 있다)
	if (ULOATelegraphSubsystem* Telegraphs = Collection.InitializeDependency<ULOATelegraphSubsystem>())
	{
		TelegraphHitHandle = Telegraphs->OnTelegraphHit.AddUObject(this, &ThisClass::JudgeTelegraphLocal);
	}
}

void ULOAServerJudgeSubsystem::Deinitialize()
{
	if (ULOATelegraphSubsystem* Telegraphs = ULOATelegraphSubsystem::Get(this))
	{
		Telegraphs->OnTelegraphHit.Remove(TelegraphHitHandle);
	}
	TelegraphHitHandle.Reset();
	OnLocalHit.Unbind();

	Super::Deinitialize();
}
