// LOACombatJudgeManagerSubsystem.cpp

#include "Combat/LOACombatJudgeManagerSubsystem.h"
#include "Combat/LOAServerJudgeSubsystem.h"
#include "Combat/LOAVictimJudgeSubsystem.h"
#include "Combat/LOAAttackJudgeSubsystem.h"
#include "Combat/LOAJustGuardComponent.h"
#include "Combat/LOACrowdControlPredictionComponent.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting_Shape.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"
#include "Core/LOANetTime.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACombatJudgeManagerSubsystem)

ULOACombatJudgeManagerSubsystem* ULOACombatJudgeManagerSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOACombatJudgeManagerSubsystem>() : nullptr;
}

// =============================================================================
// 스킬 타격 (Apply 모듈)
// =============================================================================

ULOACombatJudgeManagerSubsystem::ERole ULOACombatJudgeManagerSubsystem::GetRole(const ULOAGameplayAbility* Ability, const ULOASkillTargeting* Targeting)
{
	if (!Ability)
	{
		return ERole::None;
	}

	const bool bServer = Ability->HasSkillModuleAuthority();

	// 공격자 판정 공격이 아니면 서버만 판정 (내 컴퓨터는 신호를 받아도 아무것도 안 함. 나중에 Cue 모듈이 이펙트만 예측 재생)
	if (!ULOAAttackJudgeSubsystem::IsAttackerJudged(Ability, Targeting))
	{
		return bServer ? ERole::ServerJudge : ERole::None;
	}

	// 리슨 호스트 본인 = 서버 화면이 곧 내 화면 → 서버 판정 / 원격 플레이어 = 내 컴퓨터가 보고, 서버는 기다림
	const bool bLocallyControlled = Ability->IsLocallyControlled();
	if (bServer)
	{
		return bLocallyControlled ? ERole::ServerJudge : ERole::WaitReport;
	}
	return bLocallyControlled ? ERole::SendReport : ERole::None;
}

void ULOACombatJudgeManagerSubsystem::BeginAttackReports(ULOAGameplayAbility* Ability, const ULOASkillModule* Module,
	const ULOASkillTargeting* Targeting, const TArray<FLOAEffectPayload>& Payloads, int32 NumAllowedReports)
{
	if (GetRole(Ability, Targeting) != ERole::WaitReport)
	{
		return;
	}
	if (ULOAAttackJudgeSubsystem* AttackJudge = ULOAAttackJudgeSubsystem::Get(this))
	{
		AttackJudge->BeginAttackReports(Ability, Module, Targeting, Payloads, NumAllowedReports);
	}
}

void ULOACombatJudgeManagerSubsystem::JudgeInstantHit(ULOAGameplayAbility* Ability, const ULOASkillModule* Module,
	const ULOASkillTargeting* Targeting, const TArray<FLOAEffectPayload>& Payloads)
{
	if (!Ability || !Targeting || Payloads.IsEmpty())
	{
		return;
	}

	switch (GetRole(Ability, Targeting))
	{
	case ERole::ServerJudge:
	{
		// 예고가 없는 즉발 → 플레이어는 시전자의 모션을 보고 피한다 (Animation)
		const FLOASkillModuleContext Context = Ability->MakeSkillModuleContext();
		if (!Context.IsValid())
		{
			return;
		}
		const int32 NumNow = JudgeAndApply(Targeting, Context, Payloads, ELOAHitJudgeTiming::Animation);
		LOA_LOG(Context.SourceActor.Get(), Log, TEXT("[Apply] %s : %d target(s) now"), *GetNameSafe(Ability->GetClass()), NumNow);
		break;
	}
	case ERole::SendReport:
		if (ULOAAttackJudgeSubsystem* AttackJudge = ULOAAttackJudgeSubsystem::Get(this))
		{
			AttackJudge->SendReport(Ability, Module, Targeting);
		}
		break;
	case ERole::WaitReport:
		// 서버 자기 노티파이로는 판정하지 않는다. 보고가 오면 공격자 판정이 검사해서 올린다 (OnAttackReportJudged)
		break;
	default:
		break;
	}
}

void ULOACombatJudgeManagerSubsystem::EndAttackReports(const ULOAGameplayAbility* Ability, const ULOASkillModule* Module)
{
	// 듣고 있지 않으면 공격자 판정이 아무것도 안 한다
	if (ULOAAttackJudgeSubsystem* AttackJudge = ULOAAttackJudgeSubsystem::Get(this))
	{
		AttackJudge->EndAttackReports(Ability, Module);
	}
}

// =============================================================================
// 서버 판정으로 시작하는 공격 (장판 액터, 스킬 타격의 ServerJudge)
// =============================================================================

bool ULOACombatJudgeManagerSubsystem::IsVictimJudged(const ULOASkillTargeting* Targeting, ELOAHitJudgeTiming Timing)
{
	// 피해자 판정 = Shape + Hit Judge = Victim + 보스 모션을 보고 피하는 공격
	//   예고가 꽉 차는 장판은 Hit Judge 칸과 상관없이 서버 판정 (헤더 [누가 판정하나])
	const ULOASkillTargeting_Shape* ShapeTargeting = Cast<ULOASkillTargeting_Shape>(Targeting);
	return ShapeTargeting && ShapeTargeting->GetHitJudge() == ELOAHitJudgeMode::Victim && Timing != ELOAHitJudgeTiming::Telegraph;
}

int32 ULOACombatJudgeManagerSubsystem::JudgeAndApply(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
	const TArray<FLOAEffectPayload>& Payloads, ELOAHitJudgeTiming Timing)
{
	// 결과가 남는 일은 서버만. 시전자가 나가서 ASC가 사라졌으면 줄 수 없다. (IsValid: "파괴 예정"도 거름)
	AActor* Source = Context.SourceActor;
	ULOAServerJudgeSubsystem* ServerJudge = ULOAServerJudgeSubsystem::Get(this);
	if (!Targeting || !Source || !Source->HasAuthority() || !IsValid(Context.SourceASC) || Payloads.IsEmpty() || !ServerJudge)
	{
		return 0;
	}

	// ① 서버가 아는 위치로 대상 찾기
	TArray<FLOATargetHit> Hits;
	Targeting->FindTargets(Context, Hits);

	// ② 이번 공격의 공통 정보 (Context와 Payloads는 복사해 들고 있는다. 부른 GA·장판 액터가 먼저 사라져도 된다)
	FLOAAttackInfo Attack;
	Attack.AttackId = NextAttackId();
	Attack.Context = Context;
	Attack.Payloads = Payloads;
	Attack.Timing = Timing;
	Attack.AttackOrigin = Targeting->GetAttackOrigin(Source);
	Attack.HitServerTime = LOANetTime::GetServerTimeNow(GetWorld());

	// ③ 피해자 판정이면 원격 플레이어를 Hits에서 빼서 적어 둔다 (요청은 "판정 시작" 뒤에 보낸다)
	ULOAVictimJudgeSubsystem* VictimJudge = ULOAVictimJudgeSubsystem::Get(this);
	const int32 NumVictims = (VictimJudge && IsVictimJudged(Targeting, Timing))
		? VictimJudge->HoldRemoteVictims(Targeting, Attack, Hits)
		: 0;

	// ④ 나머지는 서버 판정 (몬스터, 허수아비, 리슨 호스트 본인, 서버 판정 공격의 전원)
	TArray<FLOAJudgeResult> Results;
	ServerJudge->Judge(Attack, Hits, Results);

	// ⑤ 판정 시작 (인원 = 서버 판정 + 피해자 판정) → 서버 판정 결과 확정 → 피해자 판정 요청
	BeginJudge(Attack, Results.Num() + NumVictims);

	int32 NumHitNow = 0;
	for (const FLOAJudgeResult& Result : Results)
	{
		NumHitNow += (FinishTarget(Attack, Result) == ELOAHitOutcome::Hit) ? 1 : 0;
	}

	if (NumVictims > 0)
	{
		VictimJudge->SendVictimRequests(Attack);
	}
	return NumHitNow;
}

// =============================================================================
// 확정 · 알리기
// =============================================================================

void ULOACombatJudgeManagerSubsystem::BeginJudge(const FLOAAttackInfo& Attack, int32 NumTargets)
{
	// 남은 인원은 알리기 전에 적는다 (듣는 쪽이 무엇을 하든 셈이 맞게)
	if (NumTargets > 0)
	{
		FLOAJudgingAttack& Judging = JudgingAttacks.Add(Attack.AttackId);
		Judging.Attack = Attack;
		Judging.NumTargetsLeft = NumTargets;
	}

	OnJudgeStarted.Broadcast(Attack);

	// 대상이 없으면 바로 끝 (전에도 대상이 없어도 "타격 하나 끝남"이 갔다)
	if (NumTargets <= 0)
	{
		OnJudgeFinished.Broadcast(Attack.AttackId);
	}
}

ELOAHitOutcome ULOACombatJudgeManagerSubsystem::ConfirmHit(const FLOAAttackInfo& Attack, const FLOAJudgeResult& Result) const
{
	// 피함
	if (!Result.bInRange)
	{
		return ELOAHitOutcome::Missed;
	}

	// 막았나? (그 플레이어의 저가 컴포넌트가 스스로 판단. 플레이어가 아니면 컴포넌트가 없다)
	APawn* TargetPawn = Cast<APawn>(Result.Hit.Target);
	if (ULOAJustGuardComponent* JustGuardComp = TargetPawn ? TargetPawn->FindComponentByClass<ULOAJustGuardComponent>() : nullptr)
	{
		FLOAIncomingHit IncomingHit;
		IncomingHit.Attacker = Attack.Context.Caster;
		IncomingHit.AttackTags = Attack.Context.AttackTags;
		IncomingHit.AttackOrigin = Attack.AttackOrigin;
		IncomingHit.HitServerTime = Attack.HitServerTime;
		IncomingHit.SeenTime = Result.SeenTime;
		IncomingHit.MyLocation = Result.TargetLocation;
		IncomingHit.bClientSaidHit = Result.bClientSaidHit;
		if (JustGuardComp->JudgeJustGuard(IncomingHit))
		{
			LOA_LOG(this, Log, TEXT("[Judge] #%d : %s JUST GUARD"), Attack.AttackId, *GetNameSafe(TargetPawn));
			return ELOAHitOutcome::Blocked;
		}
	}

	// 맞음 → Payloads (기다리는 사이 죽었거나 시전자가 나갔으면 주지 않는다. 결과는 "맞음" 그대로)
	if (ULOACombatLibrary::IsTargetable(Result.Hit.Target) && IsValid(Attack.Context.SourceASC))
	{
		ULOACombatLibrary::ApplyPayloads(Attack.Context, TArray<FLOATargetHit>{ Result.Hit }, Attack.Payloads);
	}
	return ELOAHitOutcome::Hit;
}

ELOAHitOutcome ULOACombatJudgeManagerSubsystem::FinishTarget(const FLOAAttackInfo& Attack, const FLOAJudgeResult& Result)
{
	AActor* Target = Result.Hit.Target;
	const ELOAHitOutcome Outcome = ConfirmHit(Attack, Result);
	OnHitOutcome.Broadcast(Attack.AttackId, Target, Outcome);

	// 남은 인원 하나 줄이기 (적용·알림 중에 목록이 바뀌었을 수 있어 다시 찾는다)
	FLOAJudgingAttack* Judging = JudgingAttacks.Find(Attack.AttackId);
	if (Judging && --Judging->NumTargetsLeft <= 0)
	{
		JudgingAttacks.Remove(Attack.AttackId);
		OnJudgeFinished.Broadcast(Attack.AttackId);
	}
	return Outcome;
}

void ULOACombatJudgeManagerSubsystem::OnVictimJudged(int32 AttackId, const FLOAJudgeResult& Result)
{
	// 공격 정보는 복사본으로 (확정 중에 목록이 바뀌어도 안전하게)
	const FLOAJudgingAttack* Judging = JudgingAttacks.Find(AttackId);
	if (!Judging)
	{
		LOA_LOG(this, Warning, TEXT("[Judge] #%d : victim result for an attack that is not waiting. Ignored."), AttackId);
		return;
	}
	const FLOAAttackInfo Attack = Judging->Attack;
	FinishTarget(Attack, Result);
}

void ULOACombatJudgeManagerSubsystem::OnAttackReportJudged(const FLOAAttackInfo& ReportAttack, const TArray<FLOAJudgeResult>& Results)
{
	// 보고 하나 = 공격 하나. 번호를 매기고 시작 → 결과마다 확정 → 끝
	FLOAAttackInfo Attack = ReportAttack;
	Attack.AttackId = NextAttackId();

	BeginJudge(Attack, Results.Num());
	for (const FLOAJudgeResult& Result : Results)
	{
		FinishTarget(Attack, Result);
	}
}

int32 ULOACombatJudgeManagerSubsystem::NextAttackId()
{
	LastAttackId = (LastAttackId == MAX_int32) ? 1 : LastAttackId + 1;
	return LastAttackId;
}

// =============================================================================
// 클라
// =============================================================================

bool ULOACombatJudgeManagerSubsystem::ReactLocal(const FLOALocalHit& Hit)
{
	APawn* MyPawn = Hit.MyPawn;
	if (!MyPawn)
	{
		return false;
	}

	// ① 저가 먼저: 막았으면 성공 모션만 (CC는 서버도 안 건다)
	if (ULOAJustGuardComponent* JustGuardComp = MyPawn->FindComponentByClass<ULOAJustGuardComponent>())
	{
		FLOAIncomingHit IncomingHit;
		IncomingHit.Attacker = Hit.Attacker;
		IncomingHit.AttackTags = Hit.AttackTags;
		IncomingHit.AttackOrigin = Hit.AttackOrigin;
		IncomingHit.HitServerTime = Hit.HitServerTime;
		IncomingHit.SeenTime = Hit.SeenTime;
		IncomingHit.MyLocation = Hit.MyLocation;
		if (JustGuardComp->JudgeJustGuard(IncomingHit))
		{
			return true;
		}
	}

	// ② CC: 서버 GE를 기다리지 않고 먼저 반응 (이동·스킬 금지 + 몽타주). 서버 GA가 오면 이어받는다
	if (Hit.CrowdControlEffect)
	{
		if (ULOACrowdControlPredictionComponent* PredictionComp = MyPawn->FindComponentByClass<ULOACrowdControlPredictionComponent>())
		{
			PredictionComp->PredictCrowdControl(Hit.CrowdControlEffect, Hit.AttackOrigin, Hit.CrowdControlTimeout);
		}
	}
	return false;
}

// =============================================================================
// 월드
// =============================================================================

bool ULOACombatJudgeManagerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOACombatJudgeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 판정 방식 셋의 결과 알림을 묶는다 (판정 방식들은 이 매니저를 모른다. 알림 하나에 이 매니저 하나만 묶는다)
	if (ULOAServerJudgeSubsystem* ServerJudge = Collection.InitializeDependency<ULOAServerJudgeSubsystem>())
	{
		ServerJudge->OnLocalHit.BindUObject(this, &ThisClass::ReactLocal);
	}
	if (ULOAVictimJudgeSubsystem* VictimJudge = Collection.InitializeDependency<ULOAVictimJudgeSubsystem>())
	{
		VictimJudge->OnTargetJudged.BindUObject(this, &ThisClass::OnVictimJudged);
		VictimJudge->OnLocalHit.BindUObject(this, &ThisClass::ReactLocal);
	}
	if (ULOAAttackJudgeSubsystem* AttackJudge = Collection.InitializeDependency<ULOAAttackJudgeSubsystem>())
	{
		AttackJudge->OnReportJudged.BindUObject(this, &ThisClass::OnAttackReportJudged);
	}
}

void ULOACombatJudgeManagerSubsystem::Deinitialize()
{
	if (ULOAServerJudgeSubsystem* ServerJudge = ULOAServerJudgeSubsystem::Get(this))
	{
		ServerJudge->OnLocalHit.Unbind();
	}
	if (ULOAVictimJudgeSubsystem* VictimJudge = ULOAVictimJudgeSubsystem::Get(this))
	{
		VictimJudge->OnTargetJudged.Unbind();
		VictimJudge->OnLocalHit.Unbind();
	}
	if (ULOAAttackJudgeSubsystem* AttackJudge = ULOAAttackJudgeSubsystem::Get(this))
	{
		AttackJudge->OnReportJudged.Unbind();
	}

	// 맵이 내려가는 중이면 남은 공격은 알림 없이 버린다
	JudgingAttacks.Reset();

	Super::Deinitialize();
}
