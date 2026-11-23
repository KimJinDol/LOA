// LOAGA_Stun.h
// [배관] 스턴. 스턴 태그(State.CrowdControl.Status.Stunned)가 붙어 있는 동안 켜져 있다. 보스와 플레이어가 같이 쓴다. (키로 발동하지 않음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (서버 기준. 플레이어 컴퓨터가 하는 일은 아래 [네트워크])  2026-11-10: 이벤트 → "GE → 태그 → GA" (띄움·경직과 같은 방식)
//
//   스턴 GE(ULOAGE_Stun, 예: GE_CC_Stun)가 붙음 → State.CrowdControl.Status.Stunned
//     예) 보스 Roar의 작은 장판 Payload = [ 데미지, GE_CC_Stun { SetByCaller.Duration 2 } ] → 맞은 플레이어 2초 스턴
//     예) 카운터 성공 → 카운터 모듈이 보스에게 GE_CC_Stun (Stun Duration)
//     (상태 이상 면역·띄움 중이면 GE의 Ignore 조건에 걸려 여기까지 오지 않는다 → 아무 반응 없음)
//   → 이 Ability 발동 (Trigger = Owned Tag Present: 태그가 있는 동안만. 태그가 사라지면 엔진이 취소)
//          ① 다른 Ability 모두 취소        (하던 스킬·패턴이 끊긴다. AbilityTagsToKeep은 남김: 아이덴티티)
//          ② 그 자리에 선다                 (걷던 중이면 그 자리에)
//          ③ 스턴 몽타주 재생 (있으면)      보스: M_Boss_Stun / 플레이어: 비움 → AnimBP가 태그로 스턴 자세 (아래 [모션])
//   GE 시간이 끝나 태그가 사라짐 → 엔진이 이 Ability 취소 → 몽타주가 빠진다
//
// [모션] (2026-11-10 사용자 결정: 플레이어는 사망처럼 AnimBP)
//   플레이어: AnimBP 변수 bIsStunned (ULOAAnimInstance의 Tag Property Map: State.CrowdControl.Status.Stunned) → Stunned 상태
//     태그를 따르므로 이 GA가 띄움·경직에 끊겨도, 그 몽타주가 끝나면 남은 시간 동안 다시 스턴 자세
//   보스: 지금 몽타주 그대로 (직접 조종하는 캐릭터가 아니라 복제 몽타주 문제가 없음)
//
// - 스턴 시간 = GE의 시간 (Payload의 SetByCaller.Duration, 없으면 GE의 Default Duration). 이 GA는 시간을 세지 않는다
// - 스턴 중 또 스턴: GE가 하나 더 붙는다 → 태그는 더 긴 쪽이 끝날 때까지 (몽타주는 처음부터 다시 하지 않음)
// - 스턴 중에는 아무것도 못 한다: 플레이어 Ability는 State.CrowdControl.Status에 막힘 (ULOAPlayerAbility, 회피·기상기 포함)
//   보스 패턴은 이 Ability가 켜 둔 State.Block.Ability에 막힘
// - 사망·띄움이 이 Ability를 취소해도 스턴 GE는 남는다 (사용자 결정 2026-11-10). 남은 시간 동안 플레이어는 계속 못 움직인다
//   (이 Ability는 다시 켜지지 않는다. 막는 건 태그: 스킬 = ULOAPlayerAbility, 이동 = ULOAClickMoveComponent / 자세 = AnimBP)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크] (2026-10-23: ServerInitiated. 이유는 ULOAGA_Airborne 헤더 [네트워크]와 같음:
//             엔진이 "내가 조종하는 캐릭터"의 복제 몽타주를 버려서, 플레이어가 기절하면 본인 화면에서만 모션이 안 나온다)
//   발동·끝내기는 서버만. 플레이어면 그 플레이어 컴퓨터에서도 이 GA가 돈다. 보스·몬스터는 주인 컴퓨터가 없어서 서버만.
//   1. 서버만     : 스턴 GE 적용(면역 검사), 발동, 끝내기
//   2. 복제       : 스턴 태그 (GE), 남의 화면용 스턴 몽타주 (ASC 몽타주 복제)
//   3. 각자 할 일 : 맞은 플레이어 컴퓨터의 이 GA: 다른 스킬 끊기, 이동 멈춤, 스턴 몽타주
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_<주인>_Stun (부모: LOAGA_Stun)  예: GA_Boss_Stun, GA_Stun(플레이어 공용)
//   LOA|Ability|Montage : 보스 = 스턴 몽타주 (반복 섹션) / 플레이어 = 비움 (AnimBP, 위 [모션])
//   LOA|Ability|Stun    : Ability Tags To Keep (기본: Ability.Trait.KeepOnCC)
//   꾸러미 등록         : 보스 → DA_AbilityDataSet_Boss / 플레이어 → 띄움·경직과 같은 꾸러미 (InputTag 비움)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
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

	/**
	 * 스턴될 때 취소하지 않을 Ability. 이 태그 중 하나라도 Asset Tags에 가진 Ability는 그대로 둔다.
	 * 비워 두면 이 Ability를 뺀 전부를 취소한다. (보통) 예) 스턴 중에도 계속 돌아야 하는 패시브
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Stun")
	FGameplayTagContainer AbilityTagsToKeep;

	// ② 걷던 이동 멈춤 = 부모 StopAvatarMovement, ③ 스턴 몽타주 = 부모 PlayMontageOnly (2026-11-18 부모로 옮김: 대난투 그로기 GA도 같이 씀)
};
