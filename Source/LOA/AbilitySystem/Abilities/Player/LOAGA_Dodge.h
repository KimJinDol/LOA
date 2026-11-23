// LOAGA_Dodge.h
// [배관] 회피 (스페이스). 커서 방향으로 돌아서 정해진 거리를 빠르게 이동한다. 모든 직업이 같이 쓴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [로아로 치면]
// 스페이스바 이동기. 스킬을 쓰는 도중에도 언제든 눌러서 빠져나갈 수 있고 (저스트 가드 중만 예외: 못 씀),
// 회피 모션 동안 피격 이상·상태 이상에 면역이다. (데미지는 받는다. 후딜에서 끊으면 면역도 그 순간 해제)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   스페이스 → 발동
//      ├─ (발동 직전, GAS 자동) Cancel Abilities with Tag(BP: Ability)에 해당하는 실행 중 스킬 취소
//      │     → 쓰던 스킬의 몽타주가 끊기고, 그 스킬이 붙였던 이동/행동 금지도 풀린다.
//      ├─ (부모 ULOAPlayerAbility) 쿨타임 시작(Cooldown.Common.Dodge) → 커서 쪽으로 회전
//      └─ StartSkill (이 클래스)
//            ├─ 몽타주 재생 + 끝나면 종료 (다른 스킬과 같은 PlayMontageAndEnd)
//            └─ [ApplyRootMotionConstantForce] DodgeDuration 동안 DodgeDistance만큼 밀기
//            몽타주의 Event.Montage.Unlock 노티파이부터 후딜 → 이동/스킬로 남은 동작 캔슬 가능
//
//   [회피 몽타주 구간]
//   |──── 대시 (DodgeDuration) ────|──── 착지/자세 회복 ────|
//   면역 ─────────────────────────────────────────────────▶ (Ability가 끝날 때까지)
//   이동/스킬 잠금                 ↑ Unlock 노티파이 (여기부터 후딜: 이동하거나 스킬 쓰면 끊기고 면역도 해제)
//
//   - 몽타주는 DodgeDuration보다 길어야 한다. 짧으면 몽타주가 끝날 때 Ability가 끝나서 이동도 도중에 멈춘다.
//   - Unlock 노티파이가 없으면 몽타주 끝까지 잠금. (다른 스킬과 같은 규칙)
//   - 몽타주가 비어 있으면 이동이 끝날 때 종료.
//
// ─────────────────────────────────────────────────────────────────────────────
// [스킬 중에도 발동되는 이유]
// 모든 플레이어 Ability는 State.Block.Ability가 있으면 발동하지 않는다. (ULOAPlayerAbility 생성자)
// 회피만 생성자에서 이 조건을 뺀다. → 스킬 시전 중(후딜 전)에도 발동된다.
// 회피 자체는 bBlockAbility = true라서, 회피 중에는 다른 스킬이 나가지 않는다.
//
// [면역 - Activation Owned Tags]
// GAS 기본 기능: Ability가 실행 중인 동안 이 태그들을 ASC에 붙여 두고, 끝나면 GAS가 뗀다.
// 생성자에서 State.Immune.Push / State.Immune.Status를 기본값으로 넣는다. (BP에서 바꿀 수 있음)
// 로아 회피 규칙과 같다: 끊지 않으면 회피 모션 내내 면역, 후딜에서 이동/스킬로 끊으면 그 순간 해제.
//   (Ability 종료 시점 = 몽타주 끝 또는 후딜 캔슬 → 기존 후딜 로직이 그대로 처리)
// 서버에서도 Ability가 실행되므로 서버 ASC에 태그가 붙는다. (상태 이상은 서버가 걸기 때문에 이것으로 충분)
// 아직 피격 반응·상태 이상 시스템이 없으므로 지금은 "붙기만" 한다. 그 시스템이 생기면 이 태그를 확인해서 거른다.
//
// [이동 - Root Motion Force]
// 애니메이션의 루트 모션이 아니라 GAS의 "힘으로 밀기" Task를 쓴다.
//   - 거리/시간을 데이터로 조절. 애니메이션과 상관없이 같은 거리를 간다.
//   - CharacterMovement가 예측/보정을 지원한다. (조종하는 사람 컴퓨터에서 먼저 움직이고 서버가 확인)
//   - 속도 = DodgeDistance / DodgeDuration. 끝나면 속도 0으로 멈춘다. (미끄러지지 않음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_Dodge (부모: LOAGA_Dodge)
//   Montage                     = 회피 몽타주 (DefaultSlot)
//                                   - 대시 끝: LOA Send Gameplay Event (Event.Montage.Unlock, Branching Point)
//   Cooldown Duration / Tags    = 예: 8 / Cooldown.Common.Dodge
//   Asset Tags                  = Ability.Common.Dodge    (생성자 기본값. 저스트 가드가 이 태그로 회피를 막는다)
//   Cancel Abilities with Tag   = Ability          ← 이게 있어야 쓰던 스킬이 끊긴다
//   Dodge Distance / Duration   = 예: 500 / 0.3

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "LOAGA_Dodge.generated.h"

UCLASS(Abstract)
class LOA_API ULOAGA_Dodge : public ULOAPlayerAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Dodge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ Begin ULOAPlayerAbility Interface
	/** 커서 회전이 끝난 뒤: 몽타주 + 밀기 이동 */
	virtual void StartSkill() override;
	//~ End ULOAPlayerAbility Interface

	// ---------- 데이터 (GA_Dodge BP의 Class Defaults에서 조정) ----------

	/** 이동 거리 (cm). 로아 감각으로 대략 캐릭터 몇 걸음 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Dodge", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float DodgeDistance = 500.f;

	/** 이동에 걸리는 시간 (초). 짧을수록 빠르게 튀어 나간다. 면역도 이 시간 동안 유지된다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Dodge", meta = (ClampMin = "0.05", Units = "Seconds"))
	float DodgeDuration = 0.3f;

private:
	/** 밀기가 끝났다. 몽타주가 없으면 여기서 종료 (몽타주가 있으면 몽타주가 끝낼 때 종료) */
	UFUNCTION()
	void OnDodgeMoveFinished();
};