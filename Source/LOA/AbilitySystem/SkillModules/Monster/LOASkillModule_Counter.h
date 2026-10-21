// LOASkillModule_Counter.h
// [스킬 모듈 · 몬스터 전용] Counter — 이 패턴이 도는 동안 카운터를 맞으면 스턴에 걸린다. 판정은 맞는 쪽(보스)이 한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 카운터 패턴 GA(부모 ULOAMonsterAbility)의 Modules에 넣는다. 카운터가 아닌 패턴에는 넣지 않는다.
//   예) GA_Boss_CounterSlash
//        Montage   : 0.8~1.3초에 LOA Grant Tags(State.Counterable) 구간 = 파란 빛 (언제 카운터되나)
//        Modules   : [0] Spawn (베기 장판)  [1] Counter (Stun Duration 4, Head Half Angle 45)
//
// [흐름] (전부 서버)
//   패턴 발동 → 모듈이 Event.Combat.Hit을 기다림 (Trigger: On Event, 패턴이 도는 동안만. 부모 모듈이 처리)
//   플레이어 B가 보스를 침 → ULOACombatLibrary::ApplyPayloads가 데미지를 준 뒤 보스에게 Event.Combat.Hit
//        (누가 = B, 공격 속성 = B 스킬의 Skill.Attribute.* 태그)
//   → 이 모듈의 판정
//        ① 공격 속성에 Skill.Attribute.Counter가 있나       없으면 아무 일 없음 (일반 공격)
//        ② 지금 내게 State.Counterable이 있나 (파란 구간)   없으면 아무 일 없음 (구간 밖)
//        ③ B가 내 정면 ±Head Half Angle 안에 서 있나        아니면 로그만 (옆·뒤에서 침)
//   → 성공: 내 ASC에 Event.Combat.Counter (카운터 성공 알림, 지금은 듣는 곳 없음)
//           → Event.Stun (EventMagnitude = Stun Duration) → ULOAGA_Stun이 이 패턴을 취소하고 스턴
//   실패 = 패턴이 그대로 끝까지 간다. (별도 패널티 없음)
//
// [왜 모듈인가] (2026-10-18 사용자 결정)
//   - 판정은 맞는 쪽이 한다. 공격 쪽(전투 라이브러리)은 "누가, 어떤 속성으로 쳤다"만 알린다.
//   - 카운터 데이터(스턴 시간, 헤드 각도)가 카운터 패턴에만 있다. 패턴 부모(ULOAMonsterAbility)에 두면 모든 패턴에 칸이 보인다.
//   - "패턴이 도는 동안만"이 저절로 된다. (모듈의 이벤트 대기는 패턴이 끝나면 엔진이 정리)
//
// [네트워크]
//   1. 서버만     : 판정, Event.Combat.Counter, Event.Stun, 스턴 GA
//   2. 복제       : State.Counterable·스턴 태그, 스턴 몽타주 (원래대로)
//   3. 각자 할 일 : 없음
//
// [에디터에서]
//   Trigger / Trigger Event Tag : 기본값 그대로 (On Event / Event.Combat.Hit). 바꾸면 저장 시 경고
//   Stun Duration               : 카운터 성공 시 스턴 시간 (초)
//   Head Half Angle             : 헤드 판정 반각 (도). 45 = 정면 좌우 45°씩

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "LOASkillModule_Counter.generated.h"

UCLASS(meta = (DisplayName = "Counter"))
class LOA_API ULOASkillModule_Counter : public ULOASkillModule
{
	GENERATED_BODY()

public:
	ULOASkillModule_Counter();

#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사: Trigger가 기본값(On Event, Event.Combat.Hit)인지, 몽타주(파란 구간을 칠할 곳)가 있는지 */
	virtual EDataValidationResult ValidateSkillModule(
		class FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const override;
#endif

protected:
	/** Event.Combat.Hit이 올 때마다 (맞을 때마다) 카운터 판정. 헤더 [흐름] */
	virtual void SkillModuleTriggered(const FGameplayEventData* EventData) override;

	/** 카운터 성공 시 스턴 시간 (초). 패턴마다 다르다 */
	UPROPERTY(EditDefaultsOnly, Category = "Counter", meta = (ClampMin = "0.1", Units = "Seconds"))
	float StunDuration = 3.f;

	/** 헤드 판정 반각 (도). 때린 사람이 내 정면 ±이 각도 안에 서 있어야 카운터. 45 = 합쳐서 90° */
	UPROPERTY(EditDefaultsOnly, Category = "Counter", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float HeadHalfAngle = 45.f;
};
