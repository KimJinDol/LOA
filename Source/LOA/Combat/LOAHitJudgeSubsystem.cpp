// LOAHitJudgeSubsystem.cpp

#include "Combat/LOAHitJudgeSubsystem.h"
#include "Combat/LOACombatRPCComponent.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting_Shape.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"
#include "Core/LOANetTime.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAHitJudgeSubsystem)

namespace LOAHitJudge
{
	/**
	 * 피해자 보고 마감 (판정 시각 뒤 초). 이 안에 안 온 보고는 서버 판정 결과를 쓴다.
	 * 정상 보고는 판정 뒤 왕복 지연 정도에 도착한다. (요청이 편도, 보고가 편도. 핑 200ms → 약 0.2초)
	 * 1초는 순간적인 랙까지 덮는 여유.
	 */
	constexpr double ReportWindow = 1.0;

	/**
	 * [검증] 보고 위치가 판정 순간의 서버 위치에서 이만큼(cm, 바닥 거리) 넘게 다르면 클라 판정을 믿지 않고 서버 결과를 쓴다.
	 * 정상 차이 = 지연 동안 움직인 거리. 이동 속도 600cm/s × 0.5초. 예) 핑 200ms 모션 공격 ≈ 600 × 0.2 = 120cm
	 */
	constexpr float MaxReportDistance = 300.f;

	/** [클라] 위치 기록을 몇 초치 남길까. 마감(1초)보다 조금 길게 (그보다 늦게 온 요청은 어차피 서버가 처리) */
	constexpr double HistoryLength = 1.5;
}

ULOAHitJudgeSubsystem* ULOAHitJudgeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOAHitJudgeSubsystem>() : nullptr;
}

// =============================================================================
// 입구 (서버)
// =============================================================================

int32 ULOAHitJudgeSubsystem::JudgeAndApply(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
	const TArray<FLOAEffectPayload>& Payloads, ELOAHitJudgeTiming Timing)
{
	// 결과가 남는 일은 서버만. 시전자가 나가서 ASC가 사라졌으면 줄 수 없다. (IsValid: "파괴 예정"도 거름)
	AActor* Source = Context.SourceActor;
	if (!Targeting || !Source || !Source->HasAuthority() || !IsValid(Context.SourceASC) || Payloads.IsEmpty())
	{
		return 0;
	}

	// ① 서버 판정 (지금 서버가 아는 위치)
	TArray<FLOATargetHit> Hits;
	Targeting->FindTargets(Context, Hits);

	// ② 피해자 판정이면 원격 플레이어는 빼서 보고를 기다린다.
	ULOAHitJudgeSubsystem* Subsystem = Get(Source);
	const ULOASkillTargeting_Shape* ShapeTargeting = Cast<ULOASkillTargeting_Shape>(Targeting);
	if (Subsystem && ShapeTargeting && ShapeTargeting->GetHitJudge() == ELOAHitJudgeMode::Victim)
	{
		FLOAHitJudgeRequest Request;
		if (Subsystem->HoldRemoteVictims(Targeting, Context, Hits, Request) > 0)
		{
			const FLOAHitShape& Shape = ShapeTargeting->GetShape();
			const double Now = LOANetTime::GetServerTimeNow(Subsystem->GetWorld());
			const float Yaw = Source->GetActorRotation().Yaw;

			Request.Context = Context;
			Request.Payloads = Payloads;
			Request.Shape = Shape;
			Request.ShapeCenter = ULOACombatLibrary::GetShapeCenter(Source, Shape);
			Request.Yaw = Yaw;
			Request.DeadlineServerTime = Now + LOAHitJudge::ReportWindow;

			// 번호: 서버 안에서만 겹치지 않으면 된다. 1부터 (0 = 번호 없음)
			Subsystem->LastRequestId = (Subsystem->LastRequestId == MAX_int32) ? 1 : Subsystem->LastRequestId + 1;
			const int32 RequestId = Subsystem->LastRequestId;

			// 그 플레이어에게만 판정 요청. 판정 위치 = 그 플레이어가 공격을 본 순간의 위치
			//   예고 = 기록에서 지금(판정 서버 시각)의 위치 / 모션 = 요청을 받은 순간의 위치 (헤더 [판정 시각])
			const bool bJudgeOnArrival = (Timing == ELOAHitJudgeTiming::Animation);
			for (const FLOAHitJudgeVictim& Entry : Request.Victims)
			{
				const APawn* Victim = Entry.Victim.Get();
				const APlayerController* PC = Victim ? Cast<APlayerController>(Victim->GetController()) : nullptr;
				ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr;
				if (!CombatRPC)
				{
					continue;	// 보낼 수 없으면 마감에 서버 결과
				}

				CombatRPC->ClientRequestHitJudge(RequestId, Shape, Request.ShapeCenter, Yaw, Now, bJudgeOnArrival);

				LOA_LOG(Source, Log, TEXT("[HitJudge] #%d : request to %s, server time %.2f (%s)"),
					RequestId, *GetNameSafe(Victim), Now, bJudgeOnArrival ? TEXT("Animation: judge on arrival") : TEXT("Telegraph: judge at server time"));
			}

			// 측정용 요약: 물어본 인원 중 서버가 보기에 모양 안 / 밖 (밖인데 물어본 수 = 줄일 수 있는 RPC)
			int32 NumServerHit = 0;
			for (const FLOAHitJudgeVictim& Entry : Request.Victims)
			{
				NumServerHit += Entry.bServerHit ? 1 : 0;
			}
			LOA_LOG(Source, Log, TEXT("[HitJudge] #%d : asked %d remote player(s) (server: %d in shape, %d outside), %d target(s) applied now"),
				RequestId, Request.Victims.Num(), NumServerHit, Request.Victims.Num() - NumServerHit, Hits.Num());
			Subsystem->Requests.Add(RequestId, MoveTemp(Request));
		}
	}

	// ③ 결과가 바로 난 대상 (몬스터, 허수아비, 리슨 호스트 본인, Server 방식의 전원)
	ULOACombatLibrary::ApplyPayloads(Context, Hits, Payloads);
	return Hits.Num();
}

int32 ULOAHitJudgeSubsystem::HoldRemoteVictims(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
	TArray<FLOATargetHit>& Hits, FLOAHitJudgeRequest& OutRequest) const
{
	const ULOASkillTargeting_Shape* ShapeTargeting = Cast<ULOASkillTargeting_Shape>(Targeting);
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!ShapeTargeting || !GameState)
	{
		return 0;
	}

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;

		// 리슨 호스트 본인은 서버 화면이 곧 자기 화면 → 서버 판정 그대로 (Hits에 남겨 바로 적용)
		if (!Pawn || Pawn->IsLocallyControlled())
		{
			continue;
		}

		// 이 판정의 대상인가 (살아 있음 + Filter. 예: Enemy → 시전자의 적). 아니면 보고가 와도 버려진다.
		if (!ULOACombatLibrary::IsTargetable(Pawn)
			|| !ULOACombatLibrary::PassesFilter(Context.Caster, Pawn, ShapeTargeting->GetFilter()))
		{
			continue;
		}

		// 서버 결과 적기. 맞았으면 Hits에서 빼서 지금은 주지 않는다.
		const int32 HitIndex = Hits.IndexOfByPredicate([Pawn](const FLOATargetHit& Hit) { return Hit.Target == Pawn; });

		FLOAHitJudgeVictim& Entry = OutRequest.Victims.AddDefaulted_GetRef();
		Entry.Victim = Pawn;
		Entry.bServerHit = (HitIndex != INDEX_NONE);
		Entry.ServerLocation = Pawn->GetActorLocation();

		if (HitIndex != INDEX_NONE)
		{
			Hits.RemoveAt(HitIndex);
		}
	}
	return OutRequest.Victims.Num();
}

void ULOAHitJudgeSubsystem::ReceiveReport(APawn* Victim, int32 RequestId, bool bReportedHit, const FVector& ReportedLocation)
{
	// ① 기다리는 중인 (판정 번호, 그 플레이어)가 있는가.
	//   없음 = 중복 보고 / 틀린 번호 / 마감 지남 / 대상이 아닌 플레이어 → 버림
	FLOAHitJudgeRequest* Request = Requests.Find(RequestId);
	const int32 Index = Request
		? Request->Victims.IndexOfByPredicate([Victim](const FLOAHitJudgeVictim& Candidate) { return Candidate.Victim.Get() == Victim; })
		: INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		LOA_LOG(this, Verbose, TEXT("[HitJudge] #%d : report ignored from %s"), RequestId, *GetNameSafe(Victim));
		return;
	}

	const FLOAHitJudgeVictim Entry = Request->Victims[Index];
	Request->Victims.RemoveAtSwap(Index);

	// ② 클라 판정을 쓴다. 의심할 경우는 하나: "안 맞음"인데 서버는 맞았다고 봄 (= 피했다는 주장)
	//   "맞음"은 거짓이어도 본인만 손해, 둘 다 "안 맞음"이면 다툴 게 없다 → 계산 없이 그대로
	bool bHit = bReportedHit;
	if (!bReportedHit && Entry.bServerHit)
	{
		// ③ 검증: 거리 (보고 위치가 서버 위치에서 너무 멀지 않은가, 바닥 거리) + 각도·거리 (그 위치가 정말 모양 밖인가)
		const float ReportDistance = FVector::Dist2D(ReportedLocation, Entry.ServerLocation);
		const bool bNearServer = ReportDistance <= LOAHitJudge::MaxReportDistance;
		const bool bOutsideShape = bNearServer && !ULOACombatLibrary::IsBodyInShape(Request->Shape, Request->ShapeCenter, Request->Yaw,
			ReportedLocation, Victim->GetSimpleCollisionRadius());

		if (bOutsideShape)
		{
			LOA_LOG(this, Log, TEXT("[HitJudge] #%d : %s reported MISS (server said HIT), verified (%.0fcm from server)"),
				RequestId, *GetNameSafe(Victim), ReportDistance);
		}
		else
		{
			bHit = true;	// 검증 실패 → 서버 결과
			LOA_LOG(this, Warning, TEXT("[HitJudge] #%d : %s reported MISS but %s (%.0fcm from server, max %.0f). Using server result HIT"),
				RequestId, *GetNameSafe(Victim), bNearServer ? TEXT("reported location is inside the shape") : TEXT("reported location is too far"),
				ReportDistance, LOAHitJudge::MaxReportDistance);
		}
	}
	else
	{
		LOA_LOG(this, Log, TEXT("[HitJudge] #%d : %s reported %s (server said %s)"),
			RequestId, *GetNameSafe(Victim), bReportedHit ? TEXT("HIT") : TEXT("MISS"), Entry.bServerHit ? TEXT("HIT") : TEXT("MISS"));
	}

	// 적용은 목록 정리가 끝난 뒤 복사본으로. (적용 중에 다른 판정이 목록에 추가되면 Request 포인터가 무효가 될 수 있다)
	const FLOAHitJudgeRequest Snapshot = *Request;
	if (Request->Victims.IsEmpty())
	{
		Requests.Remove(RequestId);	// 모두 보고했으면 판정을 지운다.
	}

	if (bHit)
	{
		ApplyToVictim(Snapshot, Victim);
	}
}

void ULOAHitJudgeSubsystem::ApplyToVictim(const FLOAHitJudgeRequest& Request, APawn* Victim) const
{
	// 기다리는 사이 죽었거나 시전자가 나갔으면 안 준다.
	if (!ULOACombatLibrary::IsTargetable(Victim) || !IsValid(Request.Context.SourceASC))
	{
		return;
	}

	// 맞은 지점 = Shape Targeting과 같은 규칙 (대상 캡슐 표면에서 모양 기준점과 가장 가까운 곳 → 데미지 숫자 위치)
	TArray<FLOATargetHit> Hits;
	FLOATargetHit& Hit = Hits.AddDefaulted_GetRef();
	Hit.Target = Victim;
	Hit.bHasHitPoint = ULOACombatLibrary::MakeClosestHitResult(Victim, Request.ShapeCenter, Hit.HitResult);

	ULOACombatLibrary::ApplyPayloads(Request.Context, Hits, Request.Payloads);
}

void ULOAHitJudgeSubsystem::ResolveExpired(double Now)
{
	// ① 마감이 지난 판정을 목록에서 꺼낸다. (적용 중에 목록이 바뀌어도 안전하게, 적용은 꺼낸 뒤에)
	TArray<TPair<int32, FLOAHitJudgeRequest>> Expired;
	for (auto It = Requests.CreateIterator(); It; ++It)
	{
		if (It.Value().DeadlineServerTime <= Now)
		{
			Expired.Emplace(It.Key(), MoveTemp(It.Value()));
			It.RemoveCurrent();
		}
	}

	// ② 보고가 안 온 사람 → 적어 둔 서버 결과대로
	for (const TPair<int32, FLOAHitJudgeRequest>& Pair : Expired)
	{
		for (const FLOAHitJudgeVictim& Entry : Pair.Value.Victims)
		{
			APawn* Victim = Entry.Victim.Get();
			LOA_LOG(this, Log, TEXT("[HitJudge] #%d : no report from %s, using server result %s"),
				Pair.Key, *GetNameSafe(Victim), Entry.bServerHit ? TEXT("HIT") : TEXT("MISS"));

			if (Entry.bServerHit && Victim)
			{
				ApplyToVictim(Pair.Value, Victim);
			}
		}
	}
}

// =============================================================================
// 각 클라
// =============================================================================

void ULOAHitJudgeSubsystem::JudgeLocal(int32 RequestId, const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw,
	double HitServerTime, bool bJudgeOnArrival)
{
	// 이 컴퓨터의 플레이어와 내 캐릭터
	UWorld* World = GetWorld();
	APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* MyPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
	ULOACombatRPCComponent* CombatRPC = LocalPC ? LocalPC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr;
	if (!MyPawn || !CombatRPC)
	{
		return;
	}

	// 예고 공격인데 내 시계가 아직 판정 시각 전이면, 그 시각까지 기다렸다가 판정한다.
	//   (내 시계는 실제 서버 시각보다 조금(약 0.03초) 늦게 간다. 랙이 거의 없으면 요청이 내 시계로 "조금 뒤"를 가리킬 수 있다.
	//    그 순간 내 화면의 예고가 꽉 차므로, 그때 위치가 맞다)
	const double WaitSeconds = HitServerTime - LOANetTime::GetServerTimeNow(World);
	if (!bJudgeOnArrival && WaitSeconds > 0.005)
	{
		FTimerHandle WaitHandle;
		World->GetTimerManager().SetTimer(WaitHandle, FTimerDelegate::CreateWeakLambda(this,
			[this, RequestId, Shape, ShapeCenter, Yaw, HitServerTime]()
			{
				JudgeLocal(RequestId, Shape, ShapeCenter, Yaw, HitServerTime, false);
			}), static_cast<float>(WaitSeconds), false);
		return;
	}

	// 판정할 내 위치
	//   모션 공격 : 지금 위치 (요청 도착 = 내 화면에서 공격이 닿은 순간)
	//   예고 공격 : 기록에서 판정 서버 시각의 위치 (내 화면에서 예고가 꽉 찬 순간). 기록이 없으면(방금 태어남 등) 지금 위치
	FVector MyLocation = MyPawn->GetActorLocation();
	if (!bJudgeOnArrival)
	{
		FindLocalPositionAt(HitServerTime, MyLocation);
	}

	// 판정 → 보고. (안 오면 서버는 마감에 서버 결과를 쓴다)
	//   맞음   : 번호만 (서버가 검증할 일이 없다)
	//   안 맞음: 번호 + 판정에 쓴 위치 (서버가 "정말 피했나"를 검증할 때 쓴다)
	const bool bHit = ULOACombatLibrary::IsBodyInShape(Shape, ShapeCenter, Yaw, MyLocation, MyPawn->GetSimpleCollisionRadius());
	if (bHit)
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

	LOA_LOG(this, Log, TEXT("[HitJudge] #%d : my judge %s (%s, server time %.2f, now %.2f)"),
		RequestId, bHit ? TEXT("HIT") : TEXT("MISS"), bJudgeOnArrival ? TEXT("on arrival") : TEXT("from history"),
		HitServerTime, LOANetTime::GetServerTimeNow(World));
}

void ULOAHitJudgeSubsystem::RecordLocalPosition(double Now)
{
	const UWorld* World = GetWorld();
	const APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* MyPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
	if (!MyPawn)
	{
		PositionHistory.Reset();	// 캐릭터가 없음(사망, 교체) → 다른 몸의 기록이 섞이지 않게
		return;
	}

	FLOAPositionSample& Sample = PositionHistory.AddDefaulted_GetRef();
	Sample.ServerTime = Now;
	Sample.Location = MyPawn->GetActorLocation();

	// 오래된 칸 버리기 (앞에서부터)
	int32 NumOld = 0;
	while (NumOld < PositionHistory.Num() && PositionHistory[NumOld].ServerTime < Now - LOAHitJudge::HistoryLength)
	{
		++NumOld;
	}
	if (NumOld > 0)
	{
		PositionHistory.RemoveAt(0, NumOld, EAllowShrinking::No);
	}
}

bool ULOAHitJudgeSubsystem::FindLocalPositionAt(double ServerTime, FVector& OutLocation) const
{
	if (PositionHistory.IsEmpty())
	{
		return false;
	}

	// 기록보다 이르면 가장 오래된 칸, 늦으면(아직 안 온 시각) 가장 최근 칸
	if (ServerTime <= PositionHistory[0].ServerTime)
	{
		OutLocation = PositionHistory[0].Location;
		return true;
	}
	if (ServerTime >= PositionHistory.Last().ServerTime)
	{
		OutLocation = PositionHistory.Last().Location;
		return true;
	}

	// 그 시각을 끼고 있는 두 칸 사이를 시간 비율로 섞는다.
	//   예) [101.18 → (0,0), 101.21 → (30,0)], 101.2 → 비율 2/3 → (20,0)
	for (int32 Index = 1; Index < PositionHistory.Num(); ++Index)
	{
		const FLOAPositionSample& After = PositionHistory[Index];
		if (After.ServerTime < ServerTime)
		{
			continue;
		}
		const FLOAPositionSample& Before = PositionHistory[Index - 1];
		const double Span = After.ServerTime - Before.ServerTime;
		const float Alpha = Span > KINDA_SMALL_NUMBER ? static_cast<float>((ServerTime - Before.ServerTime) / Span) : 1.f;
		OutLocation = FMath::Lerp(Before.Location, After.Location, Alpha);
		return true;
	}

	OutLocation = PositionHistory.Last().Location;
	return true;
}

// =============================================================================
// 매 프레임
// =============================================================================

bool ULOAHitJudgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOAHitJudgeSubsystem::Deinitialize()
{
	// 맵이 내려가는 중이면 남은 판정은 데미지 없이 버린다.
	Requests.Reset();
	PositionHistory.Reset();

	Super::Deinitialize();
}

bool ULOAHitJudgeSubsystem::IsTickable() const
{
	// 클라: 늘 (위치 기록) / 서버: 보고를 기다리는 판정이 있을 때만
	const UWorld* World = GetWorld();
	if (!World || IsTemplate())
	{
		return false;
	}
	return World->GetNetMode() == NM_Client || !Requests.IsEmpty();
}

TStatId ULOAHitJudgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULOAHitJudgeSubsystem, STATGROUP_Tickables);
}

void ULOAHitJudgeSubsystem::Tick(float DeltaTime)
{
	const UWorld* World = GetWorld();
	const double Now = LOANetTime::GetServerTimeNow(World);

	if (World && World->GetNetMode() == NM_Client)
	{
		RecordLocalPosition(Now);
	}
	else
	{
		ResolveExpired(Now);
	}
}
