// LOAVictimJudgeSubsystem.cpp

#include "Combat/LOAVictimJudgeSubsystem.h"
#include "Combat/LOACombatRPCComponent.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting_Shape.h"
#include "AbilitySystem/Effects/LOAGE_CrowdControl.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"
#include "Core/LOANetTime.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAVictimJudgeSubsystem)

namespace LOAVictimJudge
{
	/**
	 * 피해자 보고 마감 (판정 시각 뒤 초). 이 안에 안 온 보고는 서버 판정 결과를 쓴다.
	 * 정상 보고는 판정 뒤 왕복 지연 정도에 도착한다. (요청이 편도, 보고가 편도. 핑 200ms → 약 0.2초)
	 * 1초는 순간적인 랙까지 덮는 여유.
	 */
	constexpr double ReportWindow = 1.0;

	/**
	 * [검증 ①] 보고가 늦어도 되는 여유 (초). "안 맞음" 보고는 판정 시각 + 그 플레이어 핑(왕복) + 이 값 안에 와야 믿는다.
	 * (보고를 일부러 늦추고 그사이 걸어 나가기 막기)
	 * 0.3 = 핑 숫자가 늦게 따라오는 오차(측정 최대 0.07) + 서버 프레임 + 핑 200에서 보고 재전송 한 번 정도
	 * 클라의 CC 먼저 반응 되돌리기 여유로도 쓴다
	 */
	constexpr double ReportTimeMargin = 0.3;

	/**
	 * [검증 ③] 보고 위치 ↔ 보고가 도착한 순간 서버가 아는 그 플레이어 위치 (cm, 바닥 거리). 이 안이면 믿는다.
	 * 보고와 이동은 같이 도착하므로 정상이면 거의 같다. 차이 = 서버가 이동을 한두 프레임 늦게 받은 만큼
	 * 150 = 회피(약 1667cm/s) 약 0.09초. 측정 최대 130cm (2026-10-28) 위
	 */
	constexpr float ReportLocationTolerance = 150.f;

	/** [검증 ③] 위치가 안 맞을 때 서버 위치가 따라오길 기다리는 최대 (초). 이동 패킷이 사라지면 서버 위치가 잠깐 멈춰 있다 */
	constexpr double ReportCatchUpTime = 0.1;

	/** [디버그 측정] 서버 위치 기록을 몇 초치 남길까. 마감(1초)보다 조금 길게 */
	constexpr double HistoryLength = 1.5;

	/**
	 * "본 시각" 늦춤 최대 (초). 본 시각 = 판정 시각 + 그 플레이어의 편도 지연
	 * 0.2 = 핑 400ms까지 덮음. 더 늦는 사람은 그만큼 일찍 눌러야 한다. 클라 자기 판정도 같은 값으로 자른다
	 */
	constexpr double MaxSeenDelay = 0.2;

	/** [검증 ③] 보고 위치 ↔ 서버가 지금 아는 그 플레이어 위치 (cm, 바닥 거리). 플레이어가 없으면 아주 큰 값 */
	float GetReportDistance(const APawn* Victim, const FVector& ReportedLocation)
	{
		return Victim ? FVector::Dist2D(ReportedLocation, Victim->GetActorLocation()) : TNumericLimits<float>::Max();
	}

	// ---------- [디버그 측정] (2026-10-28) 예고 앞당김 뒤에도 클라·서버 위치가 어긋나는 원인 찾기. 원인을 찾으면 지운다 ----------

	/** 루트 모션(회피, CC 궤적, Move 모듈)으로 움직이는 중인가 */
	bool IsInRootMotion(const APawn* Pawn)
	{
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		return Movement && Movement->HasRootMotionSources();
	}

	/** 서버 기록이 보고 위치에 "왔다"고 볼 거리 (cm) */
	constexpr float DebugReachDistance = 30.f;
}

ULOAVictimJudgeSubsystem* ULOAVictimJudgeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOAVictimJudgeSubsystem>() : nullptr;
}

// =============================================================================
// 서버: 요청
// =============================================================================

int32 ULOAVictimJudgeSubsystem::HoldRemoteVictims(const ULOASkillTargeting* Targeting, const FLOAAttackInfo& Attack, TArray<FLOATargetHit>& Hits)
{
	const ULOASkillTargeting_Shape* ShapeTargeting = Cast<ULOASkillTargeting_Shape>(Targeting);
	const AActor* Source = Attack.Context.SourceActor;
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!ShapeTargeting || !Source || !GameState)
	{
		return 0;
	}

	FLOAHitJudgeRequest Request;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;

		// 리슨 호스트 본인은 서버 화면이 곧 자기 화면 → 서버 판정 그대로 (Hits에 남김)
		if (!Pawn || Pawn->IsLocallyControlled())
		{
			continue;
		}

		// 이 공격의 대상인가 (살아 있음 + Filter. 예: Enemy → 시전자의 적). 아니면 보고가 와도 버려진다.
		if (!ULOACombatLibrary::IsTargetable(Pawn)
			|| !ULOACombatLibrary::PassesFilter(Attack.Context.Caster, Pawn, ShapeTargeting->GetFilter()))
		{
			continue;
		}

		// 서버 결과 적기. 맞았으면 Hits에서 빼서 서버 판정으로 가지 않게 한다.
		const int32 HitIndex = Hits.IndexOfByPredicate([Pawn](const FLOATargetHit& Hit) { return Hit.Target == Pawn; });

		FLOAHitJudgeVictim& Entry = Request.Victims.AddDefaulted_GetRef();
		Entry.Victim = Pawn;
		Entry.bServerHit = (HitIndex != INDEX_NONE);
		Entry.ServerLocation = Pawn->GetActorLocation();

#if !UE_BUILD_SHIPPING
		// [디버그 측정] 판정 순간 서버가 아는 위치 + 그 위치가 얼마나 묵었나 (마지막 이동을 받은 뒤 지난 시간, 서버 프레임 길이)
		{
			const UWorld* World = GetWorld();
			const ACharacter* Character = Cast<ACharacter>(Pawn);
			const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
			const FNetworkPredictionData_Server_Character* ServerData = Movement ? Movement->GetPredictionData_Server_Character() : nullptr;
			const double SinceLastMove = (ServerData && World) ? World->GetTimeSeconds() - ServerData->ServerTimeStampLastServerMove : -1.0;
			LOA_LOG(this, Log, TEXT("[HitJudgeDebug] server judge %.3f : %s at (%.0f, %.0f), server one-way %.3f, root motion %s | last move received %.3fs ago, server frame %.3fs"),
				LOANetTime::GetServerTimeNow(World), *GetNameSafe(Pawn), Entry.ServerLocation.X, Entry.ServerLocation.Y,
				LOANetTime::GetPlayerOneWayLatency(Pawn), LOAVictimJudge::IsInRootMotion(Pawn) ? TEXT("yes") : TEXT("no"),
				SinceLastMove, World ? World->GetDeltaSeconds() : -1.f);
		}
#endif

		if (HitIndex != INDEX_NONE)
		{
			Hits.RemoveAt(HitIndex);
		}
	}

	const int32 NumVictims = Request.Victims.Num();
	if (NumVictims == 0)
	{
		return 0;	// 기다릴 사람이 없다
	}

	// 피해자 판정에만 필요한 것: 모양 (클라 판정·보고 검증), 기준점 (맞은 지점), 방향, 마감
	Request.AttackId = Attack.AttackId;
	Request.Shape = ShapeTargeting->GetShape();
	Request.ShapeCenter = ULOACombatLibrary::GetShapeCenter(Source, Request.Shape);
	Request.Yaw = Source->GetActorRotation().Yaw;
	Request.HitServerTime = Attack.HitServerTime;
	Request.DeadlineServerTime = Attack.HitServerTime + LOAVictimJudge::ReportWindow;
	Requests.Add(Attack.AttackId, MoveTemp(Request));
	return NumVictims;
}

void ULOAVictimJudgeSubsystem::SendVictimRequests(const FLOAAttackInfo& Attack)
{
	const FLOAHitJudgeRequest* Request = Requests.Find(Attack.AttackId);
	if (!Request)
	{
		return;
	}

	// 보낼 묶음 (모든 대상에게 같다)
	//   CC GE (띄움·경직 = ULOAGE_CrowdControl 자식). 공격 하나에 CC 하나로 본다 (여러 개면 첫 번째). 클라가 "맞음"이면 먼저 반응한다
	//   공격한 보스·속성: 맞은 사람의 저가 컴포넌트가 구간을 찾는다
	FLOAHitJudgeRequestData RequestData;
	RequestData.RequestId = Request->AttackId;
	RequestData.Shape = Request->Shape;
	RequestData.ShapeCenter = Request->ShapeCenter;
	RequestData.Yaw = Request->Yaw;
	RequestData.HitServerTime = Request->HitServerTime;
	RequestData.CrowdControlEffect = ULOACombatLibrary::FindPayloadEffect(Attack.Payloads, ULOAGE_CrowdControl::StaticClass());
	RequestData.Attacker = Attack.Context.Caster;
	RequestData.AttackTags = Attack.Context.AttackTags;

	// 그 플레이어에게만 판정 요청. 판정 위치 = 요청을 받은 순간의 위치 (= 내 화면에서 보스 공격이 닿은 순간)
	int32 NumServerHit = 0;
	for (const FLOAHitJudgeVictim& Entry : Request->Victims)
	{
		NumServerHit += Entry.bServerHit ? 1 : 0;

		const APawn* Victim = Entry.Victim.Get();
		const APlayerController* PC = Victim ? Cast<APlayerController>(Victim->GetController()) : nullptr;
		ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr;
		if (!CombatRPC)
		{
			continue;	// 보낼 수 없으면 마감에 서버 결과
		}

		CombatRPC->ClientRequestHitJudge(RequestData);

		LOA_LOG(this, Log, TEXT("[HitJudge] #%d : request to %s, server time %.2f (judge on arrival)"),
			Request->AttackId, *GetNameSafe(Victim), Request->HitServerTime);
	}

	// 측정용 요약: 물어본 인원 중 서버가 보기에 모양 안 / 밖 (밖인데 물어본 수 = 줄일 수 있는 RPC)
	LOA_LOG(this, Log, TEXT("[HitJudge] #%d : asked %d remote player(s) (server: %d in shape, %d outside)"),
		Request->AttackId, Request->Victims.Num(), NumServerHit, Request->Victims.Num() - NumServerHit);
}

// =============================================================================
// 서버: 보고
// =============================================================================

bool ULOAVictimJudgeSubsystem::TakeVictim(int32 RequestId, APawn* Victim, FLOAHitJudgeRequest& OutSnapshot, FLOAHitJudgeVictim& OutEntry)
{
	FLOAHitJudgeRequest* Request = Requests.Find(RequestId);
	const int32 Index = Request
		? Request->Victims.IndexOfByPredicate([Victim](const FLOAHitJudgeVictim& Candidate) { return Candidate.Victim.Get() == Victim; })
		: INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutEntry = Request->Victims[Index];
	Request->Victims.RemoveAtSwap(Index);

	// 결과는 목록 정리가 끝난 뒤 복사본으로 올린다. (올리는 중에 다른 요청이 목록에 추가되면 Request 포인터가 무효가 될 수 있다)
	OutSnapshot = *Request;
	OutSnapshot.Victims.Reset();
	if (Request->Victims.IsEmpty())
	{
		Requests.Remove(RequestId);	// 모두 보고했으면 요청을 지운다.
	}
	return true;
}

void ULOAVictimJudgeSubsystem::ReceiveReport(APawn* Victim, int32 RequestId, bool bReportedHit, const FVector& ReportedLocation)
{
	// ① 기다리는 중인 (판정 번호, 그 플레이어)가 있는가. 없으면 버림
	FLOAHitJudgeRequest Snapshot;
	FLOAHitJudgeVictim Entry;
	if (!TakeVictim(RequestId, Victim, Snapshot, Entry))
	{
		LOA_LOG(this, Verbose, TEXT("[HitJudge] #%d : report ignored from %s"), RequestId, *GetNameSafe(Victim));
		return;
	}

#if !UE_BUILD_SHIPPING
	if (!bReportedHit)
	{
		DebugLogServerCatchUp(Snapshot, Entry, ReportedLocation);	// [디버그 측정]
	}
#endif

	// ② 보고를 믿을 수 있나 (헤더 [클라 판정 + 서버 검증])
	const ELOAReportCheck Check = ValidateReport(Snapshot, Entry, bReportedHit, ReportedLocation);

	// 위치만 안 맞음 → 서버 위치가 따라오길 잠깐 기다린다 (ResolveCatchUps가 매 틱 다시 본다)
	if (Check == ELOAReportCheck::Wait)
	{
		FLOAHitJudgeCatchUp& CatchUp = CatchUps.AddDefaulted_GetRef();
		CatchUp.Request = MoveTemp(Snapshot);
		CatchUp.Entry = Entry;
		CatchUp.ReportedLocation = ReportedLocation;
		CatchUp.GiveUpServerTime = LOANetTime::GetServerTimeNow(GetWorld()) + LOAVictimJudge::ReportCatchUpTime;
		return;
	}

	// ③ 결과 올리기
	//   믿음: 클라 결과 그대로 ("맞음"이면 저가 컴포넌트가 맞은 시각만 적는다)
	//   거절: 서버 결과 (맞을 자리. 서버 저가 컴포넌트가 판단)
	if (Check == ELOAReportCheck::Trust)
	{
		FinishReport(Snapshot, Entry, bReportedHit, /*bClientSaidHit*/ bReportedHit);
	}
	else
	{
		FinishReport(Snapshot, Entry, Entry.bServerHit, /*bClientSaidHit*/ false);
	}
}

void ULOAVictimJudgeSubsystem::ReceiveGuardReport(APawn* Victim, int32 RequestId)
{
	// ① 기다리는 중인 (판정 번호, 그 플레이어)가 있는가. 없으면 버림 (ReceiveReport와 같은 규칙)
	FLOAHitJudgeRequest Snapshot;
	FLOAHitJudgeVictim Entry;
	if (!TakeVictim(RequestId, Victim, Snapshot, Entry))
	{
		LOA_LOG(this, Verbose, TEXT("[HitJudge] #%d : guard report ignored from %s"), RequestId, *GetNameSafe(Victim));
		return;
	}

	// ② "맞을 자리"로 올린다. 막았는지는 판정 매니저의 확정에서 서버 저가 컴포넌트가 같은 기준으로 다시 판단한다 (실패하면 맞음)
	LOA_LOG(this, Log, TEXT("[HitJudge] #%d : %s reported JUST GUARD (the server record decides)"), RequestId, *GetNameSafe(Victim));
	FinishReport(Snapshot, Entry, /*bInRange*/ true, /*bClientSaidHit*/ false);
}

void ULOAVictimJudgeSubsystem::FinishReport(const FLOAHitJudgeRequest& Request, const FLOAHitJudgeVictim& Entry, bool bInRange, bool bClientSaidHit) const
{
	APawn* Victim = Entry.Victim.Get();

	FLOAJudgeResult Result;
	Result.Hit.Target = Victim;
	Result.bInRange = bInRange && Victim != nullptr;	// 그사이 나간 플레이어는 "피함" (줄 대상이 없다)
	Result.bClientSaidHit = bClientSaidHit;
	Result.TargetLocation = Entry.ServerLocation;
	Result.SeenTime = Request.HitServerTime + GetSeenDelay(Victim);
	if (Result.bInRange)
	{
		// 맞은 지점 = Shape Targeting과 같은 규칙 (대상 캡슐 표면에서 모양 기준점과 가장 가까운 곳 → 데미지 숫자 위치)
		Result.Hit.bHasHitPoint = ULOACombatLibrary::MakeShapeHitResult(Victim, Request.ShapeCenter, Result.Hit.HitResult);
	}

	OnTargetJudged.ExecuteIfBound(Request.AttackId, Result);
}

ELOAReportCheck ULOAVictimJudgeSubsystem::ValidateReport(const FLOAHitJudgeRequest& Request, const FLOAHitJudgeVictim& Entry,
	bool bReportedHit, const FVector& ReportedLocation) const
{
	APawn* Victim = Entry.Victim.Get();
	const int32 RequestId = Request.AttackId;

	// 의심할 경우는 하나: "안 맞음"인데 서버는 맞았다고 봄 (= 피했다는 주장)
	//   "맞음"은 거짓이어도 본인만 손해, 둘 다 "안 맞음"이면 다툴 게 없다 → 계산 없이 믿음
	if (bReportedHit || !Entry.bServerHit)
	{
		LOA_LOG(this, Log, TEXT("[HitJudge] #%d : %s reported %s (server said %s)"),
			RequestId, *GetNameSafe(Victim), bReportedHit ? TEXT("HIT") : TEXT("MISS"), Entry.bServerHit ? TEXT("HIT") : TEXT("MISS"));
		return ELOAReportCheck::Trust;
	}

	// ① 시간: 판정 뒤 (핑 + 여유) 안에 왔나. 정상 보고는 왕복쯤에 도착한다
	const double Elapsed = LOANetTime::GetServerTimeNow(GetWorld()) - Request.HitServerTime;
	const double MaxElapsed = LOANetTime::GetPlayerOneWayLatency(Victim) * 2.0 + LOAVictimJudge::ReportTimeMargin;
	if (Elapsed > MaxElapsed)
	{
		LOA_LOG(this, Warning, TEXT("[HitJudge] #%d : %s reported MISS but too late (%.2fs after hit, max %.2f). Using server result HIT"),
			RequestId, *GetNameSafe(Victim), Elapsed, MaxElapsed);
		return ELOAReportCheck::Reject;
	}

	// ② 모양: 그 위치가 정말 모양 밖인가
	const float Radius = Victim ? Victim->GetSimpleCollisionRadius() : 0.f;
	const float HalfHeight = Victim ? Victim->GetSimpleCollisionHalfHeight() : 0.f;
	if (ULOACombatLibrary::IsBodyInShape(Request.Shape, Request.ShapeCenter, Request.Yaw, ReportedLocation, Radius, HalfHeight))
	{
		LOA_LOG(this, Warning, TEXT("[HitJudge] #%d : %s reported MISS but reported location is inside the shape. Using server result HIT"),
			RequestId, *GetNameSafe(Victim));
		return ELOAReportCheck::Reject;
	}

	// ③ 위치: 서버가 지금 아는 그 플레이어 위치와 맞나. 안 맞으면 바로 정하지 않고 기다린다 (이동 패킷 손실)
	const float Distance = LOAVictimJudge::GetReportDistance(Victim, ReportedLocation);
	if (Distance > LOAVictimJudge::ReportLocationTolerance)
	{
		LOA_LOG(this, Log, TEXT("[HitJudge] #%d : %s reported MISS, %.0fcm from server now (max %.0f, %.2fs after hit). Waiting up to %.2fs for the server position"),
			RequestId, *GetNameSafe(Victim), Distance, LOAVictimJudge::ReportLocationTolerance, Elapsed, LOAVictimJudge::ReportCatchUpTime);
		return ELOAReportCheck::Wait;
	}

	LOA_LOG(this, Log, TEXT("[HitJudge] #%d : %s reported MISS (server said HIT), verified (%.2fs after hit, max %.2f / %.0fcm from server now, max %.0f)"),
		RequestId, *GetNameSafe(Victim), Elapsed, MaxElapsed, Distance, LOAVictimJudge::ReportLocationTolerance);
	return ELOAReportCheck::Trust;
}

void ULOAVictimJudgeSubsystem::ResolveCatchUps(double Now)
{
	// ① 결과가 난 보고를 목록에서 꺼낸다 (올리기는 꺼낸 뒤에. ResolveExpired와 같은 이유)
	TArray<TPair<FLOAHitJudgeCatchUp, bool>> Done;	// bool = 맞을 자리 (서버 위치가 안 따라옴)
	for (int32 Index = CatchUps.Num() - 1; Index >= 0; --Index)
	{
		const FLOAHitJudgeCatchUp& CatchUp = CatchUps[Index];
		const APawn* Victim = CatchUp.Entry.Victim.Get();
		const float Distance = LOAVictimJudge::GetReportDistance(Victim, CatchUp.ReportedLocation);
		const bool bCaughtUp = Distance <= LOAVictimJudge::ReportLocationTolerance;
		if (!bCaughtUp && Now < CatchUp.GiveUpServerTime)
		{
			continue;	// 더 기다린다
		}
		const double Waited = Now - (CatchUp.GiveUpServerTime - LOAVictimJudge::ReportCatchUpTime);
		if (bCaughtUp)
		{
			LOA_LOG(this, Log, TEXT("[HitJudge] #%d : %s reported MISS (server said HIT), verified after waiting %.2fs (%.0fcm from server now)"),
				CatchUp.Request.AttackId, *GetNameSafe(Victim), Waited, Distance);
		}
		else
		{
			LOA_LOG(this, Warning, TEXT("[HitJudge] #%d : %s reported MISS but the server position did not reach it in %.2fs (%.0fcm, max %.0f). Using server result HIT"),
				CatchUp.Request.AttackId, *GetNameSafe(Victim), Waited, Distance, LOAVictimJudge::ReportLocationTolerance);
		}
		Done.Emplace(MoveTemp(CatchUps[Index]), !bCaughtUp);
		CatchUps.RemoveAtSwap(Index);
	}

	// ② 따라옴 → 클라 결과(피함) / 안 옴 → 서버 결과(맞을 자리)
	for (const TPair<FLOAHitJudgeCatchUp, bool>& Pair : Done)
	{
		FinishReport(Pair.Key.Request, Pair.Key.Entry, Pair.Value, /*bClientSaidHit*/ false);
	}
}

void ULOAVictimJudgeSubsystem::ResolveExpired(double Now)
{
	// ① 마감이 지난 요청을 목록에서 꺼낸다. (올리는 중에 목록이 바뀌어도 안전하게, 올리기는 꺼낸 뒤에)
	TArray<FLOAHitJudgeRequest> Expired;
	for (auto It = Requests.CreateIterator(); It; ++It)
	{
		if (It.Value().DeadlineServerTime <= Now)
		{
			Expired.Add(MoveTemp(It.Value()));
			It.RemoveCurrent();
		}
	}

	// ② 보고가 안 온 사람 → 적어 둔 서버 결과대로
	//    맞을 자리였으면 서버 저가 컴포넌트가 판단한다 (누름은 왔는데 "막음" 보고만 늦은 경우)
	for (const FLOAHitJudgeRequest& Request : Expired)
	{
		for (const FLOAHitJudgeVictim& Entry : Request.Victims)
		{
			LOA_LOG(this, Log, TEXT("[HitJudge] #%d : no report from %s, using server result %s"),
				Request.AttackId, *GetNameSafe(Entry.Victim.Get()), Entry.bServerHit ? TEXT("HIT") : TEXT("MISS"));
			FinishReport(Request, Entry, Entry.bServerHit, /*bClientSaidHit*/ false);
		}
	}
}

double ULOAVictimJudgeSubsystem::GetSeenDelay(const APawn* Victim)
{
	// 리슨 호스트: 서버 화면 = 자기 화면 (피해자 판정 대상이 아니지만 안전하게)
	if (!Victim || Victim->IsLocallyControlled())
	{
		return 0.0;
	}
	// 피해자 판정은 모션 공격뿐 (예고 장판은 서버 판정): 보스 모션을 편도만큼 늦게 본다
	return FMath::Min(LOANetTime::GetPlayerOneWayLatency(Victim), LOAVictimJudge::MaxSeenDelay);
}

// =============================================================================
// 각 클라
// =============================================================================

void ULOAVictimJudgeSubsystem::JudgeLocal(const FLOAHitJudgeRequestData& Request)
{
	// 받은 묶음 (FLOAHitJudgeRequestData)
	const int32 RequestId = Request.RequestId;
	const FLOAHitShape& Shape = Request.Shape;
	const FVector& ShapeCenter = Request.ShapeCenter;
	const float Yaw = Request.Yaw;
	const double HitServerTime = Request.HitServerTime;

	// 이 컴퓨터의 플레이어와 내 캐릭터
	UWorld* World = GetWorld();
	APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* MyPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
	ULOACombatRPCComponent* CombatRPC = LocalPC ? LocalPC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr;
	if (!MyPawn || !CombatRPC)
	{
		return;
	}

	// 판정할 내 위치 = 지금 위치 (요청 도착 = 내 화면에서 보스 공격이 닿은 순간. 헤더 [판정 시각])
	const FVector MyLocation = MyPawn->GetActorLocation();
	const bool bHit = ULOACombatLibrary::IsBodyInShape(Shape, ShapeCenter, Yaw, MyLocation,
		MyPawn->GetSimpleCollisionRadius(), MyPawn->GetSimpleCollisionHalfHeight());

	// 맞을 자리 → 판정 매니저 (저가 먼저 → 못 막았으면 CC 먼저 반응). 돌려받은 "막았나"로 보고를 고른다
	//   본 시각 = 지금 (요청 도착 = 내 화면에서 공격이 닿은 순간). 서버 기준과 같은 최대치로 자른다
	//   공격 기준점 = 모양 기준점 (서버의 공격 기준점 ULOASkillTargeting_Shape::GetAttackOrigin과 같은 값)
	//   CC 되돌리기 = 왕복(보고가 가고 GE가 오는 시간) + 보고 검사와 같은 여유
	bool bBlocked = false;
	if (bHit && OnLocalHit.IsBound())
	{
		FLOALocalHit Hit;
		Hit.Attacker = Request.Attacker;
		Hit.AttackTags = Request.AttackTags;
		Hit.AttackOrigin = ShapeCenter;
		Hit.HitServerTime = HitServerTime;
		Hit.SeenTime = FMath::Clamp(LOANetTime::GetServerTimeNow(World), HitServerTime, HitServerTime + LOAVictimJudge::MaxSeenDelay);
		Hit.MyPawn = MyPawn;
		Hit.MyLocation = MyLocation;
		Hit.CrowdControlEffect = Request.CrowdControlEffect;
		Hit.CrowdControlTimeout = static_cast<float>(LOANetTime::GetOneWayLatency(World) * 2.0 + LOAVictimJudge::ReportTimeMargin);
		bBlocked = OnLocalHit.Execute(Hit);
	}

	// 보고 (안 오면 서버는 마감에 서버 결과를 쓴다)
	//   막음   : 번호만 (서버 저가 컴포넌트가 다시 판단)
	//   맞음   : 번호만 (서버가 검증할 일이 없다)
	//   안 맞음: 번호 + 판정에 쓴 위치 (서버가 "정말 피했나"를 검증할 때 쓴다)
	if (bBlocked)
	{
		CombatRPC->ServerReportHitJudgeGuarded(RequestId);
	}
	else if (bHit)
	{
		CombatRPC->ServerReportHitJudgeHit(RequestId);
	}
	else
	{
		CombatRPC->ServerReportHitJudgeMiss(RequestId, MyLocation);
	}

#if ENABLE_DRAW_DEBUG
	// [디버그] LOA.Debug.HitShape: 내가 판정한 모양 + 내 몸 원 (빨강 = 맞음 / 초록 = 안 맞음). 이 컴퓨터 화면에만
	if (ULOACombatLibrary::IsHitShapeDebugEnabled())
	{
		const FColor Color = bHit ? FColor::Red : FColor::Green;
		ULOACombatLibrary::DrawDebugHitShape(World, Shape, ShapeCenter, Yaw, Color);
		DrawDebugCircle(World, MyLocation, MyPawn->GetSimpleCollisionRadius(), 24, Color, false, 1.f, 0, 2.f,
			FVector::ForwardVector, FVector::RightVector, false);
	}
#endif

#if !UE_BUILD_SHIPPING
	// [디버그 측정] 클라가 판정에 쓴 순간·위치와 지금
	{
		const double NowDebug = LOANetTime::GetServerTimeNow(World);
		const APlayerState* MyPlayerState = MyPawn->GetPlayerState();
		LOA_LOG(this, Log, TEXT("[HitJudgeDebug] #%d : client judge at %.3f (arrival) pos (%.0f, %.0f) | hit %.3f, my one-way %.3f, ping %.0fms | root motion %s"),
			RequestId, NowDebug, MyLocation.X, MyLocation.Y, HitServerTime, LOANetTime::GetOneWayLatency(World),
			MyPlayerState ? MyPlayerState->GetPingInMilliseconds() : -1.f, LOAVictimJudge::IsInRootMotion(MyPawn) ? TEXT("yes") : TEXT("no"));
	}
#endif

	LOA_LOG(this, Log, TEXT("[HitJudge] #%d : my judge %s (on arrival, server time %.2f, now %.2f)"),
		RequestId, bBlocked ? TEXT("JUST GUARD") : (bHit ? TEXT("HIT") : TEXT("MISS")), HitServerTime, LOANetTime::GetServerTimeNow(World));
}

// =============================================================================
// 매 프레임
// =============================================================================

bool ULOAVictimJudgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOAVictimJudgeSubsystem::Deinitialize()
{
	// 맵이 내려가는 중이면 남은 요청은 결과 없이 버린다.
	Requests.Reset();
	CatchUps.Reset();
	OnTargetJudged.Unbind();
	OnLocalHit.Unbind();

	Super::Deinitialize();
}

bool ULOAVictimJudgeSubsystem::IsTickable() const
{
	// 서버: 보고를 기다리는 요청이나 위치를 기다리는 보고가 있을 때만. 클라는 할 일 없음 (판정은 요청이 올 때 바로)
	const UWorld* World = GetWorld();
	if (!World || IsTemplate())
	{
		return false;
	}
	return World->GetNetMode() != NM_Client && (!Requests.IsEmpty() || !CatchUps.IsEmpty());
}

TStatId ULOAVictimJudgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULOAVictimJudgeSubsystem, STATGROUP_Tickables);
}

void ULOAVictimJudgeSubsystem::Tick(float DeltaTime)
{
	// 서버만 (IsTickable)
	const double Now = LOANetTime::GetServerTimeNow(GetWorld());

#if !UE_BUILD_SHIPPING
	DebugRecordServerPositions(Now);	// [디버그 측정]
#endif
	ResolveExpired(Now);
	ResolveCatchUps(Now);
#if !UE_BUILD_SHIPPING
	if (Requests.IsEmpty() && CatchUps.IsEmpty())
	{
		DebugServerHistory.Reset();
	}
#endif
}

#if !UE_BUILD_SHIPPING
// =============================================================================
// [디버그 측정] (2026-10-28) 원인을 찾으면 이 구역째 지운다
// =============================================================================

void ULOAVictimJudgeSubsystem::DebugRecordServerPositions(double Now)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!Pawn || Pawn->IsLocallyControlled())
		{
			continue;
		}
		TArray<FLOAPositionSample>& History = DebugServerHistory.FindOrAdd(Pawn);
		FLOAPositionSample& Sample = History.AddDefaulted_GetRef();
		Sample.ServerTime = Now;
		Sample.Location = Pawn->GetActorLocation();
		History.RemoveAll([Now](const FLOAPositionSample& Old) { return Old.ServerTime < Now - LOAVictimJudge::HistoryLength; });
	}
}

void ULOAVictimJudgeSubsystem::DebugLogServerCatchUp(const FLOAHitJudgeRequest& Request, const FLOAHitJudgeVictim& Entry,
	const FVector& ReportedLocation) const
{
	// 서버 기록(판정 시각 뒤)에서 보고 위치에 처음 온 시각 = 서버가 클라를 실제로 얼마나 늦게 따라오나
	APawn* Victim = Entry.Victim.Get();
	const double Now = LOANetTime::GetServerTimeNow(GetWorld());
	const TArray<FLOAPositionSample>* History = Victim ? DebugServerHistory.Find(Victim) : nullptr;

	double ReachTime = -1.0;
	float Closest = TNumericLimits<float>::Max();
	double ClosestTime = 0.0;
	if (History)
	{
		for (const FLOAPositionSample& Sample : *History)
		{
			if (Sample.ServerTime < Request.HitServerTime)
			{
				continue;
			}
			const float Distance = FVector::Dist2D(Sample.Location, ReportedLocation);
			if (Distance < Closest)
			{
				Closest = Distance;
				ClosestTime = Sample.ServerTime;
			}
			if (ReachTime < 0.0 && Distance <= LOAVictimJudge::DebugReachDistance)
			{
				ReachTime = Sample.ServerTime;
			}
		}
	}

	const float AtJudge = FVector::Dist2D(Entry.ServerLocation, ReportedLocation);
	if (ReachTime >= 0.0)
	{
		LOA_LOG(this, Log, TEXT("[HitJudgeDebug] #%d : reported (%.0f, %.0f) = %.0fcm from server at judge. Server reached it +%.3fs after judge (report arrived +%.3fs, server one-way %.3f)"),
			Request.AttackId, ReportedLocation.X, ReportedLocation.Y, AtJudge, ReachTime - Request.HitServerTime, Now - Request.HitServerTime,
			LOANetTime::GetPlayerOneWayLatency(Victim));
	}
	else
	{
		LOA_LOG(this, Log, TEXT("[HitJudgeDebug] #%d : reported (%.0f, %.0f) = %.0fcm from server at judge. Server NOT there yet (closest %.0fcm at +%.3fs, report arrived +%.3fs, server one-way %.3f)"),
			Request.AttackId, ReportedLocation.X, ReportedLocation.Y, AtJudge, History ? Closest : -1.f, ClosestTime - Request.HitServerTime,
			Now - Request.HitServerTime, LOANetTime::GetPlayerOneWayLatency(Victim));
	}
}
#endif
