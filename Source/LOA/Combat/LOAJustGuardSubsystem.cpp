// LOAJustGuardSubsystem.cpp

#include "Combat/LOAJustGuardSubsystem.h"
#include "Combat/LOAJustGuardComponent.h"
#include "Combat/LOACombatRPCComponent.h"
#include "Combat/LOACombatJudgeManagerSubsystem.h"
#include "Core/LOANetTime.h"
#include "Library/LOACombatSettings.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAJustGuardSubsystem)

namespace LOAJustGuard
{
	/**
	 * 원격 누름의 누른 시각이 도착보다 이만큼(초) 넘게 옛날이면 이만큼으로 당긴다.
	 * 정상 차이 = 편도 지연 + 클라 시계 오차. 핑 400ms → 편도 0.2 + 오차 0.1 = 0.3
	 * 닫힌 구간의 누름도 닫힌 뒤 이 안에 오면 받는다 (클라는 닫힘을 편도만큼 늦게 본다)
	 */
	constexpr double MaxPressBackdate = 0.3;

	/** 닫힌 구간을 몇 초 남길까. 피해자 판정 마감(1초)보다 넉넉하게 (서버 저가 컴포넌트가 늦은 보고 때도 구간을 찾을 수 있게) */
	constexpr double ClosedWindowKeep = 3.0;

	/** 로그용 방식 이름 */
	const TCHAR* GetModeName(ELOAJustGuardMode Mode)
	{
		switch (Mode)
		{
		case ELOAJustGuardMode::Hold:			return TEXT("hold");
		case ELOAJustGuardMode::HoldFromMiddle:	return TEXT("hold from middle");
		default:								return TEXT("tap");
		}
	}
}

ULOAJustGuardSubsystem* ULOAJustGuardSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOAJustGuardSubsystem>() : nullptr;
}

ELOAJustGuardGrade ULOAJustGuardSubsystem::GetCoopJustGuardGrade(int32 SuccessCount)
{
	const ULOACombatSettings* Settings = GetDefault<ULOACombatSettings>();
	if (SuccessCount >= Settings->CoopJustGuardPerfectCount)
	{
		return ELOAJustGuardGrade::Perfect;
	}
	if (SuccessCount >= Settings->CoopJustGuardGreatCount)
	{
		return ELOAJustGuardGrade::Great;
	}
	return ELOAJustGuardGrade::Bad;
}

// =============================================================================
// 저가 구간
// =============================================================================

int32 ULOAJustGuardSubsystem::OpenWindow(AActor* Caster, ELOAJustGuardMode Mode, float Duration)
{
	const UWorld* World = GetWorld();
	if (!Caster || !Caster->HasAuthority() || !World)
	{
		return 0;
	}
	const double Now = LOANetTime::GetServerTimeNow(World);

	// 오래 전에 닫힌 구간 버리기
	Windows.RemoveAll([Now](const FLOAJustGuardWindow& Window)
	{
		return !Window.IsOpen() && Window.CloseTime < Now - LOAJustGuard::ClosedWindowKeep;
	});

	LastWindowId = (LastWindowId == MAX_int32) ? 1 : LastWindowId + 1;

	FLOAJustGuardWindow& Window = Windows.AddDefaulted_GetRef();
	Window.Id = LastWindowId;
	Window.Caster = Caster;
	Window.Mode = (Mode == ELOAJustGuardMode::None) ? ELOAJustGuardMode::Tap : Mode;
	Window.OpenTime = Now;
	Window.Duration = FMath::Max(0.f, Duration);

	LOA_LOG(Caster, Log, TEXT("[JustGuard] window #%d opened (%s) at %.2f, %.2fs long"),
		Window.Id, LOAJustGuard::GetModeName(Window.Mode), Now, Window.Duration);

	SendWindowToAll(Window, ELOAJustGuardWindowState::Open);
	return Window.Id;
}

void ULOAJustGuardSubsystem::CloseWindow(int32 WindowId)
{
	FLOAJustGuardWindow* Window = FindWindowMutable(WindowId);
	if (!Window || !Window->IsOpen())
	{
		return;
	}
	Window->CloseTime = LOANetTime::GetServerTimeNow(GetWorld());

	LOA_LOG(Window->Caster.Get(), Log, TEXT("[JustGuard] window #%d closed at %.2f"), WindowId, Window->CloseTime);

	// 구간 끝 → 클라에 "닫힘". 첫 타격 때 누르기 마감을 보냈어도 다시 보낸다 (홀딩 중인 플레이어의 저가 GA가 이걸 보고 끝난다. 2026-11-04)
	SendWindowToAll(*Window, ELOAJustGuardWindowState::Closed);
}

const FLOAJustGuardWindow* ULOAJustGuardSubsystem::FindWindowAt(const AActor* Caster, double ServerTime) const
{
	// 가장 최근에 열린 것부터. 그 시각에 열려 있었나 = 열린 시각 ≤ 그 시각 ≤ 닫힌 시각 (아직 열려 있으면 닫힌 시각 없음)
	for (int32 Index = Windows.Num() - 1; Index >= 0; --Index)
	{
		const FLOAJustGuardWindow& Window = Windows[Index];
		if (Window.Caster.Get() == Caster && Window.OpenTime <= ServerTime && (Window.IsOpen() || Window.CloseTime >= ServerTime))
		{
			return &Window;
		}
	}
	return nullptr;
}

const FLOAJustGuardWindow* ULOAJustGuardSubsystem::FindWindow(int32 WindowId) const
{
	return Windows.FindByPredicate([WindowId](const FLOAJustGuardWindow& Window) { return Window.Id == WindowId; });
}

FLOAJustGuardWindow* ULOAJustGuardSubsystem::FindWindowMutable(int32 WindowId)
{
	return Windows.FindByPredicate([WindowId](const FLOAJustGuardWindow& Window) { return Window.Id == WindowId; });
}

void ULOAJustGuardSubsystem::ClosePress(FLOAJustGuardWindow& Window)
{
	if (Window.IsPressClosed())
	{
		return;	// 홀딩 2타부터
	}
	Window.PressCloseTime = LOANetTime::GetServerTimeNow(GetWorld());

	LOA_LOG(Window.Caster.Get(), Log, TEXT("[JustGuard] window #%d press closed at %.2f (first hit)"), Window.Id, Window.PressCloseTime);
	SendWindowToAll(Window, ELOAJustGuardWindowState::PressClosed);
}

void ULOAJustGuardSubsystem::SendWindowToAll(const FLOAJustGuardWindow& Window, ELOAJustGuardWindowState State) const
{
	// 리슨 호스트 본인 PC는 Client RPC가 그 자리에서 실행된다
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr)
		{
			CombatRPC->ClientJustGuardWindow(Window.Id, Window.Mode, State, Window.Caster.Get(), Window.Duration);
		}
	}
}

// =============================================================================
// 원격 누름
// =============================================================================

void ULOAJustGuardSubsystem::ReceivePress(APawn* Player, int32 WindowId, double ReportedPressTime, float PressYaw)
{
	ULOAJustGuardComponent* JustGuardComp = Player ? Player->FindComponentByClass<ULOAJustGuardComponent>() : nullptr;
	const FLOAJustGuardWindow* Window = FindWindow(WindowId);
	if (!JustGuardComp || !Window)
	{
		LOA_LOG(Player, Warning, TEXT("[JustGuard] press for unknown window #%d ignored"), WindowId);
		return;
	}

	// 닫힌 지 오래된 구간 = 너무 늦은 보고
	const double Now = LOANetTime::GetServerTimeNow(GetWorld());
	if (!Window->IsOpen() && Now - Window->CloseTime > LOAJustGuard::MaxPressBackdate)
	{
		LOA_LOG(Player, Warning, TEXT("[JustGuard] press for window #%d ignored (closed %.2fs ago)"), WindowId, Now - Window->CloseTime);
		return;
	}

	// 시각: 미래 → 지금 / 열리기 전이나 0.3초 넘게 옛날 → 그쪽으로 당김
	const double Earliest = FMath::Min(FMath::Max(Window->OpenTime, Now - LOAJustGuard::MaxPressBackdate), Now);
	const double PressTime = FMath::Clamp(ReportedPressTime, Earliest, Now);
	if (!FMath::IsNearlyEqual(PressTime, ReportedPressTime, 0.001))
	{
		LOA_LOG(Player, Warning, TEXT("[JustGuard] window #%d press time %.2f clamped to %.2f (now %.2f)"),
			WindowId, ReportedPressTime, PressTime, Now);
	}

	// 이미 누른 구간이면 장부가 무시한다 (한 구간에 한 번)
	// 방향은 검사하지 않는다: 어느 쪽을 보고 누를지는 플레이어가 커서로 정하는 값 (속여도 얻는 게 없다)
	JustGuardComp->RecordPress(WindowId, Window->Mode, PressTime, PressYaw);
}

void ULOAJustGuardSubsystem::ReceiveRelease(APawn* Player, int32 WindowId, double ReportedTime, bool bCancelled)
{
	ULOAJustGuardComponent* JustGuardComp = Player ? Player->FindComponentByClass<ULOAJustGuardComponent>() : nullptr;
	const FLOAJustGuardRecord* Record = JustGuardComp ? JustGuardComp->FindRecord(WindowId) : nullptr;
	if (!Record || !Record->HasPressed())
	{
		return;
	}

	// 누른 시각 ~ 지금 사이로 (미래로 미뤄 "더 오래 누른 척" 못 하게)
	const double Time = FMath::Clamp(ReportedTime, Record->PressTime, LOANetTime::GetServerTimeNow(GetWorld()));
	if (bCancelled)
	{
		JustGuardComp->RecordCancel(WindowId, Time);
	}
	else
	{
		JustGuardComp->RecordRelease(WindowId, Time);
	}
}

// =============================================================================
// 판정 매니저 알림 듣기 (헤더 [집계])
// =============================================================================

void ULOAJustGuardSubsystem::OnJudgeStarted(const FLOAAttackInfo& Attack)
{
	if (!Attack.Context.AttackTags.HasTag(LOAGameplayTags::Skill_Attribute_JustGuard))
	{
		return;	// 저가 속성이 없는 공격
	}

	AActor* Caster = Attack.Context.Caster;
	const FLOAJustGuardWindow* Window = FindWindowAt(Caster, Attack.HitServerTime);
	if (!Window)
	{
		LOA_LOG(Caster, Warning, TEXT("[JustGuard] Skill.Attribute.JustGuard attack but no open window on %s. Not guardable (paint LOA Just Guard Window over the hit)."),
			*GetNameSafe(Caster));
		return;
	}

	// 이 타격의 집계 (공격 번호로)
	FLOAJustGuardResolved& Tally = Tallies.Add(Attack.AttackId);
	Tally.Caster = Caster;
	Tally.WindowId = Window->Id;
	Tally.AttackTags = Attack.Context.AttackTags;

	// 이 구간의 첫 타격이면 누르기 마감 (그 뒤에 누르면 어차피 못 막는다 → 헛모션 방지). 홀딩 중간부터는 마감하지 않는다
	if (Window->Mode != ELOAJustGuardMode::HoldFromMiddle)
	{
		if (FLOAJustGuardWindow* MutableWindow = FindWindowMutable(Window->Id))
		{
			ClosePress(*MutableWindow);
		}
	}
}

void ULOAJustGuardSubsystem::OnHitOutcome(int32 AttackId, AActor* Target, ELOAHitOutcome Outcome)
{
	FLOAJustGuardResolved* Tally = Tallies.Find(AttackId);
	if (!Tally)
	{
		return;	// 저가 공격이 아님
	}

	// 플레이어만 센다 (저가 컴포넌트가 있는 캐릭터. 몬스터·허수아비는 빼고)
	APawn* Player = Cast<APawn>(Target);
	if (!Player || !Player->FindComponentByClass<ULOAJustGuardComponent>())
	{
		return;
	}

	if (Outcome == ELOAHitOutcome::Blocked)
	{
		Tally->Guarded.Add(Player);
	}
	else if (Outcome == ELOAHitOutcome::Hit)
	{
		Tally->Hit.Add(Player);
	}
}

void ULOAJustGuardSubsystem::OnJudgeFinished(int32 AttackId)
{
	// 목록에서 먼저 꺼내고 알린다 (듣는 쪽이 새 공격을 일으켜 목록이 바뀌어도 안전하게)
	FLOAJustGuardResolved Result;
	if (!Tallies.RemoveAndCopyValue(AttackId, Result))
	{
		return;
	}

	LOA_LOG(Result.Caster.Get(), Log, TEXT("[JustGuard] window #%d hit resolved (attack #%d): %d guarded, %d hit"),
		Result.WindowId, AttackId, Result.Guarded.Num(), Result.Hit.Num());
	OnJustGuardResolved.Broadcast(Result);
}

// =============================================================================
// 월드
// =============================================================================

bool ULOAJustGuardSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOAJustGuardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 판정 매니저 알림 듣기 (판정 매니저는 누가 듣는지 모른다). 판정 매니저가 먼저 만들어지게 한다
	if (ULOACombatJudgeManagerSubsystem* JudgeManager = Collection.InitializeDependency<ULOACombatJudgeManagerSubsystem>())
	{
		JudgeStartedHandle = JudgeManager->OnJudgeStarted.AddUObject(this, &ThisClass::OnJudgeStarted);
		HitOutcomeHandle = JudgeManager->OnHitOutcome.AddUObject(this, &ThisClass::OnHitOutcome);
		JudgeFinishedHandle = JudgeManager->OnJudgeFinished.AddUObject(this, &ThisClass::OnJudgeFinished);
	}
}

void ULOAJustGuardSubsystem::Deinitialize()
{
	if (ULOACombatJudgeManagerSubsystem* JudgeManager = ULOACombatJudgeManagerSubsystem::Get(this))
	{
		JudgeManager->OnJudgeStarted.Remove(JudgeStartedHandle);
		JudgeManager->OnHitOutcome.Remove(HitOutcomeHandle);
		JudgeManager->OnJudgeFinished.Remove(JudgeFinishedHandle);
	}
	JudgeStartedHandle.Reset();
	HitOutcomeHandle.Reset();
	JudgeFinishedHandle.Reset();

	Windows.Reset();
	Tallies.Reset();
	Super::Deinitialize();
}
