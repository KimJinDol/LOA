// LOAGA_Charge.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_Charge.h"
#include "AbilitySystem/Tasks/LOAAbilityTask_WaitChargeRelease.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Charge)

namespace LOAChargeFreeze
{
	/** 블렌드 아웃 시작 시점보다 이만큼 더 앞에서 멈춘다 (초, 몽타주 시간). 프레임이 늦게 와도 블렌드 아웃에 들어가지 않게 */
	constexpr float SafetyMargin = 0.05f;

	/**
	 * 멈춤 재생 속도. 0이 아니라 아주 작은 값 (헤더 상단 [몽타주] ②)
	 * 0이면 엔진이 멈춘 동안의 시간을 쌓아 두었다가 풀 때 한꺼번에 써서 Release가 끝부분만 나온다.
	 * 0이 아니면 매 프레임 그 프레임 시간을 바로 쓴다 (아주 조금 움직임) → 쌓이지 않음
	 * 움직이는 양: 1초 멈춤 = 몽타주 0.001초. SafetyMargin(0.05)을 다 쓰려면 50초 → 최대 차지 시간보다 훨씬 김
	 */
	constexpr float FrozenPlayRate = 0.001f;
}

ULOAGA_Charge::ULOAGA_Charge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 인스턴스 정책, 예측 실행, 이동 금지, 커서 회전(누를 때 한 번)은 부모 기본값을 그대로 쓴다.
}

// =============================================================================
// 흐름
// =============================================================================

void ULOAGA_Charge::StartSkill()
{
	// 발동마다 초기화 (인스턴스를 재사용하므로 이전 발동 값이 남지 않게)
	ChargeMultiplier = 1.f;
	bMontageFrozen = false;
	FreezeTask = nullptr;
	bChargeStarted = false;
	bChargeReleased = false;

	// ① 몽타주 재생. 끝나면(Release 섹션까지 다 돌면) 부모가 종료한다.
	if (!PlayMontageAndEnd())
	{
		LOA_LOG(GetAvatarActorFromActorInfo(), Warning, TEXT("[%s] Charge: Montage is not set."), *GetNameSafe(GetClass()));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// ①-2 루프가 없으면 첫 섹션 끝에서 멈출 예약. (루프가 있으면 몽타주 섹션 설정대로 반복된다)
	if (LoopSectionName.IsNone())
	{
		// 0초를 주면 타이머가 켜지지 않으므로 아주 작은 값으로 (SetTimer 규칙)
		FreezeTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(GetChargeFreezeDelay(), KINDA_SMALL_NUMBER));
		FreezeTask->OnFinish.AddDynamic(this, &ThisClass::OnChargeFreeze);
		FreezeTask->ReadyForActivation();
	}

	// ② 뗌 / 최대 시간 대기. Task 주인 = 이 Ability → 끊기면 엔진이 정리한다.
	ULOAAbilityTask_WaitChargeRelease* WaitTask = ULOAAbilityTask_WaitChargeRelease::WaitChargeRelease(this, MaxChargeTime);
	WaitTask->OnReleased.AddDynamic(this, &ThisClass::OnChargeReleased);
	WaitTask->ReadyForActivation();

	// ③ 차지 게이지 보이기 (조종하는 사람 컴퓨터만. 헤더 [차지 게이지 UI])
	bChargeStarted = true;
	SendChargeUIEvent(LOAGameplayTags::Event_Skill_Charge_Started);
}

void ULOAGA_Charge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 차지를 시작했는데 떼기 전에 끝남 (회피·CC로 끊김) → 차지 게이지 숨기기
	if (IsEndAbilityValid(Handle, ActorInfo) && bChargeStarted && !bChargeReleased)
	{
		SendChargeUIEvent(LOAGameplayTags::Event_Skill_Charge_Cancelled);
	}
	bChargeStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULOAGA_Charge::SendChargeUIEvent(const FGameplayTag& EventTag) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;	// UI는 조종하는 사람 컴퓨터에만 있다
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetAvatarActorFromActorInfo();
	Payload.OptionalObject = this;
	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
}

void ULOAGA_Charge::OnChargeReleased(float HeldTime, bool bServerTimedOut)
{
	// ① 배율 확정. 이후 모듈이 만드는 Context에 이 값이 들어간다.
	bChargeReleased = true;
	bool bPerfect = false;
	ChargeMultiplier = CalculateChargeMultiplier(HeldTime, bPerfect);

	LOA_LOG(GetAvatarActorFromActorInfo(), Log, TEXT("[Charge] %s : held %.2fs -> x%.2f%s"),
		*GetNameSafe(GetClass()), HeldTime, ChargeMultiplier, bPerfect ? TEXT(" (Perfect)") : TEXT(""));

	// ② 발사 신호. Trigger Event Tag = Event.Skill.Charge.Released 인 모듈이 받는다. (이펙트, 커서 따라 돌기 멈춤 …)
	//    EventMagnitude에 누른 시간을 실어 보낸다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayEventData Payload;
		Payload.Instigator = GetAvatarActorFromActorInfo();
		Payload.EventMagnitude = HeldTime;
		Payload.OptionalObject = this;	// 차지 게이지 UI가 도달 단계를 물어본다 (GetReachedStageCount)

		Payload.EventTag = LOAGameplayTags::Event_Skill_Charge_Released;
		ASC->HandleGameplayEvent(Payload.EventTag, &Payload);

		if (bPerfect)
		{
			Payload.EventTag = LOAGameplayTags::Event_Skill_Charge_Perfect;
			ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		}
	}

	// ③ 발사 몽타주로. (모듈이 ②에서 Ability를 끝냈으면 하지 않음)
	//    재생 속도 복구는 양쪽 다 한다. (서버 몽타주도 서버의 멈춤 타이머로 멈춰 있다)
	//    점프는 조종하는 사람 컴퓨터만 한다. (헤더 [네트워크] - Release 점프)
	if (IsActive())
	{
		// 아직 안 멈췄으면 멈춤 예약 취소
		if (FreezeTask)
		{
			FreezeTask->EndTask();
			FreezeTask = nullptr;
		}

		// 멈춰 있었으면 재생 속도를 되돌린다.
		if (bMontageFrozen)
		{
			bMontageFrozen = false;
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->CurrentMontageSetPlayRate(MontagePlayRate);
			}
		}

		const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
		const bool bLocallyControlled = ActorInfo && ActorInfo->IsLocallyControlled();
		if (bLocallyControlled || bServerTimedOut)
		{
			MontageJumpToSection(ReleaseSectionName);
		}
		// 서버(원격 플레이어)의 정상 경로: 클라이언트의 엔진 점프 RPC가 바로 뒤에 도착해서 점프한다.
	}
}

// =============================================================================
// 루프 없는 몽타주 (② 멈춤)
// =============================================================================

float ULOAGA_Charge::GetChargeFreezeDelay() const
{
	const UAnimMontage* MontageToPlay = GetMontage();
	if (!MontageToPlay)
	{
		return 0.f;
	}

	// 첫 섹션 = 몽타주 0초 위치의 섹션 (PlayMontageAndEnd는 처음부터 재생한다)
	const int32 FirstIndex = MontageToPlay->GetSectionIndexFromPosition(0.f);
	const float SectionLength = MontageToPlay->GetSectionLength(FirstIndex);

	// 블렌드 아웃 시작 = 끝에서 BlendOutTriggerTime 전 (음수면 블렌드 아웃 시간을 쓴다)
	const float BlendOutTrigger = MontageToPlay->BlendOutTriggerTime >= 0.f
		? MontageToPlay->BlendOutTriggerTime
		: MontageToPlay->GetDefaultBlendOutTime();

	// 몽타주 시간 → 실제 시간 (재생 속도로 나눔)
	const float FreezePosition = FMath::Max(SectionLength - BlendOutTrigger - LOAChargeFreeze::SafetyMargin, 0.f);
	return FreezePosition / FMath::Max(MontagePlayRate, KINDA_SMALL_NUMBER);
}

void ULOAGA_Charge::OnChargeFreeze()
{
	FreezeTask = nullptr;

	// 재생 속도 거의 0 = 그 자세로 멈춤 (정확히 0은 안 됨: FrozenPlayRate 설명). ASC를 통해서 바꾸므로 다른 플레이어 화면에도 복제된다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageSetPlayRate(LOAChargeFreeze::FrozenPlayRate);
		bMontageFrozen = true;
	}
}

// =============================================================================
// 배율 계산
// =============================================================================

float ULOAGA_Charge::CalculateChargeMultiplier(float HeldTime, bool& bOutPerfect) const
{
	bOutPerfect = false;

	// 퍼펙트 존 (두 방식 공통). 존 안이면 단계와 상관없이 퍼펙트 배율.
	if (bUsePerfectZone && HeldTime >= PerfectZoneStart && HeldTime <= PerfectZoneEnd)
	{
		bOutPerfect = true;
		return PerfectMultiplier;
	}

	// 단계가 없으면 (설정 누락 - 저장 시 검사가 오류로 알림) 가장 낮은 배율
	if (Stages.IsEmpty())
	{
		return BaseMultiplier;
	}

	// 단계 1개 → 비율식: 단계 배율 × 채운 비율 (최솟값 MinChargeRatio)
	if (Stages.Num() == 1)
	{
		const FLOAChargeStage& FullStage = Stages[0];
		const float Ratio = FullStage.Time > 0.f ? HeldTime / FullStage.Time : 1.f;
		return FullStage.Multiplier * FMath::Clamp(Ratio, MinChargeRatio, 1.f);
	}

	// 단계 2개 이상 → 단계식: 끝까지 채운 마지막 단계의 배율. 하나도 못 채웠으면 Base.
	float Result = BaseMultiplier;
	for (const FLOAChargeStage& Stage : Stages)
	{
		if (HeldTime >= Stage.Time)
		{
			Result = Stage.Multiplier;
		}
	}
	return Result;
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOAGA_Charge::IsDataValid(FDataValidationContext& Context) const
{
	// 부모: 쿨타임 태그, 모듈 검사
	EDataValidationResult Result = Super::IsDataValid(Context);
	const FString OwnerName = GetNameSafe(GetClass());

	auto AddError = [&Context, &Result, &OwnerName](const FString& Message)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Charge : %s"), *OwnerName, *Message)));
		Result = EDataValidationResult::Invalid;
	};

	// ① 단계
	if (Stages.IsEmpty())
	{
		AddError(TEXT("Stages is empty. Add at least one stage."));
	}

	for (int32 Index = 1; Index < Stages.Num(); ++Index)
	{
		if (Stages[Index].Time <= Stages[Index - 1].Time)
		{
			AddError(FString::Printf(TEXT("Stages[%d] Time (%.2f) must be greater than Stages[%d] Time (%.2f)."),
				Index, Stages[Index].Time, Index - 1, Stages[Index - 1].Time));
		}
	}

	// ② 최대 시간은 마지막 단계 이상 (못 채우는 단계가 생기지 않게)
	if (!Stages.IsEmpty() && MaxChargeTime < Stages.Last().Time)
	{
		AddError(FString::Printf(TEXT("Max Charge Time (%.2f) is shorter than the last stage Time (%.2f)."),
			MaxChargeTime, Stages.Last().Time));
	}

	// ③ 퍼펙트 존 범위
	if (bUsePerfectZone)
	{
		if (PerfectZoneStart >= PerfectZoneEnd)
		{
			AddError(FString::Printf(TEXT("Perfect Zone Start (%.2f) must be less than End (%.2f)."), PerfectZoneStart, PerfectZoneEnd));
		}
		if (PerfectZoneStart > MaxChargeTime)
		{
			AddError(FString::Printf(TEXT("Perfect Zone Start (%.2f) is after Max Charge Time (%.2f). It can never be reached."),
				PerfectZoneStart, MaxChargeTime));
		}
	}

	// ④ 몽타주 섹션 구조
	if (Montage)
	{
		const FString MontageName = GetNameSafe(Montage);

		// Release 섹션이 있는지
		const int32 ReleaseIndex = Montage->GetSectionIndex(ReleaseSectionName);
		if (ReleaseIndex == INDEX_NONE)
		{
			AddError(FString::Printf(TEXT("Montage %s has no section '%s'."), *MontageName, *ReleaseSectionName.ToString()));
		}

		// 차지 중에 Release가 저절로 재생되면 안 된다 → 첫 섹션부터 다음 섹션을 따라가며 확인
		const int32 FirstIndex = Montage->GetSectionIndexFromPosition(0.f);
		if (FirstIndex != INDEX_NONE && FirstIndex == ReleaseIndex)
		{
			AddError(FString::Printf(TEXT("Montage %s starts with the Release section. Add a charge section before it."), *MontageName));
		}
		else if (FirstIndex != INDEX_NONE && !LoopSectionName.IsNone())
		{
			// ① 루프 있음: Loop 섹션이 있고, 다음 섹션이 자기 자신이고, 첫 섹션 → … → Loop 사이에 Release가 없어야 한다.
			const int32 LoopIndex = Montage->GetSectionIndex(LoopSectionName);
			if (LoopIndex == INDEX_NONE)
			{
				AddError(FString::Printf(TEXT("Montage %s has no section '%s' (Loop Section)."), *MontageName, *LoopSectionName.ToString()));
			}
			else
			{
				if (Montage->CompositeSections[LoopIndex].NextSectionName != LoopSectionName)
				{
					AddError(FString::Printf(TEXT("Montage %s : Loop section '%s' must have itself as the next section."),
						*MontageName, *LoopSectionName.ToString()));
				}

				// 첫 섹션부터 다음 섹션 따라가기 (섹션 수만큼만 → 엉뚱한 고리에 빠져도 끝난다)
				int32 Index = FirstIndex;
				bool bReachedLoop = false;
				for (int32 Step = 0; Step < Montage->CompositeSections.Num() && Index != INDEX_NONE; ++Step)
				{
					if (Index == LoopIndex)
					{
						bReachedLoop = true;
						break;
					}
					if (Index == ReleaseIndex)
					{
						break;
					}
					Index = Montage->GetSectionIndex(Montage->CompositeSections[Index].NextSectionName);
				}

				if (!bReachedLoop)
				{
					AddError(FString::Printf(TEXT("Montage %s : Loop section '%s' is never reached from the first section '%s' (or Release plays before it). Check the next-section links."),
						*MontageName, *LoopSectionName.ToString(), *Montage->GetSectionName(FirstIndex).ToString()));
				}
			}
		}
		else if (FirstIndex != INDEX_NONE)
		{
			// ② 루프 없음: 첫 섹션의 다음 섹션(Montage Sections 패널의 연결)은 비어 있어야 한다.
			//    정상이면 코드가 섹션 끝보다 먼저 멈추므로 연결이 있어도 차이가 없다.
			//    차이는 멈추는 타이밍이 늦었을 때(렉으로 프레임이 길어짐 등)만 생긴다.
			//      - Release로 이어져 있으면 : Release가 저절로 재생 → Hit 노티파이 → 키를 안 뗐는데 데미지 (버그)
			//      - 비어 있으면            : 몽타주가 끝남 → 스킬이 취소될 뿐, 데미지 없음 (안전한 실패)
			//    그래서 비어 있지 않으면 오류로 막는다.
			const FName FirstNext = Montage->CompositeSections[FirstIndex].NextSectionName;
			if (!FirstNext.IsNone())
			{
				AddError(FString::Printf(TEXT("Montage %s : Loop Section is None, so the first section '%s' must have no next section (now '%s')."),
					*MontageName, *Montage->GetSectionName(FirstIndex).ToString(), *FirstNext.ToString()));
			}

			// 첫 섹션이 블렌드 아웃보다 짧으면 0초에 멈춘다 (동작은 하지만 시작 자세 그대로)
			if (GetChargeFreezeDelay() <= 0.f)
			{
				Context.AddWarning(FText::FromString(FString::Printf(
					TEXT("%s : Charge : First section '%s' of %s is shorter than the blend out time. The charge pose will freeze at the very start."),
					*OwnerName, *Montage->GetSectionName(FirstIndex).ToString(), *MontageName)));
			}
		}
	}

	return Result;
}
#endif

// =============================================================================
// 차지 게이지 UI (헤더 [차지 게이지 UI])
// =============================================================================

FLOAChargeGaugeInfo ULOAGA_Charge::MakeChargeGaugeInfo() const
{
	FLOAChargeGaugeInfo Info;
	Info.MaxChargeTime = MaxChargeTime;
	for (const FLOAChargeStage& Stage : Stages)
	{
		Info.StageTimes.Add(Stage.Time);
	}
	Info.StageTimes.Sort();	// 저장 시 검사가 순서를 보지만, 게이지 계산은 오름차순을 가정하므로 한 번 더
	Info.bUsePerfectZone = bUsePerfectZone;
	Info.PerfectZoneStart = PerfectZoneStart;
	Info.PerfectZoneEnd = PerfectZoneEnd;
	return Info;
}

int32 ULOAGA_Charge::GetReachedStageCount(float HeldTime) const
{
	int32 Count = 0;
	for (const FLOAChargeStage& Stage : Stages)
	{
		if (HeldTime >= Stage.Time)
		{
			++Count;
		}
	}
	return Count;
}
