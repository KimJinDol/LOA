// LOAGA_BossBrawl.h
// [배관] 대난투 그로기. 보스 무력화가 0이 되면 켜지고, 대난투가 끝나 원래 자리로 돌아오면 꺼진다. (2026-11-18 apply122)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] 서버만 (보스는 주인 컴퓨터가 없다)
//   보스 스탯창(ULOAAttributeSet_Boss) 무력화 처음 0 → Event.Trigger.StaggerBroken → 이 Ability 발동
//     ① 하던 패턴 취소 (AbilityTagsToKeep은 남김)
//     ② 그 자리에 선다 (부모 StopAvatarMovement)
//     ③ 그로기 몽타주 (반복 섹션, 부모 PlayMontageOnly. 비우면 몽타주 없이)
//     ④ 대난투 경기장(ALOABrawlArena)에 StartBrawl → 슬로우·차원 이동·진행·복귀는 경기장이 한다
//   경기장 OnStageAdvanced (다음 단계, 남은 시간 멈춤. 2026-11-18 apply125 → apply128 패턴으로)
//     → 협동 저가 패턴 발동 (Coop Just Guard Pattern Tag, ULOAMonsterAbility::TryActivatePattern. 대상 없음 = 보는 쪽 그대로)
//        패턴의 공격 모듈이 판정·피해·CC (실패하면 맞는다, 에어본 포함. 사용자 11-18), Just Guard 모듈이 집계
//     → Just Guard 모듈이 패턴 끝에 Event.JustGuard.PatternResolved (EventMagnitude = 성공 인원) → 받아 둔다
//     → 보스 ASC OnAbilityEnded (그 패턴이 끝남. 엔진 순서상 패턴의 태그가 빠지고 몽타주 작업이 끝난 뒤)
//        → 경기장 FinishCoopJustGuard(성공 인원) → 등급·추가 시간·다시 흐름 + 그로기 몽타주 다시
//     태그가 비었거나 발동이 거부되면 → 경고 + 바로 ResumeBrawlTimer (추가 시간 없음. 시간이 멈춘 채 남지 않게)
//     예) 2단계 진입 → Ability.Boss.JustGuard 발동 → 3연타 중 2명이 전부 막음 → 패턴 끝 → Great +3초
//   패턴 중에는 단계가 넘어가지 않는다 (단계 게이지가 멈춰 있음, apply127) → 패턴은 한 번에 하나
//   경기장 OnBrawlEnded (원래 자리로 다 돌아옴) → 끝 → 무력화를 다시 채운다 (ALOABossCharacter::RefillStagger, 사용자: "다 깔 때마다 가능")
//   켜져 있는 동안 State.Body.Groggy → 보스 StateTree "대난투" 상태 (아무것도 안 함) → 다른 패턴은 나가지 않는다
//     State.Block.Ability는 붙이지 않는다 (apply128): 붙이면 협동 저가 패턴도 막힌다 (모든 패턴의 Activation Blocked Tags)
//     대신 치트(LOA.Boss.Pattern)로는 대난투 중에도 패턴이 나간다
//
//   예) 서버 시각 100.0 무력화 0 → 발동 → 경기장 들어가는 중 … 117.5 원래 자리 → 끝 → 무력화 100
//
// [예외]
//   경기장이 레벨에 없거나 시작을 거부하면 → 바로 끝 + 무력화를 다시 채움 (경고 로그). 보스가 멈춰 서지 않게
//   이 Ability가 대난투 중에 끊기면 (사망 등) → 경기장에 EndBrawl을 요청해 원래 자리로 돌아가게 한다
//   협동 저가 패턴 중에 이 Ability가 끝나면 (누적 ≥ 체력으로 대난투 끝 → 돌아옴) → 그 패턴을 취소한다
//
// [하지 않는 일]
//   순간이동·연출·시간은 모른다 (경기장). 무력화 깎기·0 판단은 모른다 (보스 스탯창)
//
// [네트워크] 서버만. 결과(태그, 몽타주, 자리)는 각각의 복제로 간다
//
// [BP에서 설정할 것] GA_Boss_Brawl (부모 LOAGA_BossBrawl) → 보스 꾸러미(DA_ADS_Boss) Granted Abilities에 (InputTag 없음)
//   LOA|Ability|Montage : 그로기 몽타주 (반복 섹션). 지금은 스턴 몽타주를 써도 된다
//   LOA|Ability|Brawl   : Coop Just Guard Pattern Tag = 단계가 넘어갈 때 쓸 저가 패턴 (apply128. 지금 Ability.Boss.JustGuard)
//                         그 패턴은 보스 꾸러미에 있어야 하고 Modules에 Just Guard 모듈이 있어야 한다 (없으면 성공 0명 = Bad)
//   보스 StateTree      : "대난투" 상태 = LOA Has Tag(State.Body.Groggy, 있으면 참)로 들어가고 (없으면 참)으로 나옴. 스턴 상태 위에 (사용자가 에디터에서)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "LOAGA_BossBrawl.generated.h"

class ALOABrawlArena;
struct FAbilityEndedData;

UCLASS()
class LOA_API ULOAGA_BossBrawl : public ULOAGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAGA_BossBrawl(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/** 그로기가 되어도 취소하지 않을 Ability (Asset Tags 중 하나라도 가지면 남김). 기본: Ability.Trait.KeepOnCC (스턴과 같음) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Brawl")
	FGameplayTagContainer AbilityTagsToKeep;

	/**
	 * 대난투 단계가 넘어갈 때 쓸 협동 저가 패턴 (그 패턴 GA의 Asset Tags 중 하나, apply128). 끝날 때까지 남은 시간이 멈춘다
	 * 비우면 패턴 없이 바로 다시 흐른다 (경고). 예) Ability.Boss.JustGuard
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Brawl", meta = (Categories = "Ability"))
	FGameplayTag CoopJustGuardPatternTag;

private:
	/** 경기장: 원래 자리로 다 돌아옴 → 끝 */
	void HandleBrawlEnded(ALOABrawlArena* Arena);

	/** 경기장: 다음 단계 (남은 시간 멈춤) → 협동 저가 패턴 발동. 못 쓰면 바로 ResumeBrawlTimer */
	void HandleStageAdvanced(ALOABrawlArena* Arena, int32 NewStage);

	/** Just Guard 모듈: 패턴 끝, 성공 인원 (이벤트 대기 작업 알림이라 UFUNCTION) → 받아 두기만 */
	UFUNCTION()
	void HandleJustGuardResolved(FGameplayEventData Payload);

	/** 보스 ASC: 어떤 Ability가 끝남 → 기다리던 협동 저가 패턴이면 경기장 FinishCoopJustGuard + 그로기 몽타주 다시 */
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	/** 이 Ability가 협동 저가 패턴인가 (Asset Tags에 Coop Just Guard Pattern Tag) */
	bool IsCoopJustGuardPattern(const UGameplayAbility* Ability) const;

	FDelegateHandle StageAdvancedHandle;

	/** 보스 ASC OnAbilityEnded 구독 (붙인 ASC를 같이 들고 있다) */
	TWeakObjectPtr<UAbilitySystemComponent> ListenedASC;
	FDelegateHandle AbilityEndedHandle;

	/** 협동 저가 패턴이 도는 중 (발동 ~ 끝) */
	bool bWaitingCoopJustGuard = false;

	/** 이번 협동 저가 패턴의 성공 인원 (Just Guard 모듈 알림. 못 받으면 0) */
	int32 CoopJustGuardSuccessCount = 0;

	/** 시작한 경기장 (끝 알림 해제, 끊길 때 EndBrawl 요청) */
	TWeakObjectPtr<ALOABrawlArena> ActiveArena;

	FDelegateHandle BrawlEndedHandle;
};
