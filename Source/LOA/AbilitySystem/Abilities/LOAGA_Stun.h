// LOAGA_Stun.h
// [배관] 스턴. Event.Stun을 받으면 자동으로 발동한다. 보스와 (나중에) 플레이어가 같이 쓴다. (키로 발동하지 않음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (전부 서버)
//
//   예) 쌍검 베기 카운터 성공 → 패턴이 보스 ASC에 Event.Stun (EventMagnitude = 3)
//     → 이 Ability 발동 (Trigger = Event.Stun)
//          ① 다른 Ability 모두 취소        (하던 패턴이 끊긴다 → 몽타주의 카운터 구간도 같이 끝남. AbilityTagsToKeep은 남김)
//          ② State.CrowdControl.Debuff.Stunned 3초             (ULOAGE_TimedTags. 모든 컴퓨터에 복제)
//          ③ 이동 멈춤                      (걷던 중이면 그 자리에)
//          ④ 스턴 몽타주 재생               (반복 섹션이면 3초 내내 반복)
//          ⑤ 3초 뒤 종료                    → 몽타주가 빠지고 State.CrowdControl.Debuff.Stunned도 지운다
//
// - 스턴 시간은 이벤트가 가져온다. (패턴마다 다름) 이벤트에 시간이 없으면(0 이하) Default Duration.
// - 몽타주가 3초보다 짧아도 3초가 될 때까지 끝나지 않는다. 몽타주에 반복 섹션을 만들어 둔다.
//     예) 섹션 Start(쓰러짐) → Loop(멍한 자세, 자기 자신으로 연결)
// - 스턴 도중 또 스턴이 와도 이어 붙이지 않는다. (이미 발동 중이면 GAS가 새 발동을 거절)
// - 사망은 스턴보다 강하다. GA_Death가 이 Ability도 취소한다.
// - 스턴 중에는 State.Block.Ability가 붙어 패턴·스킬이 발동하지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [면역]
// 보스는 대부분의 상태 이상에 면역이지만 카운터 스턴은 걸린다. 그래서 기본값은 State.Immune.Debuff에 막히지 않는다.
// 플레이어용 스턴 BP를 만들 때 Activation Blocked Tags에 State.Immune.Debuff를 넣는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만     : 발동, 다른 Ability 취소, 이동 멈춤, 끝나는 시각
//   2. 복제       : State.CrowdControl.Debuff.Stunned 태그 (GE), 스턴 몽타주 (ASC 몽타주 복제)
//   3. 각자 할 일 : 없음 (나중에 머리 위 별, "스턴" 글씨를 붙이면 태그를 보고 각자)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_<주인>_Stun (부모: LOAGA_Stun)  예: GA_Boss_Stun
//   LOA|Ability|Montage : Montage = 스턴 몽타주 (반복 섹션 권장), Montage Play Rate
//   LOA|Ability|Stun    : Default Duration (이벤트에 시간이 없을 때), Ability Tags To Keep (선택)
//   꾸러미 등록         : 보스 → DA_AbilityDataSet_Boss의 Abilities에 추가 (InputTag 비움)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "LOAGA_Stun.generated.h"

UCLASS()
class LOA_API ULOAGA_Stun : public ULOAGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Stun(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	// ---------- 데이터 (각 스턴 BP의 Class Defaults에서 조정) ----------

	/** 이벤트에 스턴 시간이 없을 때(EventMagnitude <= 0) 쓰는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Stun", meta = (ClampMin = "0.1", Units = "Seconds"))
	float DefaultDuration = 2.f;

	/**
	 * 스턴될 때 취소하지 않을 Ability. 이 태그 중 하나라도 Asset Tags에 가진 Ability는 그대로 둔다.
	 * 비워 두면 이 Ability를 뺀 전부를 취소한다. (보통) 예) 스턴 중에도 계속 돌아야 하는 패시브
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Stun")
	FGameplayTagContainer AbilityTagsToKeep;

private:
	/** 시간이 다 됨 → 정상 종료 */
	UFUNCTION()
	void OnStunFinished();

	/** ③ 걷던 이동 멈춤 (캐릭터 이동 + AI 길 찾기) */
	void StopMovement() const;

	/** ④ 스턴 몽타주 재생. 끝나도 Ability는 끝내지 않는다 (시간이 정한다). Ability가 끝나면 몽타주도 멈춘다 */
	void PlayStunMontage();

	/** State.CrowdControl.Debuff.Stunned GE 번호표. 일찍 끝날 때(사망으로 취소) 지운다 */
	FActiveGameplayEffectHandle StunnedEffectHandle;
};
