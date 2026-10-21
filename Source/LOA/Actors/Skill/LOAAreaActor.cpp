// LOAAreaActor.cpp

#include "Actors/Skill/LOAAreaActor.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting_Shape.h"
#include "Library/LOACombatLibrary.h"
#include "Combat/Telegraph/LOATelegraphSubsystem.h"
#include "Combat/Telegraph/LOATelegraphTypes.h"
#include "Combat/LOAHitJudgeSubsystem.h"
#include "Core/LOALog.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAreaActor)

ALOAAreaActor::ALOAAreaActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 움직이지 않는 액터. 처음 위치/방향만 복제되면 된다. (bReplicates는 부모가 켬)
	SetReplicateMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ALOAAreaActor::SetFirstHitDelay(float Seconds)
{
	FirstHitDelayOverride = FMath::Max(Seconds, 0.f);
}

void ALOAAreaActor::BeginPlay()
{
	Super::BeginPlay();

	// 클라: 아무것도 하지 않는다. (판정은 서버, 예고는 서버가 따로 보낸다)
	if (!HasAuthority())
	{
		return;
	}

	// 설정 누락: 판정할 수 없으니 연출 시간만큼 두었다가 사라진다.
	if (!Targeting)
	{
		LOA_LOG(this, Warning, TEXT("Area actor has no Targeting. It will do nothing. Check the BP Class Defaults."));
		SetLifeSpan(FMath::Max(LingerTime, 0.1f));
		return;
	}
	if (GetSkillPayloads().IsEmpty())
	{
		LOA_LOG(this, Warning, TEXT("Area actor has no Payloads. It will hit but do nothing. Check the Spawn module."));
	}

	// 첫 판정까지의 초: Spawn 모듈이 넘겨준 값(몽타주 노티파이) > BP의 First Tick Delay
	const float FirstHitDelay = FirstHitDelayOverride >= 0.f ? FirstHitDelayOverride : FirstTickDelay;

	// 예고: 이 자리, 판정 모양 그대로, 첫 판정 순간 꽉 차게. (겉모습이 없거나 0초면 예고 없음)
	//   예) 1.2 → 모든 플레이어 화면에 이 자리 1.2초짜리 예고
	TelegraphId = ShowTelegraph(FirstHitDelay);

	// 첫 판정. 타이머에 0초를 주면 예약이 안 되므로 0이면 바로 판정한다.
	// 서버 시계와 타이머는 같은 월드 시간으로 흐른다 → 예고가 꽉 차는 순간 = 이 타이머 (slomo도 같이 늦어짐)
	if (FirstHitDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(TickTimerHandle, this, &ThisClass::DoTick, FirstHitDelay, false);
	}
	else
	{
		DoTick();
	}
}

void ALOAAreaActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 사라지는 중에 남은 판정 예약이 실행되지 않게 지운다.
	GetWorldTimerManager().ClearTimer(TickTimerHandle);

	// 첫 판정 전에 사라짐 (Spawn 모듈이 패턴 취소로 지움) → 예고도 지운다. 판정 없이 예고만 꽉 차는 일이 없게
	if (HasAuthority() && TelegraphId != 0)
	{
		ULOATelegraphSubsystem::HideToAll(this, TelegraphId);
		TelegraphId = 0;
	}

	Super::EndPlay(EndPlayReason);
}

void ALOAAreaActor::DoTick()
{
	if (!HasAuthority() || !Targeting || IsActorBeingDestroyed())
	{
		return;
	}

	++TicksDone;

	// 첫 판정이 났다 → 예고는 판정 시각에 스스로 사라진다. 이제 이 액터가 지워져도 예고를 지울 필요 없음
	TelegraphId = 0;

	// ①② 범위 안 대상 판정 + Payloads 적용. 판정 방식(서버 / 피해자)은 판정 담당이 Targeting을 보고 정한다.
	//    (모양 기준점 = 이 액터 / 편 판단 = 시전자. Context는 InitSkillActor에서 그렇게 바뀌어 있다)
	//    플레이어가 보고 피하는 것: 예고를 띄우는 장판 = 예고 / 예고 없는 장판(보이지 않는 휘두르기 등) = 보스 모션
	const ELOAHitJudgeTiming Timing = TelegraphStyle ? ELOAHitJudgeTiming::Telegraph : ELOAHitJudgeTiming::Animation;
	const int32 NumNow = ULOAHitJudgeSubsystem::JudgeAndApply(Targeting, GetSkillContext(), GetSkillPayloads(), Timing);

	LOA_LOG(this, Log, TEXT("[Area] tick %d/%d : %d target(s) now"), TicksDone, TickCount, NumNow);

	// ③ 남았으면 다음 판정, 다 했으면 사라지기
	if (TicksDone < TickCount)
	{
		// 0초 타이머는 예약되지 않아서 판정이 멈추고 액터가 영원히 남는다. → 최소 0.01초 (칸 자체도 0.01 이상으로 막혀 있음)
		GetWorldTimerManager().SetTimer(TickTimerHandle, this, &ThisClass::DoTick, FMath::Max(TickInterval, 0.01f), false);
	}
	else if (LingerTime > 0.f)
	{
		SetLifeSpan(LingerTime);
	}
	else
	{
		Destroy();
	}
}

int32 ALOAAreaActor::ShowTelegraph(float SecondsUntilHit) const
{
	// 겉모습을 안 골랐으면 예고 없음 (플레이어 스킬 낫 등)
	if (!TelegraphStyle)
	{
		return 0;
	}

	// 예고 모양 = 판정 모양. 모양을 아는 Targeting은 Shape뿐이다.
	const ULOASkillTargeting_Shape* ShapeTargeting = Cast<ULOASkillTargeting_Shape>(Targeting);
	if (!ShapeTargeting)
	{
		LOA_LOG(this, Warning, TEXT("[Area] Telegraph Style is set but Targeting is not Shape. No telegraph."));
		return 0;
	}
	const FLOAHitShape& HitShape = ShapeTargeting->GetShape();

	// ① 겉모습 + 모양 (판정 모양 그대로. 예: Box 800 × 200)
	FLOATelegraphData Data;
	Data.Style = TelegraphStyle;
	Data.Shape = HitShape;

	// ② 자리와 방향 = 판정과 같은 규칙. 기준점 = 이 액터 위치 + 앞 × ForwardDistance, 방향 = 이 액터 앞
	Data.Location = ULOACombatLibrary::GetShapeCenter(this, HitShape);
	Data.Yaw = GetActorRotation().Yaw;

	// ③ 모든 플레이어 화면으로 (시각과 번호는 입구가 채운다)
	return ULOATelegraphSubsystem::ShowToAll(this, Data, SecondsUntilHit);
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ALOAAreaActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// 메시지 앞에 붙일 이름 (BP 클래스 이름. 예: BP_Area_GuillotineSwing_C)
	const FString Name = GetNameSafe(GetClass());

	if (!Targeting)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Targeting is not set."), *Name)));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		const FString TargetingPrefix = FString::Printf(TEXT("%s Targeting"), *Name);
		Result = CombineDataValidationResults(Result, Targeting->ValidateTargeting(Context, TargetingPrefix));
	}

	if (TickCount > 1 && TickInterval <= 0.f)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Tick Count is %d but Tick Interval is %.2f. Tick Interval must be greater than 0."),
			*Name, TickCount, TickInterval)));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
