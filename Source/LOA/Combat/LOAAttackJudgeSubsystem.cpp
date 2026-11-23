// LOAAttackJudgeSubsystem.cpp

#include "Combat/LOAAttackJudgeSubsystem.h"
#include "Combat/LOAHitJudgeTypes.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "Character/Monster/LOAPositionHistoryComponent.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"
#include "Core/LOANetTime.h"

#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetSerialization.h"
#include "UObject/CoreNet.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAttackJudgeSubsystem)

namespace LOAAttackJudge
{
	/**
	 * [검사 ①] 보고한 내 위치 ↔ 보고가 도착한 순간 서버가 아는 내 위치 (cm, 바닥 거리). 이 안이면 믿는다.
	 * 처음 값은 피해자 판정 보고 검사(ULOAVictimJudgeSubsystem)와 같은 150. PktLag 100 / 200 측정 후 다시 정한다 (2026-11-02)
	 */
	constexpr float LocationTolerance = 150.f;

	/**
	 * [검사 ③] 되감기 한도 = 지금 − (그 플레이어 핑 + 이 값) (초).
	 * 처음 값은 피해자 판정의 ReportTimeMargin과 같은 0.3. 측정 후 다시 정한다 (2026-11-02)
	 */
	constexpr double RewindMargin = 0.3;

	/** 서버가 아는 그 플레이어의 왕복 지연 (초). 핑(ms)을 초로 */
	double GetRoundTrip(const APawn* Pawn)
	{
		const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
		return PlayerState ? PlayerState->GetPingInMilliseconds() * 0.001 : 0.0;
	}
}

// =============================================================================
// 공격 보고 직렬화
// =============================================================================

FString FLOAAttackReportTargetData::ToString() const
{
	return FString::Printf(TEXT("LOAAttackReport module %d, %d target(s)"), ModuleIndex, Targets.Num());
}

bool FLOAAttackReportTargetData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	Ar << ModuleIndex;

	// 내 자리 (0.1cm)
	bool bLocationSuccess = true;
	CasterLocation.NetSerialize(Ar, Map, bLocationSuccess);
	bOutSuccess &= bLocationSuccess;

	// 내 방향 2바이트
	uint16 CompressedYaw = 0;
	if (Ar.IsSaving())
	{
		CompressedYaw = FRotator::CompressAxisToShort(CasterYaw);
	}
	Ar << CompressedYaw;
	if (Ar.IsLoading())
	{
		CasterYaw = FRotator::DecompressAxisFromShort(CompressedYaw);
	}

	// 내 화면이 보여 주는 서버 시각
	Ar << ViewServerTime;

	// 대상 수 (최대 MaxTargets. 받는 쪽도 그 이상이면 거절 = 조작된 데이터)
	uint8 NumTargets = static_cast<uint8>(FMath::Min(Targets.Num(), MaxTargets));
	Ar << NumTargets;
	if (Ar.IsLoading())
	{
		if (NumTargets > MaxTargets)
		{
			bOutSuccess = false;
			return false;
		}
		Targets.SetNum(NumTargets);
	}

	// 대상: 액터 참조는 PackageMap이 번호로 바꿔 보낸다.
	//   서버가 모르는 번호(그사이 사라진 액터)면 비어서 온다 → 서버가 그 대상만 건너뛴다 (보고 전체를 버리지 않는다)
	for (int32 Index = 0; Index < NumTargets; ++Index)
	{
		UObject* TargetObject = Targets[Index].Target;
		if (Map)
		{
			Map->SerializeObject(Ar, AActor::StaticClass(), TargetObject);
		}
		else
		{
			bOutSuccess = false;
		}
		if (Ar.IsLoading())
		{
			Targets[Index].Target = Cast<AActor>(TargetObject);
		}
		Ar << Targets[Index].TargetServerTime;
	}

	return true;
}

// =============================================================================
// 서브시스템
// =============================================================================

ULOAAttackJudgeSubsystem* ULOAAttackJudgeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOAAttackJudgeSubsystem>() : nullptr;
}

bool ULOAAttackJudgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOAAttackJudgeSubsystem::Deinitialize()
{
	// 남은 듣기 해제 (ASC가 먼저 사라졌으면 그냥 버림)
	for (TPair<int32, FLOAAttackReportListener>& Pair : Listeners)
	{
		if (UAbilitySystemComponent* ASC = Pair.Value.ASC.Get())
		{
			ASC->AbilityTargetDataSetDelegate(Pair.Value.SpecHandle, Pair.Value.ActivationKey).Remove(Pair.Value.DelegateHandle);
		}
	}
	Listeners.Reset();
	OnReportJudged.Unbind();

	Super::Deinitialize();
}

bool ULOAAttackJudgeSubsystem::IsAttackerJudged(const ULOAGameplayAbility* Ability, const ULOASkillTargeting* Targeting)
{
	// 범위가 있고(Self 아님) 적을 고르는 플레이어 스킬. 보스 패턴(ServerOnly)은 서버가 곧 시전자 화면 (헤더 [공격자 판정 공격인가])
	FLOAHitShape Shape;
	ELOATargetFilter Filter = ELOATargetFilter::Enemy;
	return Ability && Targeting && Targeting->GetRange(Shape, Filter)
		&& Filter == ELOATargetFilter::Enemy
		&& Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// =============================================================================
// 입구
// =============================================================================

void ULOAAttackJudgeSubsystem::BeginAttackReports(ULOAGameplayAbility* Ability, const ULOASkillModule* Module,
	const ULOASkillTargeting* Targeting, const TArray<FLOAEffectPayload>& Payloads, int32 NumAllowedReports)
{
	// 할 일(서버 + 원격 플레이어의 공격자 판정 공격)인지는 판정 매니저가 정해서 부른다
	if (!Ability || !Targeting || Payloads.IsEmpty())
	{
		return;
	}

	UAbilitySystemComponent* ASC = Ability->GetAbilitySystemComponentFromActorInfo();
	const int32 ModuleIndex = Ability->GetSkillModuleIndex(Module);
	if (!ASC || ModuleIndex == INDEX_NONE)
	{
		return;
	}

	// 같은 모듈의 옛 듣기가 남아 있으면 먼저 끝낸다 (끝 신호를 놓쳤을 때를 대비)
	EndAttackReports(Ability, Module);

	const int32 ListenerId = ++LastListenerId;
	FLOAAttackReportListener& Listener = Listeners.Add(ListenerId);
	Listener.Ability = Ability;
	Listener.Module = Module;
	Listener.Targeting = Targeting;
	Listener.ASC = ASC;
	Listener.ModuleIndex = ModuleIndex;
	Listener.Payloads = Payloads;
	Listener.SpecHandle = Ability->GetCurrentAbilitySpecHandle();
	Listener.ActivationKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
	Listener.NumReportsLeft = FMath::Max(NumAllowedReports, 1);

	// "이 발동의 TargetData가 오면 알려 줘" (커서와 같은 칸. 받는 함수에서 구조체 종류·모듈 번호로 가른다)
	Listener.DelegateHandle = ASC->AbilityTargetDataSetDelegate(Listener.SpecHandle, Listener.ActivationKey)
		.AddUObject(this, &ThisClass::OnReportReceived, ListenerId);

	// 듣기 전에 와 있었으면 바로 받는다 (발동과 같은 통로라 보통은 없다)
	ASC->CallReplicatedTargetDataDelegatesIfSet(Listener.SpecHandle, Listener.ActivationKey);
}

void ULOAAttackJudgeSubsystem::EndAttackReports(const ULOAGameplayAbility* Ability, const ULOASkillModule* Module)
{
	for (auto It = Listeners.CreateIterator(); It; ++It)
	{
		FLOAAttackReportListener& Listener = It.Value();
		if (Listener.Ability.Get() != Ability || Listener.Module.Get() != Module)
		{
			continue;
		}
		if (UAbilitySystemComponent* ASC = Listener.ASC.Get())
		{
			ASC->AbilityTargetDataSetDelegate(Listener.SpecHandle, Listener.ActivationKey).Remove(Listener.DelegateHandle);
		}
		It.RemoveCurrent();
	}
}

// =============================================================================
// 내 컴퓨터: 보고
// =============================================================================

void ULOAAttackJudgeSubsystem::SendReport(ULOAGameplayAbility* Ability, const ULOASkillModule* Module, const ULOASkillTargeting* Targeting)
{
	UAbilitySystemComponent* ASC = Ability->GetAbilitySystemComponentFromActorInfo();
	const FLOASkillModuleContext Context = Ability->MakeSkillModuleContext();
	const AActor* Caster = Context.Caster;
	const int32 ModuleIndex = Ability->GetSkillModuleIndex(Module);
	if (!ASC || !Context.IsValid() || !Caster || ModuleIndex == INDEX_NONE || ModuleIndex > MAX_uint8)
	{
		return;
	}

	// ① 내 화면으로 대상 찾기 (서버 판정과 같은 Targeting·같은 함수. 다른 점은 "내 컴퓨터가 아는 보스 자리"뿐)
	TArray<FLOATargetHit> Hits;
	Targeting->FindTargets(Context, Hits);
	if (Hits.IsEmpty())
	{
		return;	// 맞은 게 없으면 보낼 것도 없다 (서버는 이 노티파이에 대해 아무것도 안 한다)
	}

	// ② 보고: 내 자리·방향 + 내 화면이 보여 주는 서버 시각 + 대상마다 "그 대상 자리의 서버 시각"
	FLOAAttackReportTargetData* Report = new FLOAAttackReportTargetData();
	Report->ModuleIndex = static_cast<uint8>(ModuleIndex);
	Report->CasterLocation = Caster->GetActorLocation();
	Report->CasterYaw = Caster->GetActorRotation().Yaw;
	//   지금 서버 시각 − 편도 = 엔진 추정값 그대로 = 서버에서 방금 도착한 모습(보스·파란 빛)의 시각
	const UWorld* World = Caster->GetWorld();
	Report->ViewServerTime = static_cast<float>(LOANetTime::GetServerTimeNow(World) - LOANetTime::GetOneWayLatency(World));
	for (const FLOATargetHit& Hit : Hits)
	{
		if (Report->Targets.Num() >= FLOAAttackReportTargetData::MaxTargets)
		{
			break;
		}
		FLOAAttackReportTarget& Entry = Report->Targets.AddDefaulted_GetRef();
		Entry.Target = Hit.Target;
		// 엔진이 위치와 같이 보내 준 서버 시각을 그대로 옮겨 적는다 (보스: ULOAPositionHistoryComponent가 켬). 캐릭터가 아니면 0
		const ACharacter* TargetCharacter = Cast<ACharacter>(Hit.Target);
		Entry.TargetServerTime = TargetCharacter ? TargetCharacter->GetReplicatedServerLastTransformUpdateTimeStamp() : 0.f;
	}

	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(Report);	// Handle이 메모리 관리를 맡는다 (직접 delete하지 않음)

	// ③ 스킬 발동과 같은 ASC 통로로 (발동보다 먼저 도착하지 않는다). 예측 번호는 빈 값 → 서버 결과만
	ASC->ServerSetReplicatedTargetData(
		Ability->GetCurrentAbilitySpecHandle(),
		Ability->GetCurrentActivationInfo().GetActivationPredictionKey(),
		DataHandle,
		FGameplayTag(),
		FPredictionKey());

	// [측정] 보낸 대상과 그 자리의 서버 시각, 내 화면 서버 시각 (서버 로그의 rewind, view와 비교)
	for (const FLOAAttackReportTarget& Entry : Report->Targets)
	{
		LOA_LOG(Caster, Log, TEXT("[AttackReport] %s module %d : sent %s (target server time %.3f, view server time %.3f)"),
			*GetNameSafe(Ability->GetClass()), ModuleIndex, *GetNameSafe(Entry.Target), Entry.TargetServerTime, Report->ViewServerTime);
	}
}

// =============================================================================
// 서버: 받기 · 검사
// =============================================================================

void ULOAAttackJudgeSubsystem::OnReportReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag, int32 ListenerId)
{
	FLOAAttackReportListener* Listener = Listeners.Find(ListenerId);
	ULOAGameplayAbility* Ability = Listener ? Listener->Ability.Get() : nullptr;
	const ULOASkillTargeting* Targeting = Listener ? Listener->Targeting.Get() : nullptr;
	if (!Listener || !Ability || !Targeting)
	{
		return;
	}

	for (int32 DataIndex = 0; DataIndex < DataHandle.Num(); ++DataIndex)
	{
		// 같은 우편함(스킬·발동 번호마다 하나)으로 커서 데이터도 온다 → 공격 보고만. 모듈 번호가 다르면 이 GA의 다른 Apply 모듈 몫
		const FGameplayAbilityTargetData* Data = DataHandle.Get(DataIndex);
		if (!Data || Data->GetScriptStruct() != FLOAAttackReportTargetData::StaticStruct())
		{
			continue;
		}
		const FLOAAttackReportTargetData& Report = *static_cast<const FLOAAttackReportTargetData*>(Data);
		if (Report.ModuleIndex != Listener->ModuleIndex)
		{
			continue;
		}

		// 받았다 → 칸 비우기 (다시 듣기를 시작해도 같은 보고가 또 오지 않게)
		if (UAbilitySystemComponent* ASC = Listener->ASC.Get())
		{
			ASC->ConsumeClientReplicatedTargetData(Listener->SpecHandle, Listener->ActivationKey);
		}

		// 개수 제한: 몽타주의 그 노티파이 수까지
		if (Listener->NumReportsLeft <= 0)
		{
			LOA_LOG(Ability->GetAvatarActorFromActorInfo(), Warning,
				TEXT("[AttackReport] %s module %d : too many reports in one activation. Ignored."),
				*GetNameSafe(Ability->GetClass()), Listener->ModuleIndex);
			continue;
		}
		--Listener->NumReportsLeft;

		const FLOASkillModuleContext Context = Ability->MakeSkillModuleContext();
		if (!Context.IsValid())
		{
			continue;
		}

		// 검사 → 대상마다 결과
		TArray<FLOAJudgeResult> Results;
		FVector AttackOrigin = FVector::ZeroVector;
		if (!JudgeReport(Targeting, Context, Report, Results, AttackOrigin) || Results.IsEmpty())
		{
			return;
		}
		int32 NumInRange = 0;
		for (const FLOAJudgeResult& Result : Results)
		{
			NumInRange += Result.bInRange ? 1 : 0;
		}
		LOA_LOG(Context.SourceActor.Get(), Log, TEXT("[Apply] %s : %d target(s) passed by attack report (%d reported)"),
			*GetNameSafe(Ability->GetClass()), NumInRange, Report.Targets.Num());

		// 판정 매니저에 올린다 (공격 번호는 판정 매니저가 매긴다)
		//   판정 매니저가 적용하는 중에 데미지로 GA가 끝나 듣기가 지워질 수 있다 → 필요한 값은 미리 복사하고, 올린 뒤에는 Listener를 쓰지 않는다
		FLOAAttackInfo Attack;
		Attack.Context = Context;
		Attack.Payloads = Listener->Payloads;
		Attack.Timing = ELOAHitJudgeTiming::Animation;
		Attack.AttackOrigin = AttackOrigin;
		Attack.HitServerTime = LOANetTime::GetServerTimeNow(GetWorld());
		OnReportJudged.ExecuteIfBound(Attack, Results);
		return;	// 한 번에 보고 하나 (칸이 하나라 같이 온 것이 없다)
	}
}

bool ULOAAttackJudgeSubsystem::JudgeReport(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
	const FLOAAttackReportTargetData& Report, TArray<FLOAJudgeResult>& OutResults, FVector& OutAttackOrigin)
{
	// 결과가 남는 일은 서버만
	const APawn* Caster = Cast<APawn>(Context.Caster);
	UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	if (!Targeting || !Caster || !World || !Caster->HasAuthority() || !IsValid(Context.SourceASC))
	{
		return false;
	}

	// 서버 시계 = 월드 시각 (엔진이 보스 위치에 붙이는 시각, 자리 기록과 같은 시계. 서버의 LOANetTime도 같은 값)
	const double Now = World->GetTimeSeconds();

	// ① 내 자리: 보고한 위치 ↔ 지금 서버가 아는 내 위치 (보고와 이동이 같이 도착하므로 정상이면 거의 같다)
	const float CasterDistance = FVector::Dist2D(Report.CasterLocation, Caster->GetActorLocation());
	const float YawDifference = FMath::Abs(FMath::FindDeltaAngleDegrees(Report.CasterYaw, Caster->GetActorRotation().Yaw));
	if (CasterDistance > LOAAttackJudge::LocationTolerance)
	{
		LOA_LOG(Caster, Warning, TEXT("[AttackReport] rejected all %d target(s): reported caster location is %.0fcm from server (limit %.0f)"),
			Report.Targets.Num(), CasterDistance, LOAAttackJudge::LocationTolerance);
		return false;
	}

	// 범위: Targeting이 내준 모양 + 편. 보고한 내 자리·방향에 놓는다 (서버 검색과 같은 규칙: ForwardDistance, 땅에 붙이기)
	FLOAHitShape Shape;
	ELOATargetFilter Filter = ELOATargetFilter::Enemy;
	if (!Targeting->GetRange(Shape, Filter))
	{
		return false;
	}
	const FVector ShapeCenter = ULOACombatLibrary::GetShapeCenterAt(World, Report.CasterLocation,
		FRotator(0.f, Report.CasterYaw, 0.f).Vector(), Shape, Caster);
	OutAttackOrigin = ShapeCenter;
	auto IsInRange = [&](const AActor* Target, const FVector& TargetLocation)
	{
		// 편 + 바닥 모양·높이 (서버 검색·피해자 판정과 같은 함수)
		return ULOACombatLibrary::PassesFilter(Caster, Target, Filter)
			&& ULOACombatLibrary::IsBodyInShape(Shape, ShapeCenter, Report.CasterYaw,
				TargetLocation, Target->GetSimpleCollisionRadius(), Target->GetSimpleCollisionHalfHeight());
	};

	// 되감기 한도: 지금 − (그 플레이어 핑 + 여유). 기록 길이보다 길게 되감지 않는다
	const double MaxRewind = FMath::Min(LOAAttackJudge::GetRoundTrip(Caster) + LOAAttackJudge::RewindMargin,
		ULOAPositionHistoryComponent::HistoryLength);

	// 공격 판정 기록의 시각 = 내 화면이 보여 주는 서버 시각. 자리 시각과 같은 한도로 자른다 (지금보다 미래도 지금으로). 없으면 지금
	//   보스가 제자리여도 계속 흐르는 시각이라 카운터(그 시각에 파란 구간이었나)에 쓴다 (2026-11-03)
	double AttackJudgeTime = Now;
	bool bViewClamped = false;
	if (Report.ViewServerTime > 0.f)
	{
		AttackJudgeTime = FMath::Min(static_cast<double>(Report.ViewServerTime), Now);
		if (AttackJudgeTime < Now - MaxRewind)
		{
			AttackJudgeTime = Now - MaxRewind;
			bViewClamped = true;
		}
	}

	for (const FLOAAttackReportTarget& Entry : Report.Targets)
	{
		AActor* Target = Entry.Target;

		// ② 대상: 지금 맞을 수 있나 (그사이 죽었거나 사라졌으면 건너뜀) / 같은 대상 두 번은 한 번만. 편은 ④에서 본다
		if (!Target || !ULOACombatLibrary::IsTargetable(Target)
			|| OutResults.ContainsByPredicate([Target](const FLOAJudgeResult& Existing) { return Existing.Hit.Target == Target; }))
		{
			LOA_LOG(Caster, Log, TEXT("[AttackReport] skipped %s (gone, dead or duplicate)"), *GetNameSafe(Target));
			continue;
		}

		// ③ 되감기: 보고에 적힌 서버 시각의 자리·방향. 시각이 없거나 기록이 없으면 지금 자리
		FVector TargetLocation = Target->GetActorLocation();
		float TargetYaw = Target->GetActorRotation().Yaw;
		double RewindAge = 0.0;		// 몇 초 전으로 되감았나 (로그)
		bool bClamped = false;
		const ULOAPositionHistoryComponent* History = Target->FindComponentByClass<ULOAPositionHistoryComponent>();
		if (History && Entry.TargetServerTime > 0.f)
		{
			double RewindTime = FMath::Min(static_cast<double>(Entry.TargetServerTime), Now);
			if (RewindTime < Now - MaxRewind)
			{
				// 한도보다 옛날. 보스가 오래 서 있었으면 정상이다 (마지막으로 움직인 시각이 옛날). 그때는 한도 시각 자리도 같은 자리
				RewindTime = Now - MaxRewind;
				bClamped = true;
			}
			History->FindAt(RewindTime, TargetLocation, TargetYaw);
			RewindAge = Now - RewindTime;
		}

		// ④ 범위: A가 보고한 자리·방향에 스킬 모양을 놓고, A 화면 시각으로 되감은 보스 자리가 그 안에 있나
		const bool bInRange = IsInRange(Target, TargetLocation);

		// [측정] 되감은 시간, 그사이 대상이 움직인 거리(= 되감지 않았으면 어긋났을 거리), 지금 자리였으면 맞았나, 내 자리·방향 차이
		const float TargetMoved = FVector::Dist2D(TargetLocation, Target->GetActorLocation());
		const bool bInRangeNow = IsInRange(Target, Target->GetActorLocation());
		LOA_LOG(Caster, Log, TEXT("[AttackReport] %s : %s | rewind %.3fs%s (limit %.3f), target moved %.0fcm since, in range now = %s | view %.3fs%s | caster diff %.0fcm, yaw diff %.1f"),
			*GetNameSafe(Target), bInRange ? TEXT("HIT") : TEXT("REJECTED"), RewindAge, bClamped ? TEXT(" (clamped)") : TEXT(""), MaxRewind,
			TargetMoved, bInRangeNow ? TEXT("yes") : TEXT("no"), Now - AttackJudgeTime, bViewClamped ? TEXT(" (clamped)") : TEXT(""),
			CasterDistance, YawDifference);

		// 결과 하나 (실패 = 피함, 통과 = 맞을 자리)
		FLOAJudgeResult& Result = OutResults.AddDefaulted_GetRef();
		Result.Hit.Target = Target;
		Result.bInRange = bInRange;
		Result.SeenTime = AttackJudgeTime;
		Result.TargetLocation = TargetLocation;
		if (!bInRange)
		{
			LOA_LOG(Caster, Warning, TEXT("[AttackReport] rejected %s: not in range at rewound location"), *GetNameSafe(Target));
			continue;
		}

		// 통과 → 맞은 지점(데미지 숫자 위치)은 지금 몸 기준(서버 검색과 같은 규칙)
		//        공격 판정 기록의 공격 방향·백·헤드는 보고한 내 자리 + 되감은 자리·방향으로, 시각은 내 화면 서버 시각으로
		Result.Hit.bHasHitPoint = ULOACombatLibrary::MakeShapeHitResult(Target, ShapeCenter, Result.Hit.HitResult);
		Result.Hit.bHasDirectionBasis = true;
		Result.Hit.AttackerLocation = Report.CasterLocation;
		Result.Hit.TargetLocation = TargetLocation;
		Result.Hit.TargetYaw = TargetYaw;
		Result.Hit.AttackJudgeServerTime = AttackJudgeTime;
	}
	return true;
}
