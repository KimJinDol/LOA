// LOAJustGuardComponent.cpp

#include "Combat/LOAJustGuardComponent.h"
#include "Combat/LOAJustGuardSubsystem.h"
#include "Combat/LOACombatRPCComponent.h"
#include "Library/LOACombatSettings.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAJustGuardComponent)

namespace LOAJustGuard
{
	/** 기록을 몇 개 남길까 (최근 것부터) */
	constexpr int32 MaxRecords = 8;

	/** 맞은 시각 두 개를 같은 타격으로 볼 차이 (초) */
	constexpr double SameHitTolerance = 0.001;
}

ULOAJustGuardComponent::ULOAJustGuardComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 기록·판정은 불릴 때만
	PrimaryComponentTick.bCanEverTick = false;

	// 복제하지 않는다. 값은 ULOACombatRPCComponent의 RPC로만 오간다
	SetIsReplicatedByDefault(false);
}

// =============================================================================
// 조종하는 사람 컴퓨터
// =============================================================================

void ULOAJustGuardComponent::SetWindowState(int32 WindowId, ELOAJustGuardMode Mode, ELOAJustGuardWindowState State, AActor* Attacker, float Duration)
{
	if (WindowId == 0)
	{
		return;
	}
	FLOAJustGuardRecord& Record = FindOrAddRecord(WindowId, Mode);
	Record.bOpen = (State == ELOAJustGuardWindowState::Open);
	if (Attacker)
	{
		Record.Attacker = Attacker;
	}

	// 홀딩 게이지: 열림을 받은 순간과 구간 길이 / 구간 끝 (2026-11-05)
	if (State == ELOAJustGuardWindowState::Open)
	{
		const UWorld* World = GetWorld();
		if (Record.OpenLocalTime < 0.0 && World)
		{
			Record.OpenLocalTime = World->GetTimeSeconds();
		}
		Record.Duration = FMath::Max(0.f, Duration);
	}
	else if (State == ELOAJustGuardWindowState::Closed)
	{
		Record.bClosed = true;
	}

	const TCHAR* StateName = (State == ELOAJustGuardWindowState::Open) ? TEXT("opened")
		: (State == ELOAJustGuardWindowState::PressClosed) ? TEXT("press closed") : TEXT("closed");
	LOA_LOG(GetOwner(), Log, TEXT("[JustGuard] window #%d %s (%s)"), WindowId, StateName, *GetNameSafe(Attacker));

	// 열림 → 듣는 쪽에 알림 (플레이어 테스터의 자동 저가) / 구간 끝 → GA에 알림 (홀딩 중이면 저가 자세를 푼다)
	if (State == ELOAJustGuardWindowState::Open)
	{
		OnWindowOpened.Broadcast(WindowId);
	}
	else if (State == ELOAJustGuardWindowState::Closed)
	{
		OnWindowClosed.Broadcast(WindowId);
	}
}

bool ULOAJustGuardComponent::GetHoldProgress(float& OutProgress) const
{
	OutProgress = 0.f;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 가장 최근 기록부터: 홀딩 구간 + 누르고 있음(뗌·취소 없음) + 구간이 아직 안 끝남 + 첫 타가 닿음
	const double Now = World->GetTimeSeconds();
	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		const FLOAJustGuardRecord& Record = Records[Index];
		const bool bHoldMode = (Record.Mode == ELOAJustGuardMode::Hold || Record.Mode == ELOAJustGuardMode::HoldFromMiddle);
		const bool bHolding = Record.HasPressed() && Record.ReleaseTime < 0.0 && !Record.bClosed;
		if (!bHoldMode || !bHolding || Record.Duration <= 0.f || Record.OpenLocalTime < 0.0 || Record.FirstHitLocalTime < 0.0)
		{
			continue;	// 첫 타 전이면 게이지를 띄우지 않는다 (사용자 결정 2026-11-08)
		}

		// (지금 − 첫 타) ÷ (구간 끝 − 첫 타). 첫 타가 구간 끝 이후면 보여 줄 구간이 없다
		const double WindowEndTime = Record.OpenLocalTime + Record.Duration;
		const double Span = WindowEndTime - Record.FirstHitLocalTime;
		if (Span <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		OutProgress = FMath::Clamp(static_cast<float>((Now - Record.FirstHitLocalTime) / Span), 0.f, 1.f);
		return true;
	}
	return false;
}

int32 ULOAJustGuardComponent::FindPressableWindow() const
{
	// ID가 작은 것부터 = 먼저 열린 것부터 (단타 연타 패턴: #7을 누른 뒤에야 #8)
	for (const FLOAJustGuardRecord& Record : Records)
	{
		if (Record.bOpen && !Record.HasPressed())
		{
			return Record.WindowId;
		}
	}
	return 0;
}

// =============================================================================
// 기록
// =============================================================================

void ULOAJustGuardComponent::RecordPress(int32 WindowId, ELOAJustGuardMode Mode, double PressServerTime, float PressYaw)
{
	FLOAJustGuardRecord& Record = FindOrAddRecord(WindowId, Mode);
	if (Record.HasPressed())
	{
		return;	// 한 구간에 한 번
	}
	Record.PressTime = PressServerTime;
	Record.PressYaw = PressYaw;
	LOA_LOG(GetOwner(), Log, TEXT("[JustGuard] window #%d press at %.2f (yaw %.0f)"), WindowId, PressServerTime, PressYaw);
}

void ULOAJustGuardComponent::RecordRelease(int32 WindowId, double ReleaseServerTime)
{
	FLOAJustGuardRecord* Record = FindRecordMutable(WindowId);
	if (!Record || !Record->HasPressed() || Record->ReleaseTime >= 0.0)
	{
		return;
	}
	Record->ReleaseTime = FMath::Max(ReleaseServerTime, Record->PressTime);
	LOA_LOG(GetOwner(), Log, TEXT("[JustGuard] window #%d release at %.2f"), WindowId, Record->ReleaseTime);
}

void ULOAJustGuardComponent::RecordCancel(int32 WindowId, double CancelServerTime)
{
	FLOAJustGuardRecord* Record = FindRecordMutable(WindowId);
	if (!Record || !Record->HasPressed() || Record->CancelTime >= 0.0)
	{
		return;
	}
	Record->CancelTime = FMath::Max(CancelServerTime, Record->PressTime);
	if (Record->ReleaseTime < 0.0)
	{
		Record->ReleaseTime = Record->CancelTime;	// 취소 = 그 순간 손을 뗀 것 (홀딩도 거기서 끝)
	}
	LOA_LOG(GetOwner(), Log, TEXT("[JustGuard] window #%d cancelled at %.2f"), WindowId, Record->CancelTime);
}

const FLOAJustGuardRecord* ULOAJustGuardComponent::FindRecord(int32 WindowId) const
{
	return Records.FindByPredicate([WindowId](const FLOAJustGuardRecord& Record) { return Record.WindowId == WindowId; });
}

// =============================================================================
// 막았나
// =============================================================================

bool ULOAJustGuardComponent::JudgeJustGuard(const FLOAIncomingHit& Hit)
{
	// ① 저가 공격인가
	if (!Hit.AttackTags.HasTag(LOAGameplayTags::Skill_Attribute_JustGuard))
	{
		return false;
	}

	// ② 그 보스의 구간 (없으면 막을 수 없는 타격)
	FLOAJustGuardRecord* Record = FindRecordForHit(Hit);
	if (!Record)
	{
		return false;
	}

	// ③ 맞은 시각 적기 (안 눌렀어도. 늦게 누른 사람이 다음 타격부터 막지 못하게 홀딩의 첫 타를 남긴다)
	AddHitTime(*Record, Hit.SeenTime);

	// 홀딩 게이지의 0%: 이 컴퓨터에서 처음 닿은 순간 (로컬 시계. 2026-11-08). 서버 사본에서도 적히지만 쓰지 않는다
	if (Record->FirstHitLocalTime < 0.0)
	{
		if (const UWorld* World = GetWorld())
		{
			Record->FirstHitLocalTime = World->GetTimeSeconds();
		}
	}

	// ④ 클라가 이미 "맞음"이라 함 → 판단하지 않는다 (거짓이어도 본인만 손해)
	if (Hit.bClientSaidHit)
	{
		return false;
	}

	// ⑤ 막았나
	const int32 WindowId = Record->WindowId;
	if (!IsBlocked(*Record, Hit.SeenTime, Hit.AttackOrigin, Hit.MyLocation))
	{
		return false;
	}

	// ⑥ 막음 알리기: 이 컴퓨터의 GA → 성공 모션 / 서버 사본이면 그 클라에도
	LOA_LOG(GetOwner(), Log, TEXT("[JustGuard] window #%d guarded (seen %.2f, %s)"),
		WindowId, Hit.SeenTime, GetOwner() && GetOwner()->HasAuthority() ? TEXT("server record") : TEXT("my judge, server confirms"));
	NotifyGuardSucceeded(WindowId);
	NotifyClientGuardSucceeded(WindowId);
	return true;
}

FLOAJustGuardRecord* ULOAJustGuardComponent::FindRecordForHit(const FLOAIncomingHit& Hit)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Hit.Attacker)
	{
		return nullptr;
	}

	// 서버: 저가 매니저가 가진 구간 기록 (열린·닫힌 시각)에서 그 보스의 구간 중 판정 시각을 포함하는 것
	if (Owner->HasAuthority())
	{
		const ULOAJustGuardSubsystem* JustGuardSubsystem = ULOAJustGuardSubsystem::Get(this);
		const FLOAJustGuardWindow* Window = JustGuardSubsystem ? JustGuardSubsystem->FindWindowAt(Hit.Attacker.Get(), Hit.HitServerTime) : nullptr;
		if (!Window)
		{
			return nullptr;
		}
		FLOAJustGuardRecord& Record = FindOrAddRecord(Window->Id, Window->Mode);
		Record.Attacker = Hit.Attacker.Get();
		return &Record;
	}

	// 클라: 받은 구간 알림 중 그 보스의 가장 최근 구간
	//   요청·예고와 구간 알림은 같은 통로(PC의 ULOACombatRPCComponent)로 순서대로 온다 → 그 타격 뒤에 열린 구간은 아직 안 왔다
	return FindLatestRecordOf(Hit.Attacker.Get());
}

void ULOAJustGuardComponent::AddHitTime(FLOAJustGuardRecord& Record, double SeenTime)
{
	// 같은 타격은 한 번 (클라가 먼저 적고 서버 알림이 와도 기록은 컴퓨터마다 따로라 보통 겹치지 않는다. 안전하게)
	for (const double Existing : Record.HitTimes)
	{
		if (FMath::IsNearlyEqual(Existing, SeenTime, LOAJustGuard::SameHitTolerance))
		{
			return;
		}
	}

	// 작은 것부터 (피해자 판정 보고는 늦게 와서 순서가 바뀔 수 있다)
	const int32 InsertIndex = Record.HitTimes.IndexOfByPredicate([SeenTime](double Existing) { return Existing > SeenTime; });
	if (InsertIndex == INDEX_NONE)
	{
		Record.HitTimes.Add(SeenTime);
	}
	else
	{
		Record.HitTimes.Insert(SeenTime, InsertIndex);
	}
}

double ULOAJustGuardComponent::GetReferenceTime(const FLOAJustGuardRecord& Record, double SeenTime)
{
	switch (Record.Mode)
	{
	case ELOAJustGuardMode::Hold:
		// 이 사람에게 처음 닿은 타격
		return Record.HitTimes.IsEmpty() ? SeenTime : Record.HitTimes[0];

	case ELOAJustGuardMode::HoldFromMiddle:
	{
		// 누른 뒤 처음 닿은 타격 (그 전 타격은 기준이 못 된다)
		const double* FirstAfterPress = Record.HitTimes.FindByPredicate([&Record](double Time) { return Time >= Record.PressTime; });
		return FirstAfterPress ? *FirstAfterPress : SeenTime;
	}

	default:
		// 단타: 이 타격
		return SeenTime;
	}
}

bool ULOAJustGuardComponent::IsBlocked(const FLOAJustGuardRecord& Record, double SeenTime, const FVector& AttackOrigin, const FVector& VictimLocation) const
{
	if (Record.Mode == ELOAJustGuardMode::None || !Record.HasPressed())
	{
		return false;
	}

	// 기준 타격 (헤더 [판정 규칙]). 기준보다 앞에 맞은 타격은 못 막음 (홀딩 중간부터: 누르기 전에 맞은 타격)
	const double Reference = GetReferenceTime(Record, SeenTime);
	if (SeenTime < Reference)
	{
		return false;
	}

	// 기준 − 0.5 ≤ 누른 시각 ≤ 기준
	const double Window = GetDefault<ULOACombatSettings>()->JustGuardWindow;
	const bool bPressedInTime = Record.PressTime <= Reference && Record.PressTime >= Reference - Window;
	if (!bPressedInTime)
	{
		return false;
	}

	// 단타: 공격 전에 취소(피격 이상으로 끊김)됐으면 못 막음 / 홀딩: 이 타격 때 아직 누르고 있어야 (취소도 뗀 것)
	const bool bStillGuarding = (Record.Mode == ELOAJustGuardMode::Tap)
		? !Record.WasCancelledBefore(SeenTime)
		: Record.IsHeldAt(SeenTime);
	if (!bStillGuarding)
	{
		return false;
	}

	// 방향: 공격 기준점이 정면 ±각도 안
	float Angle = 0.f;
	if (!IsFacing(Record, AttackOrigin, VictimLocation, Angle))
	{
		LOA_LOG(GetOwner(), Log, TEXT("[JustGuard] window #%d not facing the attack (%.0f deg off, max %.0f)"),
			Record.WindowId, Angle, GetDefault<ULOACombatSettings>()->JustGuardFacingHalfAngle);
		return false;
	}
	return true;
}

bool ULOAJustGuardComponent::IsFacing(const FLOAJustGuardRecord& Record, const FVector& AttackOrigin, const FVector& VictimLocation, float& OutAngle) const
{
	OutAngle = 0.f;
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// 기준점이 내 몸 안이면 방향이 없다 → 통과 (예: 내 발밑 원형 장판)
	if (FVector::DistSquared2D(AttackOrigin, VictimLocation) <= FMath::Square(Owner->GetSimpleCollisionRadius()))
	{
		return true;
	}

	// 정면 = 누른 순간 방향 (저가 중엔 못 돈다)
	const FVector Forward = FRotator(0.f, Record.PressYaw, 0.f).Vector();
	OutAngle = ULOACombatLibrary::GetFacingAngleTo(VictimLocation, Forward, AttackOrigin);
	return OutAngle <= GetDefault<ULOACombatSettings>()->JustGuardFacingHalfAngle;
}

// =============================================================================
// 결과
// =============================================================================

void ULOAJustGuardComponent::NotifyGuardSucceeded(int32 WindowId)
{
	OnGuardSucceeded.Broadcast(WindowId);
}

void ULOAJustGuardComponent::NotifyClientGuardSucceeded(int32 WindowId) const
{
	// 서버 사본 + 원격 플레이어만 (리슨 호스트 본인은 위의 알림이 곧 자기 GA, 클라는 자기 원본)
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->HasAuthority() || OwnerPawn->IsLocallyControlled())
	{
		return;
	}
	const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr)
	{
		CombatRPC->ClientJustGuardResult(WindowId);
	}
}

FLOAJustGuardRecord* ULOAJustGuardComponent::FindLatestRecordOf(const AActor* Attacker)
{
	// 기록은 ID 순 (오래된 것이 앞) → 뒤에서부터
	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		if (Records[Index].Attacker.Get() == Attacker)
		{
			return &Records[Index];
		}
	}
	return nullptr;
}

// =============================================================================
// 내부
// =============================================================================

FLOAJustGuardRecord* ULOAJustGuardComponent::FindRecordMutable(int32 WindowId)
{
	return Records.FindByPredicate([WindowId](const FLOAJustGuardRecord& Record) { return Record.WindowId == WindowId; });
}

FLOAJustGuardRecord& ULOAJustGuardComponent::FindOrAddRecord(int32 WindowId, ELOAJustGuardMode Mode)
{
	if (FLOAJustGuardRecord* Existing = FindRecordMutable(WindowId))
	{
		return *Existing;
	}

	// 오래된 기록 먼저 버린다 (ID가 작은 것 = 오래된 것이 앞)
	if (Records.Num() >= LOAJustGuard::MaxRecords)
	{
		Records.RemoveAt(0, Records.Num() - LOAJustGuard::MaxRecords + 1);
	}

	// ID 순서대로 넣는다 (FindPressableWindow가 앞에서부터 본다)
	FLOAJustGuardRecord NewRecord;
	NewRecord.WindowId = WindowId;
	NewRecord.Mode = Mode;
	const int32 InsertIndex = Records.IndexOfByPredicate([WindowId](const FLOAJustGuardRecord& Record) { return Record.WindowId > WindowId; });
	if (InsertIndex == INDEX_NONE)
	{
		return Records.Add_GetRef(NewRecord);
	}
	return Records.Insert_GetRef(NewRecord, InsertIndex);
}
