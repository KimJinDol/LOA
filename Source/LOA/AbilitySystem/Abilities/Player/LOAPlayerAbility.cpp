// LOAPlayerAbility.cpp

#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "Character/Player/LOACharacter.h"
#include "Player/LOAPlayerController.h"
#include "Input/LOAClickMoveComponent.h"
#include "Core/LOAGameplayTags.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "AbilitySystem/Effects/LOAGE_ManaCost.h"
#include "AbilitySystem/Effects/LOAGE_IdentityGain.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "AbilitySystem/Tasks/LOAAbilityTask_WaitCursorTarget.h"
#include "Animation/LOAAnimNotify_SendGameplayEvent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPlayerAbility)

ULOAPlayerAbility::ULOAPlayerAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 누른 사람 컴퓨터에서 먼저 실행 → 서버가 확인
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 다른 스킬이 후딜 전(State.Block.Ability)이면 발동하지 않는다.
	// 회피처럼 잠금을 무시해야 하는 Ability는 자기 생성자에서 이 태그를 뺀다. (ULOAGA_Dodge)
	ActivationBlockedTags.AddTag(LOAGameplayTags::State_Block_Ability);

	// 띄움·다운 같은 피격 이상 중에는 어떤 플레이어 Ability도 발동하지 않는다. (회피 포함. 기상기만 자기 생성자에서 뺀다)
	// 태그가 GE로 붙어 B 컴퓨터에도 복제되므로 예측 발동도 여기서 막힌다.
	ActivationBlockedTags.AddTag(LOAGameplayTags::State_CrowdControl_Push);

	// 경직 중에도 아무것도 못 한다 (회피 포함, 2026-10-21 사용자 결정)
	// 주의: BP에서 Activation Blocked Tags 칸을 고친 적 있는 GA(예: GA_Dodge)는 이 기본값을 따라오지 않는다 → BP에서 직접 추가
	ActivationBlockedTags.AddTag(LOAGameplayTags::State_CrowdControl_Paralysis);

	// 상태 이상(스턴 등) 중에도 아무것도 못 한다 (회피·기상기 포함, 2026-11-10 사용자 결정: 기상기로 스턴을 못 푼다)
	// 부모 태그라 State.CrowdControl.Status.* 전부 막힌다. 위와 같은 주의: BP에서 칸을 고친 GA는 직접 추가
	ActivationBlockedTags.AddTag(LOAGameplayTags::State_CrowdControl_Status);
}

// =============================================================================
// 도우미
// =============================================================================

ALOACharacter* ULOAPlayerAbility::GetLOACharacterFromActorInfo() const
{
	// Avatar = 몸(캐릭터). ASC는 PlayerState에 있지만 Ability가 움직이는 대상은 캐릭터다.
	return Cast<ALOACharacter>(GetAvatarActorFromActorInfo());
}

ALOAPlayerController* ULOAPlayerAbility::GetLOAPlayerControllerFromActorInfo() const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Cast<ALOAPlayerController>(Info->PlayerController.Get()) : nullptr;
}

// =============================================================================
// 발동 흐름
// =============================================================================

void ULOAPlayerAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// ① 쿨타임 시작 + 마나 차감. 실패하면(쿨타임 중, 마나 부족) 취소
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bHasCursorLocation = false;

	// ② 방향이 없는 Ability는 바로 시작
	if (!bFaceCursor)
	{
		BeginSkill();
		return;
	}

	// ③ 커서 위치를 서버와 공유 → 받으면(양쪽 모두) 회전 → BeginSkill
	ULOAAbilityTask_WaitCursorTarget* CursorTask = ULOAAbilityTask_WaitCursorTarget::WaitCursorTarget(this, CursorTraceChannel);
	CursorTask->ValidData.AddDynamic(this, &ThisClass::OnCursorTargetReady);
	CursorTask->Failed.AddDynamic(this, &ThisClass::OnCursorTargetFailed);
	CursorTask->ReadyForActivation();   // Task 실행 시작 (이 줄이 없으면 Task가 동작하지 않음)
}

void ULOAPlayerAbility::OnCursorTargetReady(const FGameplayAbilityTargetDataHandle& DataHandle)
{
	// 조종하는 사람 컴퓨터와 서버 양쪽에서 같은 위치로 호출된다.
	bHasCursorLocation = ULOAAbilityTask_WaitCursorTarget::GetLocationFromData(DataHandle, CursorLocation);
	if (bHasCursorLocation)
	{
		FaceLocation(CursorLocation);
	}

	BeginSkill();
}

void ULOAPlayerAbility::OnCursorTargetFailed(const FGameplayAbilityTargetDataHandle& DataHandle)
{
	// 커서 위치를 전혀 못 구함 (맵 밖은 가상 바닥 평면으로 처리되므로 거의 없음) → 지금 보는 방향 그대로
	BeginSkill();
}

void ULOAPlayerAbility::BeginSkill()
{
	// ① 모듈 먼저. 모듈의 이벤트 대기(예: Apply의 WaitGameplayEvent)가 몽타주보다 먼저 등록돼야
	//    몽타주 첫 프레임 근처의 노티파이도 놓치지 않는다.
	ActivateSkillModules();

	// ② 모듈 시작 중에 Ability가 끝났으면(드묾: 설정 오류 등) 몽타주를 틀지 않는다.
	if (!IsActive())
	{
		return;
	}

	// ③ 실제 스킬 동작 (자식이 덮어쓰는 곳)
	StartSkill();
}

void ULOAPlayerAbility::StartSkill()
{
	// 기본: 몽타주 재생 + 끝나면 종료. 몽타주가 없으면 할 일이 없으므로 바로 정상 종료
	if (!PlayMontageAndEnd())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

bool ULOAPlayerAbility::GetCursorLocation(FVector& OutLocation) const
{
	if (!bHasCursorLocation)
	{
		return false;
	}

	OutLocation = CursorLocation;
	return true;
}

// =============================================================================
// 공통 동작
// =============================================================================

void ULOAPlayerAbility::PreActivate(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);

	bInRecovery = false;

	// 지금 가던 길 지우기 (조종하는 사람 컴퓨터에서만 의미 있음). 발동할 때만 한다.
	if (bBlockMove)
	{
		StopClickMove(ActorInfo);
	}

	// 이동/행동 금지 + 후딜 시작 대기
	ApplyLock(ActorInfo);

	// 사용형 아이덴티티 수급 노티파이 대기 (서버)
	ListenIdentityGainEvent(Handle, ActorInfo);
}

void ULOAPlayerAbility::RestartLock()
{
	// ① 후딜 상태 끝내기: 이동 캔슬 구독, 후딜 대기, 남은 태그를 모두 정리
	StopListening();
	RemoveBlockTags();
	bInRecovery = false;

	// ② 발동 때와 같은 규칙으로 다시 잠근다.
	ApplyLock(GetCurrentActorInfo());
}

void ULOAPlayerAbility::ApplyLock(const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}

	// ---------- 이동 금지 ----------
	// 후딜 시작(또는 종료)까지 이동 금지 태그
	// 서버와 조종하는 사람 컴퓨터가 각자 자기 ASC에 붙인다. (복제하지 않는 로컬 태그)
	if (bBlockMove)
	{
		ASC->AddLooseGameplayTag(LOAGameplayTags::State_Block_Move);
		bBlockMoveTagAdded = true;
	}

	// ---------- 행동 금지 ----------
	// 조종하는 사람 컴퓨터에서만 붙인다. 서버는 붙이지 않으므로 서버는 이 잠금으로 발동을 거부하지 않는다.
	// (서버 몽타주가 조금 늦어서 생기는 평캔 거부를 막기 위함. 헤더 설명 참고)
	if (bBlockAbility && ActorInfo->IsLocallyControlled())
	{
		ASC->AddLooseGameplayTag(LOAGameplayTags::State_Block_Ability);
		bBlockAbilityTagAdded = true;
	}

	// ---------- 후딜 시작 대기 ----------
	// 잠근 게 있을 때만 기다린다. (잠근 게 없으면 풀 것도 없음)
	// Task 대신 ASC의 GameplayEvent 알림에 직접 등록한다: 발동 때는 PreActivate(ActivateAbility보다 먼저)에서 불리므로
	// 자식이 ActivateAbility를 어떻게 짜든 항상 동작한다.
	if (bBlockMove || bBlockAbility)
	{
		UnlockEventHandle = ASC->GenericGameplayEventCallbacks
			.FindOrAdd(LOAGameplayTags::Event_Montage_Unlock)
			.AddUObject(this, &ThisClass::OnUnlockEvent);
	}
}

void ULOAPlayerAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 정상 종료, 취소 모두 여기를 지난다. 두 번 불려도 안전하게(붙인 것만 떼고, 등록한 것만 해제) 정리한다.
	StopListening();
	StopListeningIdentityGain();
	RemoveBlockTags();
	bInRecovery = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULOAPlayerAbility::StopClickMove(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// 클릭 이동 경로는 조종하는 사람 컴퓨터에만 있다.
	// 서버에서 원격 플레이어의 Ability가 실행될 때는 멈출 경로가 없으므로 건너뛴다.
	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	const ALOAPlayerController* PC = Cast<ALOAPlayerController>(ActorInfo->PlayerController.Get());
	if (ULOAClickMoveComponent* ClickMove = PC ? PC->GetClickMoveComponent() : nullptr)
	{
		ClickMove->StopMove();
	}
}

// =============================================================================
// 마나 소모
// =============================================================================
// GAS 기본 동작은 CostGameplayEffectClass(스킬마다 GE 하나)를 쓴다.
// LOA는 공용 GE 하나 + Ability의 값(ManaCost)으로 대신한다. → 엔진의 코스트 GE 칸은 쓰지 않는다.
//
// 두 함수 모두 const: Ability 인스턴스가 없을 때 CDO에서도 불릴 수 있으므로 멤버 변수를 바꾸지 않는다.

bool ULOAPlayerAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// 소모량 꺼내기. 지금은 Ability 레벨(=1) 기준. (캐릭터 레벨 연동은 레벨업 작업 때)
	const float Cost = ManaCost.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo));
	if (Cost <= 0.f)
	{
		return true;	// 마나를 안 쓰는 Ability (평타 등)
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return false;
	}

	// 현재 마나와 직접 비교. (기본 CheckCost는 SetByCaller 값 없이 Spec을 만들어서 공용 GE와 맞지 않는다)
	bool bFound = false;
	const float CurrentMana = ASC->GetGameplayAttributeValue(ULOAAttributeSet_Player::GetManaAttribute(), bFound);
	if (!bFound || CurrentMana < Cost)
	{
		// 실패 이유 태그. 나중에 UI가 "마나가 부족합니다"를 띄울 때 쓴다.
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailCostTag);
		}
		return false;
	}

	return true;
}

void ULOAPlayerAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const float Level = GetAbilityLevel(Handle, ActorInfo);
	const float Cost = ManaCost.GetValueAtLevel(Level);
	if (Cost <= 0.f)
	{
		return;
	}

	// ① 공용 마나 GE로 Spec 만들기
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, ULOAGE_ManaCost::StaticClass(), Level);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// ② 값 넣기. GE는 Mana에 "더하므로" 음수로 넣어야 마나가 준다. (데미지와 같은 방식)
	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_ManaCost, -Cost);

	// ③ 나에게 적용. 조종하는 사람 컴퓨터에서는 예측으로 먼저 깎이고, 서버가 거부하면 되돌아간다.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

// =============================================================================
// 아이덴티티 게이지 수급
// =============================================================================

void ULOAPlayerAbility::NotifyPayloadsApplied(bool bHitEnemy)
{
	// 적 타격형: 이 판정에 적이 있었으면 +. 아군만 맞은 판정(힐·버프)으로는 안 참
	if (bHitEnemy && IdentityGainTrigger == ELOAIdentityGainTrigger::OnEnemyHit)
	{
		GrantIdentityGain(GetCurrentActorInfo(), GetAbilityLevel());
	}
}

void ULOAPlayerAbility::GrantIdentityGain(const FGameplayAbilityActorInfo* ActorInfo, float Level) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 켜져 있는 동안은 안 참 (사용자 규칙)
	if (ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Identity_Active))
	{
		return;
	}

	ULOAGE_IdentityGain::ApplyGaugeDelta(ASC, IdentityGain.GetValueAtLevel(Level));
}

void ULOAPlayerAbility::ListenIdentityGainEvent(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo)
{
	StopListeningIdentityGain();

	// 사용형 + 양이 있을 때, 서버에서만 (게이지는 서버만 바꾼다)
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (IdentityGainTrigger != ELOAIdentityGainTrigger::OnUse || !ASC || !ASC->IsOwnerActorAuthoritative()
		|| IdentityGain.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo)) <= 0.f)
	{
		return;
	}

	IdentityGainEventHandle = ASC->GenericGameplayEventCallbacks
		.FindOrAdd(LOAGameplayTags::Event_Montage_IdentityGain)
		.AddUObject(this, &ThisClass::OnIdentityGainEvent);
}

void ULOAPlayerAbility::StopListeningIdentityGain()
{
	if (!IdentityGainEventHandle.IsValid())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(LOAGameplayTags::Event_Montage_IdentityGain))
		{
			Delegate->Remove(IdentityGainEventHandle);
		}
	}
	IdentityGainEventHandle.Reset();
}

void ULOAPlayerAbility::OnIdentityGainEvent(const FGameplayEventData* Payload)
{
	// 스킬 1번에 1번만: 먼저 대기를 끊고 채운다
	StopListeningIdentityGain();

	if (IsActive())
	{
		GrantIdentityGain(GetCurrentActorInfo(), GetAbilityLevel());
	}
}

// =============================================================================
// 후딜
// =============================================================================

void ULOAPlayerAbility::OnUnlockEvent(const FGameplayEventData* Payload)
{
	EnterRecovery();
}

void ULOAPlayerAbility::EnterRecovery()
{
	// 몽타주에 후딜 시작 노티파이가 여러 개여도(또는 노티파이 + 몽타주 끝) 처음 한 번만 처리
	if (bInRecovery || !IsActive())
	{
		return;
	}
	bInRecovery = true;

	// ① 잠금 해제 → 이동, 평타, 다른 스킬 가능
	RemoveBlockTags();

	// ② 이동 금지 스킬이었다면: 후딜 중에 이동하면 남은 몽타주를 끊고 끝낸다. (이동 캔슬)
	//    클릭 이동은 조종하는 사람 컴퓨터에만 있다. 거기서 종료하면 GAS가 서버에도 종료를 전달한다.
	//    몽타주보다 오래 사는 Ability(아이덴티티)는 이동해도 끝내지 않는다 (bEndOnRecoveryMove, 2026-11-09)
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!bBlockMove || !bEndOnRecoveryMove || !ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	const ALOAPlayerController* PC = GetLOAPlayerControllerFromActorInfo();
	if (ULOAClickMoveComponent* ClickMove = PC ? PC->GetClickMoveComponent() : nullptr)
	{
		MoveStartedHandle = ClickMove->OnMoveStarted.AddUObject(this, &ThisClass::OnMoveStartedInRecovery);
		ListenedClickMove = ClickMove;
	}

	// (평캔) 다른 Ability는 이제 발동할 수 있고, 그 몽타주가 이 몽타주를 끊으면
	//        아래 OnMontageCancelled가 후딜 중임을 보고 정상 종료한다.
}

void ULOAPlayerAbility::OnMontageCancelled()
{
	// 후딜 중에 끊김 (평캔, 다른 스킬) → 할 일(타격)은 이미 끝났으므로 "정상 종료"
	if (bInRecovery)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 후딜 전에 끊김 (기절, 회피기 등) → 부모 처리: 취소 종료
	Super::OnMontageCancelled();
}

void ULOAPlayerAbility::OnMoveStartedInRecovery()
{
	// 후딜 중 이동 → 할 일은 이미 끝났으므로 "취소"가 아니라 "정상 종료"
	// 종료되면 몽타주 Task가 몽타주를 멈춘다. (bStopWhenAbilityEnds)
	// bReplicateEndAbility = true : 서버에도 종료를 알려서 서버 몽타주도 멈추게 한다.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULOAPlayerAbility::RemoveBlockTags()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	// 붙인 적이 있을 때만 뗀다. → 후딜 시작 때 한 번, 종료 때 또 불려도 한 번만 뗌
	// (태그는 개수를 세므로, 기절 같은 다른 이유로 붙은 같은 태그는 남는다)
	if (bBlockMoveTagAdded)
	{
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(LOAGameplayTags::State_Block_Move);
		}
		bBlockMoveTagAdded = false;
	}

	if (bBlockAbilityTagAdded)
	{
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(LOAGameplayTags::State_Block_Ability);
		}
		bBlockAbilityTagAdded = false;
	}
}

void ULOAPlayerAbility::StopListening()
{
	// 후딜 이벤트 대기 해제
	if (UnlockEventHandle.IsValid())
	{
		const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(LOAGameplayTags::Event_Montage_Unlock))
			{
				Delegate->Remove(UnlockEventHandle);
			}
		}
		UnlockEventHandle.Reset();
	}

	// 이동 시작 알림 구독 해제
	if (MoveStartedHandle.IsValid())
	{
		if (ULOAClickMoveComponent* ClickMove = ListenedClickMove.Get())
		{
			ClickMove->OnMoveStarted.Remove(MoveStartedHandle);
		}
		MoveStartedHandle.Reset();
		ListenedClickMove.Reset();
	}
}

// =============================================================================
// 툴팁 숫자 (2026-11-14)
// =============================================================================

bool ULOAPlayerAbility::FindDamageRate(float Level, float& OutDamageRate) const
{
	// 처음 만난 DamageRate 하나만 (한 번 맞을 때의 계수)
	return FindFirstSetByCallerValue(LOAGameplayTags::SetByCaller_DamageRate, Level, OutDamageRate);
}

bool ULOAPlayerAbility::HasStaggerPayload() const
{
	float StaggerValue = 0.f;
	return FindFirstSetByCallerValue(LOAGameplayTags::SetByCaller_Stagger, 1.f, StaggerValue);
}

bool ULOAPlayerAbility::FindFirstSetByCallerValue(const FGameplayTag& ValueTag, float Level, float& OutValue) const
{
	// 모듈 → Payload → 값 순서. 처음 만난 하나
	for (const ULOASkillModule* Module : Modules)
	{
		const TArray<FLOAEffectPayload>* Payloads = Module ? Module->GetPayloads() : nullptr;
		if (!Payloads)
		{
			continue;
		}
		for (const FLOAEffectPayload& Payload : *Payloads)
		{
			for (const FLOASetByCallerValue& Value : Payload.Values)
			{
				if (Value.Tag == ValueTag)
				{
					OutValue = Value.Value.GetValueAtLevel(Level);
					return true;
				}
			}
		}
	}
	return false;
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOAPlayerAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 사용형인데 노티파이가 없으면 게이지가 절대 안 찬다. 몽타주가 직업 표에서 오면(Montage 비어 있음) 여기서는 모름
	if (IdentityGainTrigger == ELOAIdentityGainTrigger::OnUse && IdentityGain.GetValueAtLevel(1.f) > 0.f && Montage)
	{
		float EventTime = 0.f;
		if (!ULOAAnimNotify_SendGameplayEvent::FindEventTimeInMontage(Montage, LOAGameplayTags::Event_Montage_IdentityGain, 0.f, EventTime))
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s : Identity Gain Trigger is On Use but montage %s has no LOA Send Gameplay Event notify with Event.Montage.IdentityGain. Gauge will never fill."),
				*GetNameSafe(GetClass()), *GetNameSafe(Montage))));
		}
	}

	// 무력화 등급(표시용)과 무력화 Payload(실제 양)가 어긋나면 경고 (2026-11-18 apply121)
	//   예) 등급 = 상인데 Payload에 LOAGE_Stagger가 없음 → 툴팁은 "무력화 : 상"인데 게이지는 안 깎임
	const bool bHasStaggerGrade = SkillDisplayInfo.StaggerGrade != ELOAStaggerGrade::None;
	const bool bHasStaggerPayload = HasStaggerPayload();
	if (bHasStaggerGrade && !bHasStaggerPayload)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Stagger Grade is set but no payload has SetByCaller.Stagger. Tooltip shows stagger but the boss gauge will not drop."),
			*GetNameSafe(GetClass()))));
	}
	else if (!bHasStaggerGrade && bHasStaggerPayload)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : A payload has SetByCaller.Stagger but Stagger Grade is None. Tooltip will not show stagger."),
			*GetNameSafe(GetClass()))));
	}

	return Result;
}
#endif
