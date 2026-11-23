// LOACombatRPCComponent.cpp

#include "Combat/LOACombatRPCComponent.h"
#include "UI/LOAHUD.h"
#include "Combat/Telegraph/LOATelegraphSubsystem.h"
#include "Combat/LOAVictimJudgeSubsystem.h"
#include "Combat/LOAJustGuardSubsystem.h"
#include "Combat/LOAJustGuardComponent.h"
#include "Combat/LOACombatNoticeSubsystem.h"

#include "Core/LOANetTime.h"
#include "Core/LOALog.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Engine/HitResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACombatRPCComponent)

ULOACombatRPCComponent::ULOACombatRPCComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 받아서 넘기기만 하므로 매 프레임 할 일이 없다.
	PrimaryComponentTick.bCanEverTick = false;

	// RPC를 쓰려면 복제가 켜져 있어야 한다. (끄면 RPC가 조용히 안 감)
	// 복제할 변수는 없으므로 RPC 통로로만 쓴다.
	SetIsReplicatedByDefault(true);
}

void ULOACombatRPCComponent::BeginPlay()
{
	Super::BeginPlay();

	// [서버] 주인 플레이어가 때린 피해(데미지 숫자), 받은 회복(회복 숫자, 2026-11-16), 맞은 피해(빨간 숫자, apply118)만 받는다. 클라의 PC는 구독하지 않는다 (알림은 서버에서만 온다)
	//   열쇠 = 주인 PlayerState (플레이어 스킬 데미지의 Instigator = ASC의 주인 = PlayerState / 회복의 TargetOwner = 받은 쪽 ASC의 주인 = PlayerState)
	const APlayerController* PC = GetOwner<APlayerController>();
	if (!PC || !PC->HasAuthority() || !PC->PlayerState)
	{
		return;
	}

	if (ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(this))
	{
		CombatNoticeKey = PC->PlayerState;
		DamageDealtHandle = CombatNotice->OnDamageDealtBy(PC->PlayerState).AddUObject(this, &ThisClass::HandleDamageDealt);
		HealReceivedHandle = CombatNotice->OnHealReceivedBy(PC->PlayerState).AddUObject(this, &ThisClass::HandleHealReceived);
		DamageReceivedHandle = CombatNotice->OnDamageReceivedBy(PC->PlayerState).AddUObject(this, &ThisClass::HandleDamageReceived);
	}
}

void ULOACombatRPCComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(this);
	if (CombatNotice && CombatNoticeKey.IsValid())
	{
		CombatNotice->OnDamageDealtBy(CombatNoticeKey.Get()).Remove(DamageDealtHandle);
		CombatNotice->OnHealReceivedBy(CombatNoticeKey.Get()).Remove(HealReceivedHandle);
		CombatNotice->OnDamageReceivedBy(CombatNoticeKey.Get()).Remove(DamageReceivedHandle);
	}
	CombatNoticeKey.Reset();
	DamageDealtHandle.Reset();
	HealReceivedHandle.Reset();
	DamageReceivedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void ULOACombatRPCComponent::HandleDamageDealt(const FLOADamageAppliedInfo& Info)
{
	if (!Info.Target || Info.IncomingDamage <= 0.f)
	{
		return;
	}

	// 숫자 정보로 옮겨 담기 (전에 ULOACombatLibrary::NotifyDamageDealt가 하던 것 그대로)
	FLOACombatTextInfo Number;
	Number.Kind = ELOACombatTextKind::Damage;
	Number.Amount = Info.IncomingDamage;	// 들어온 데미지 전체 (보호막 몫 포함)
	Number.DirectionalAttack = Info.AttackJudgeRecord.DirectionalAttack;
	Number.bCritical = Info.AttackJudgeRecord.bCritical;	// 노랗고 크게 (2026-11-13)
	Number.bWeakPoint = Info.AttackJudgeRecord.bWeakPoint;	// "약점 공격" 글자 (2026-11-18 apply124)
	Number.bBrawlDamage = Info.bBrawlDamage;	// 대난투 색 (2026-11-18 apply125)

	// 맞은 지점 → 대상 기준 상대 위치 (클라의 대상 위치에 더해서 쓴다. 지연으로 위치가 어긋나도 대상에 붙어 보이게)
	// 맞은 지점이 없으면(도트 등) 비워 둔다 → 받은 클라가 내 캐릭터 기준으로 계산
	if (Info.HitResult)
	{
		Number.bHasHitOffset = true;
		Number.HitOffset = Info.HitResult->ImpactPoint - Info.Target->GetActorLocation();
	}

	// 이 플레이어 컴퓨터로만 간다 (리슨 서버 호스트 본인이면 바로 실행)
	ClientShowCombatText(Info.Target, Number);
}

void ULOACombatRPCComponent::HandleHealReceived(const FLOAHealAppliedInfo& Info)
{
	if (!Info.Target || Info.IncomingHealing <= 0.f)
	{
		return;
	}

	// 회복 숫자 = 들어온 양 (사용자 2026-11-16. 넘친 양 포함, 데미지 숫자와 같은 기준). 예) 900/1000에서 300 회복 → "300"
	//   위치는 받은 클라의 레이어가 정한다 (내 캐릭터 캡슐 가운데, apply117). 맞은 지점은 보내지 않는다
	FLOACombatTextInfo Number;
	Number.Kind = ELOACombatTextKind::Heal;
	Number.Amount = Info.IncomingHealing;

	// 이 플레이어 컴퓨터로만 간다 (리슨 서버 호스트 본인이면 바로 실행)
	ClientShowCombatText(Info.Target, Number);
}

void ULOACombatRPCComponent::HandleDamageReceived(const FLOADamageAppliedInfo& Info)
{
	if (!Info.Target || Info.IncomingDamage <= 0.f)
	{
		return;
	}

	// 받은 데미지 숫자 = 들어온 양 (보호막 몫 포함, 내가 때린 숫자와 같은 기준). 예) 보스가 900 → 빨간 "900" (사용자 2026-11-16 apply118)
	//   위치는 받은 클라의 레이어가 정한다 (내 캐릭터 캡슐 가운데). 맞은 지점·백 어택·치명타는 보내지 않는다 (빨간 숫자는 모양이 하나)
	FLOACombatTextInfo Number;
	Number.Kind = ELOACombatTextKind::DamageTaken;
	Number.Amount = Info.IncomingDamage;

	// 이 플레이어 컴퓨터로만 간다 (리슨 서버 호스트 본인이면 바로 실행)
	ClientShowCombatText(Info.Target, Number);
}

// =============================================================================
// 서버 → 이 플레이어 컴퓨터
// =============================================================================

void ULOACombatRPCComponent::ClientShowCombatText_Implementation(AActor* Target, const FLOACombatTextInfo& Info)
{
	// 글자를 띄우는 일은 HUD 담당. 여기는 네트워크로 받아서 넘기기만 한다.
	const APlayerController* PC = GetOwner<APlayerController>();
	if (ALOAHUD* LOAHUD = PC ? PC->GetHUD<ALOAHUD>() : nullptr)
	{
		LOAHUD->ShowCombatText(Target, Info);
	}
}

void ULOACombatRPCComponent::ClientShowTelegraph_Implementation(const FLOATelegraphData& Data)
{
	// 그리는 일은 이 컴퓨터의 예고 매니저 담당. 여기는 네트워크로 받아서 넘기기만 한다.
	if (ULOATelegraphSubsystem* Telegraphs = ULOATelegraphSubsystem::Get(this))
	{
		Telegraphs->ShowTelegraph(Data);
	}
}

void ULOACombatRPCComponent::ClientHideTelegraph_Implementation(int32 TelegraphId)
{
	if (ULOATelegraphSubsystem* Telegraphs = ULOATelegraphSubsystem::Get(this))
	{
		Telegraphs->HideTelegraph(TelegraphId);
	}
}

void ULOACombatRPCComponent::ClientRequestHitJudge_Implementation(const FLOAHitJudgeRequestData& Request)
{
	// 판정은 이 컴퓨터의 피해자 판정 담당. 여기는 네트워크로 받아서 넘기기만 한다.
	if (ULOAVictimJudgeSubsystem* VictimJudge = ULOAVictimJudgeSubsystem::Get(this))
	{
		VictimJudge->JudgeLocal(Request);
	}
}

void ULOACombatRPCComponent::ClientJustGuardWindow_Implementation(int32 WindowId, ELOAJustGuardMode Mode, ELOAJustGuardWindowState State, AActor* Attacker, float Duration)
{
	// 받아서 내 캐릭터의 장부에 넘기기만 한다. (GA가 장부를 보고 발동 여부를 정한다)
	if (ULOAJustGuardComponent* JustGuardComp = FindPawnJustGuardComponent())
	{
		JustGuardComp->SetWindowState(WindowId, Mode, State, Attacker, Duration);
	}
}

void ULOACombatRPCComponent::ClientJustGuardResult_Implementation(int32 WindowId)
{
	// 받아서 내 캐릭터의 장부에 넘기기만 한다. (장부가 GA에 알린다)
	if (ULOAJustGuardComponent* JustGuardComp = FindPawnJustGuardComponent())
	{
		JustGuardComp->NotifyGuardSucceeded(WindowId);
	}
}

// =============================================================================
// 이 플레이어 컴퓨터 → 서버
// =============================================================================

void ULOACombatRPCComponent::ServerReportHitJudgeHit_Implementation(int32 RequestId)
{
	ForwardHitJudgeReport(RequestId, true, FVector::ZeroVector);
}

void ULOACombatRPCComponent::ServerReportHitJudgeMiss_Implementation(int32 RequestId, FVector_NetQuantize ReportedLocation)
{
	ForwardHitJudgeReport(RequestId, false, ReportedLocation);
}

void ULOACombatRPCComponent::ServerReportHitJudgeGuarded_Implementation(int32 RequestId)
{
	// 맞은 사람 = 보낸 플레이어의 캐릭터. 판단은 서버 장부(사본)가 한다 (피해자 판정 → 판정 매니저의 확정)
	const APlayerController* PC = GetOwner<APlayerController>();
	APawn* Victim = PC ? PC->GetPawn() : nullptr;
	ULOAVictimJudgeSubsystem* VictimJudge = ULOAVictimJudgeSubsystem::Get(this);
	if (Victim && VictimJudge)
	{
		VictimJudge->ReceiveGuardReport(Victim, RequestId);
	}
}

void ULOACombatRPCComponent::ServerReportJustGuardPress_Implementation(int32 WindowId, double PressServerTime, uint8 PressYaw)
{
	// 보낸 플레이어의 캐릭터 = 누른 사람. 검사(구간, 시각)와 장부 사본에 적기는 저가 매니저가 한다.
	const APlayerController* PC = GetOwner<APlayerController>();
	ULOAJustGuardSubsystem* JustGuardSubsystem = ULOAJustGuardSubsystem::Get(this);
	if (JustGuardSubsystem && PC)
	{
		JustGuardSubsystem->ReceivePress(PC->GetPawn(), WindowId, PressServerTime, FRotator::DecompressAxisFromByte(PressYaw));
	}
}

void ULOACombatRPCComponent::ServerReportJustGuardRelease_Implementation(int32 WindowId, double ServerTime, bool bCancelled)
{
	const APlayerController* PC = GetOwner<APlayerController>();
	ULOAJustGuardSubsystem* JustGuardSubsystem = ULOAJustGuardSubsystem::Get(this);
	if (JustGuardSubsystem && PC)
	{
		JustGuardSubsystem->ReceiveRelease(PC->GetPawn(), WindowId, ServerTime, bCancelled);
	}
}

ULOAJustGuardComponent* ULOACombatRPCComponent::FindPawnJustGuardComponent() const
{
	const APlayerController* PC = GetOwner<APlayerController>();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<ULOAJustGuardComponent>() : nullptr;
}

void ULOACombatRPCComponent::ForwardHitJudgeReport(int32 RequestId, bool bHit, const FVector& ReportedLocation) const
{
	// 맞은 사람 = 이 RPC를 보낸 플레이어의 캐릭터 (인자로 받지 않는다 → 남을 대신 보고할 수 없다)
	const APlayerController* PC = GetOwner<APlayerController>();
	APawn* Victim = PC ? PC->GetPawn() : nullptr;
	ULOAVictimJudgeSubsystem* VictimJudge = ULOAVictimJudgeSubsystem::Get(this);
	if (!Victim || !VictimJudge)
	{
		return;
	}

	// 받아 줄지(기다리는 중인가, 검증)는 피해자 판정이 정한다.
	VictimJudge->ReceiveReport(Victim, RequestId, bHit, ReportedLocation);
}

// =============================================================================
// [디버그 측정] (2026-10-29) 시계 확인. 원인을 찾으면 지운다
// =============================================================================

void ULOACombatRPCComponent::ServerDebugClockPing_Implementation(double ClientRealTime)
{
#if !UE_BUILD_SHIPPING
	// 받은 프레임의 서버 시각을 붙여 그대로 돌려준다
	ClientDebugClockPong(ClientRealTime, LOANetTime::GetServerTimeNow(GetWorld()));
#endif
}

void ULOACombatRPCComponent::ClientDebugClockPong_Implementation(double ClientRealTime, double ServerTime)
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 왕복 시간 = 내 실제 시계로 잰 값 (서버 시계와 상관없음)
	const double Rtt = FPlatformTime::Seconds() - ClientRealTime;

	// 지금 실제 서버 시각 짐작 = 서버가 답할 때 시각 + 돌아오는 데 걸린 시간(왕복 ÷ 2, 양쪽 지연이 같다고 봄)
	const double ServerNowEstimate = ServerTime + Rtt * 0.5;

	// 내 시계들과 비교. + = 내 시계가 앞섬 / − = 늦음
	const double MyClock = LOANetTime::GetServerTimeNow(World);
	const AGameStateBase* GameState = World->GetGameState();
	const double EngineClock = GameState ? GameState->GetServerWorldTimeSeconds() : 0.0;
	const APlayerController* PC = GetOwner<APlayerController>();
	const APlayerState* PlayerState = PC ? PC->PlayerState : nullptr;

	LOA_LOG(this, Log, TEXT("[ClockCheck] RTT %.3f (PlayerState ping %.0fms) | LOANetTime error %+.3f | engine clock error %+.3f | my one-way %.3f vs RTT/2 %.3f"),
		Rtt, PlayerState ? PlayerState->GetPingInMilliseconds() : -1.f,
		MyClock - ServerNowEstimate, EngineClock - ServerNowEstimate,
		LOANetTime::GetOneWayLatency(World), Rtt * 0.5);
#endif
}
