// LOAGA_Paralysis.h
// [배관] 경직. 플레이어가 경직 태그를 받으면 자동으로 발동한다 (키로 발동하지 않음). 모든 직업 공용.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (전부 서버)  예) 보스 휘두르기, GE_CC_Paralysis { Duration 0.5, LaunchDistance 100 }
//
//   B가 맞음 → GE_CC_Paralysis → B에게 State.CrowdControl.Paralysis → 이 Ability 발동 (Trigger = 태그가 붙음)
//     (슈퍼아머·피격 이상 면역·띄움 중이면 GE의 Ignore 조건에 걸려 여기까지 오지 않는다 → 아무 반응 없음)
//     ① 하던 스킬 전부 취소, 이동 금지 (State.Block.Move GE → 복제 → B 컴퓨터의 클릭 이동이 멈춘다)
//        경직 태그 자체가 모든 플레이어 Ability의 차단 태그 → 회피·기상기·스킬 전부 안 나감 (사용자 결정)
//     ② 밀기: 때린 쪽 반대로 100cm를 0.15초(GE의 Push Duration, 고정) 동안 일정하게 → 바로 멈춤 (2026-10-21: 물리 → 정해진 궤적)
//          방향 규칙은 ULOAGE_CrowdControl (띄움과 같음). 궤적·때린 쪽 바라보기는 ULOACharacterMovementComponent::StartCrowdControlMove
//          (서버가 숫자를 정해 B 컴퓨터로 복제 → 두 쪽이 같은 궤적)
//     ③ 경직 몽타주 (있으면)
//     ④ 0.5초 뒤 종료 → EndAbility가 경직·이동 금지 GE를 지움
//   경직 중 또 경직 → 옛 경직 GE를 지우고 새 GE로 처음부터 (시간 다시, 다시 밀림, 몽타주 처음부터) (사용자 결정)
//   경직 중 띄움     → ULOAGA_Airborne이 다른 Ability를 모두 취소 → 이 Ability가 끊김 → EndAbility가 경직 GE를 지움 → 띄움으로
//
// [왜 GE 시간이 아니라 이 GA가 시간을 세나] 띄움과 같은 방식. "또 맞으면 처음부터"를 GA 한 곳에서 처리하고,
//   GE의 시간(기본 5초)은 끝 신호를 놓쳐도 태그가 남지 않게 하는 안전용이다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만     : 발동, 취소, 밀 숫자 정하기, 끝나는 시각
//   2. 복제       : 경직·이동 금지 태그 (GE), 몽타주, 밀 숫자 (이동 컴포넌트의 CrowdControlMove, B에게만)
//   3. 각자 할 일 : 궤적 걸기, 때린 쪽 바라보기 (서버와 B 컴퓨터가 같은 숫자로)
//   대가: B 화면에서 핑만큼 늦게 밀린다 (띄움과 같음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_Paralysis (부모: LOAGA_Paralysis)
//   LOA|Ability|Montage : Montage = 경직 몽타주 (짧은 피격 모션. 경직 시간보다 짧으면 끝난 자세로 남는다 → 길면 반복 섹션)
//   LOA|Paralysis       : Ability Tags To Keep (선택)
//   꾸러미 등록         : 띄움과 같은 곳 (직업의 플레이어 꾸러미 또는 공용 꾸러미), InputTag 없이
//   주의: BP에서 Activation Blocked Tags를 고친 적 있는 GA(GA_Dodge 등)는 State.CrowdControl.Paralysis를 직접 추가

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffectTypes.h"
#include "Engine/TimerHandle.h"
#include "LOAGA_Paralysis.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_PlayMontageAndWait;
struct FGameplayEffectSpec;
struct FActiveGameplayEffect;

UCLASS(Abstract)
class LOA_API ULOAGA_Paralysis : public ULOAGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Paralysis(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	/** 경직될 때 취소하지 않을 Ability (Asset Tags 기준). 비우면 이 Ability를 뺀 전부 취소. 예) 패시브 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Paralysis")
	FGameplayTagContainer AbilityTagsToKeep;

private:
	/** 이 경직 GE로 (다시) 시작: 옛 경직 GE 지우기 → 밀기 → 시간 다시 → (다시면) 몽타주 처음부터 */
	void StartParalysis(const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle EffectHandle, bool bRestart);

	/** 때린 쪽 반대로 Distance만큼, Push Duration 동안 밀리는 궤적을 건다 (서버) */
	void PushFromEffect(const FGameplayEffectSpec& Spec, float Distance);

	/** 경직 몽타주를 처음부터 튼다. 또 맞을 때도 새로 다시 튼다 (이미 끝났어도 나오게) */
	void PlayParalysisMontage();

	/** 지금 경직 몽타주를 트는 작업. 또 맞으면 끝내고 새로 */
	TWeakObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** 지금 붙어 있는 경직 GE 중 가장 늦게 붙은 것 (발동 순간 처음 쓸 GE). 없으면 nullptr */
	const FActiveGameplayEffect* FindNewestParalysisEffect() const;

	/** [알림] 내게 GE가 적용됨 → 경직 GE면 처음부터 다시 */
	void OnEffectAppliedToSelf(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle ActiveHandle);

	/** [알림] 경직 태그 개수가 바뀜 → 0이 됨 = 안전 시간이 다 됐거나 누가 지움 → 종료 */
	void OnParalysisTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 경직 시간이 끝남 → 정상 종료 */
	void OnParalysisTimeFinished();

	/** 내가 서버에서 붙인 GE를 지운다 (번호표를 비움) */
	void RemoveOwnedEffect(FActiveGameplayEffectHandle& InOutHandle) const;

	/** 지금 경직을 정하는 GE (가장 최근에 맞은 것). 같은 GE의 "적용됨" 알림이 한 번 더 오면 무시 */
	FActiveGameplayEffectHandle CurrentParalysisHandle;

	/** 이동 금지(State.Block.Move) GE 번호표. 종료 때 지운다 */
	FActiveGameplayEffectHandle BlockMoveEffectHandle;

	/** 경직 시간 타이머. 또 맞으면 다시 건다 */
	FTimerHandle ParalysisTimer;

	/** 알림 구독 번호 (종료 때 해제) */
	FDelegateHandle EffectAppliedHandle;
	FDelegateHandle ParalysisTagHandle;
};
