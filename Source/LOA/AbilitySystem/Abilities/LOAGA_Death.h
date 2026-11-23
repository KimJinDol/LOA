// LOAGA_Death.h
// [배관] 사망. 체력이 처음 0이 되면 자동으로 발동한다. 플레이어와 보스가 같이 쓴다. (키로 발동하지 않음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (전부 서버)
//
//   데미지 → Common 스탯창: 체력 300 → 0 "처음 0이 됨" → 자기 ASC에 Event.Trigger.Death
//     → 이 Ability 발동 (Trigger = Event.Trigger.Death)
//          ① 다른 Ability 모두 취소              (패턴·스킬 몽타주가 멈춘다. AbilityTagsToKeep에 해당하면 남김)
//          ①-1 버프·디버프 지우기               (State.Buff.* / State.Debuff.* 를 주는 GE 전부, 보호막 포함. 2026-11-07)
//          ② State.Body.Dead 태그 부여               (ULOAGE_InfiniteTags. 모든 컴퓨터에 복제)
//               → 각 컴퓨터: AnimBP가 죽음 상태로 (ULOAAnimInstance), 몸 반응 부품이 캡슐 충돌 끄기·이동 멈춤
//               → 서버: 아이덴티티 GA가 게이지를 0으로 (ULOAGA_Identity::OnDeadTagChanged. 이 GA는 아이덴티티를 모른다)
//          ③ 스킬 모듈 시작                      (선택. 예: 죽을 때 폭발 = Apply 모듈)
//          ④ 사망 몽타주 재생 → 끝나면 Ability 종료 (몽타주가 비어 있으면 바로 종료)
//
// - ②를 ④보다 먼저 하는 이유: 태그가 먼저 붙어야 몽타주 "밑에서" AnimBP가 이미 죽음 상태로 가 있다.
//   그래서 몽타주가 끝나며 빠질 때 서 있는 자세로 튀지 않고 그대로 시체 포즈가 된다.
// - 몽타주 없이 AnimBP 죽음 상태가 사망 애니를 직접 재생해도 된다. (그때는 Montage 칸을 비운다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [수명 - Ability는 끝나도 State.Dead는 남는다]
//   이 Ability는 몽타주가 끝나면 종료한다. 시체 포즈는 AnimBP가 태그를 보고 유지하므로 붙잡고 있을 게 없다.
//   State.Body.Dead GE는 종료해도 지우지 않는다. 지우는 쪽 = "부활" (아직 없음. 보스는 부활이 없으므로 영구)
//   → 부활을 만들 때: State.Dead를 주는 GE를 지운다. (RemoveActiveEffectsWithGrantedTags)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - ServerOnly: 서버만 실행한다. 클라이언트는 결과만 본다.
//     State.Body.Dead → ASC 태그 복제 (모든 컴퓨터) / 몽타주 → ASC 몽타주 복제 (모든 컴퓨터)
// - 결과가 남는 일(태그 GE, 모듈의 데미지)은 서버 일이므로 예측할 필요가 없다.
// - 늦게 들어온 플레이어도 태그가 이미 붙은 상태로 보스를 받으므로 시체 포즈·충돌 꺼짐이 바로 맞는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [발동 차단]
// 모든 LOA Ability는 State.Dead가 있으면 발동하지 않는다. (ULOAGameplayAbility 생성자)
// 이 Ability만 생성자에서 그 차단을 뺀다. (부활 후 다시 죽는 경우를 위해. 지금은 태그가 붙기 전에 발동하므로 영향 없음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_Death_<주인> (부모: LOAGA_Death)  예: GA_Death_Grux
//   LOA|Ability|Montage : Montage = 사망 몽타주 (선택), Montage Play Rate
//   LOA|Ability|Death   : Additional Dead Tags (선택), Ability Tags To Keep (선택)
//   꾸러미 등록         : 보스 → DA_AbilityDataSet_Boss_<이름>의 Abilities에 추가 (InputTag 비움)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "LOAGA_Death.generated.h"

UCLASS()
class LOA_API ULOAGA_Death : public ULOAGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Death(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface

	// ---------- 데이터 (각 GA_Death BP의 Class Defaults에서 조정) ----------

	/**
	 * State.Dead와 함께 죽어 있는 동안 붙일 태그. (State.Dead는 코드가 항상 붙이므로 여기 넣지 않는다)
	 * 비워 두는 것이 보통이다. 예) 시체에게 걸리면 안 되는 효과를 막는 면역 태그
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Death")
	FGameplayTagContainer AdditionalDeadTags;

	/**
	 * 죽을 때 취소하지 않을 Ability. 이 태그 중 하나라도 Asset Tags에 가진 Ability는 그대로 둔다.
	 * 비워 두면 이 Ability를 뺀 전부를 취소한다. (보통)
	 * 예) 죽어도 계속 돌아야 하는 패시브
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Death")
	FGameplayTagContainer AbilityTagsToKeep;

private:
	/** [서버] State.Buff.* / State.Debuff.* 를 주는 GE를 전부 지운다 (보호막 GE 포함 → Shield도 0). (2026-11-07) */
	void ClearBuffsOnDeath() const;
};
