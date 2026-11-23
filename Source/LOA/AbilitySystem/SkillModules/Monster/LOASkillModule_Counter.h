// LOASkillModule_Counter.h
// [스킬 모듈 · 몬스터 전용] Counter — 이 패턴이 도는 동안 카운터를 맞으면 스턴에 걸린다. 판정은 맞는 쪽(보스)이 한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 카운터 패턴 GA(부모 ULOAMonsterAbility)의 Modules에 넣는다. 카운터가 아닌 패턴에는 넣지 않는다.
//   예) GA_Boss_CounterSlash
//        Montage   : 0.8~1.3초에 LOA Grant Tags(State.Window.Counterable) 구간 = 파란 빛 (언제 카운터되나)
//        Modules   : [0] Spawn (베기 장판)  [1] Counter (Stun Duration 4)
//
// [흐름] (전부 서버)
//   패턴 발동 → 모듈이 Event.Combat.Hit을 기다림 (Trigger: On Event, 패턴이 도는 동안만. 부모 모듈이 처리)
//   플레이어 B가 보스를 침 → ULOACombatLibrary::ApplyPayloads가 데미지를 준 뒤 보스에게 Event.Combat.Hit
//        (누가 = B, 공격 속성 = B 스킬의 Skill.Attribute.* 태그)
//   → 이 모듈의 판정
//        ① 공격 속성에 Skill.Attribute.Counter가 있나       없으면 아무 일 없음 (일반 공격)
//        ② 판정 시각에 State.Counterable이었나 (파란 구간)  아니면 로그만 (구간 밖)
//        ③ 공격 판정 기록의 공격 방향이 Front인가 (B가 내 정면)    아니면 로그만 (옆·뒤에서 침)
//   → 성공: 내 ASC에 Event.Combat.Counter (카운터 성공 알림, 지금은 듣는 곳 없음)
//           → 내게 Stun Effect (SetByCaller.Duration = Stun Duration) → 스턴 태그 → ULOAGA_Stun이 이 패턴을 취소하고 스턴
//             (2026-11-10: Event.Trigger.Stun 이벤트 → 스턴 GE. 플레이어 스턴과 같은 길)
//   실패 = 패턴이 그대로 끝까지 간다. (별도 패널티 없음)
//
// [판정 시각·공격 방향] (2026-11-03 사용자 결정: 원격 플레이어 화면 기준)
//   "맞았다" 신호의 쪽지(EventData->ContextHandle)에 공격 판정 기록(FLOAAttackJudgeRecord)이 온다. 데미지 GE와 같은 기록이다.
//     ServerTime : 공격자 판정이면 B 화면이 보여 주던 서버 시각(보고의 ViewServerTime), 아니면 판정한 순간
//     AttackSide : 그 자리·방향으로 정한 공격 방향 (앞 / 옆 / 뒤). 각도 = 헤드 어택 각도 (Project Settings, 패턴마다 바꾸지 않음)
//     → 데미지 숫자에 HEAD가 뜨는 자리 = 카운터가 정면으로 인정하는 자리. 계산은 라이브러리 한 곳 (이 모듈은 읽기만)
//   ② 때문에 파란 구간이 열리고 닫힌 서버 시각을 이 모듈이 적어 둔다 (CounterWindows). 카운터 패턴에서만 필요한 기록이라 여기 둔다.
//     예) 서버 구간 0.80~1.30초. B 화면에서 1.28초 보스를 보고 침 → 보고가 1.38초 도착 → 1.28초는 구간 안 → 성공
//         (전에는 도착 시각 1.38초의 태그를 봐서 실패였다)
//   반대로 B 화면에서 구간이 열리기 전에 친 건, 서버에 도착할 때 구간이 열려 있어도 실패다 (화면 기준)
//   조작: 클라가 바꿀 수 있는 건 보고의 시각뿐이고, 되감기 한도(핑 + 0.3초)로 잘린다. 구간·자리·맞은 쪽은 서버 기록으로 정한다
//   [한계] 보고가 패턴이 끝난 뒤 도착하면 이 모듈이 없어서 판정 못 한다 (전과 같음).
//          구간 끝 → 패턴의 타격까지 틈이 되감기 한도보다 길면 받아 줄 보고는 전부 패턴 안에 온다 → 패턴 만들 때 틈을 둔다
//
// [왜 모듈인가] (2026-10-18 사용자 결정)
//   - 판정은 맞는 쪽이 한다. 공격 쪽(전투 라이브러리)은 "누가, 어떤 속성으로 쳤다"만 알린다.
//   - 카운터 데이터(스턴 시간)가 카운터 패턴에만 있다. 패턴 부모(ULOAMonsterAbility)에 두면 모든 패턴에 칸이 보인다.
//   - "패턴이 도는 동안만"이 저절로 된다. (모듈의 이벤트 대기는 패턴이 끝나면 엔진이 정리)
//
// [네트워크]
//   1. 서버만     : 구간 시각 기록, 판정, Event.Combat.Counter, 스턴 GE, 스턴 GA
//   2. 복제       : State.Window.Counterable·스턴 태그, 스턴 몽타주 (원래대로)
//   3. 각자 할 일 : 없음
//
// [에디터에서]
//   Trigger / Trigger Event Tag : 기본값 그대로 (On Event / Event.Combat.Hit). 바꾸면 저장 시 경고
//   Stun Effect                 : 스턴 GE (GE_CC_Stun). 비어 있으면 저장 시 경고
//   Stun Duration               : 카운터 성공 시 스턴 시간 (초)
//   정면 각도 칸은 없다. Project Settings → LOA Combat → Directional Attack Half Angle (헤드 어택과 같음, 2026-11-03)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "LOASkillModule_Counter.generated.h"

class ULOAGE_Stun;

/** 파란 구간 하나의 서버 시각 (초). 닫히지 않았으면 CloseTime < 0 */
struct FLOACounterWindow
{
	double OpenTime = 0.0;
	double CloseTime = -1.0;
};

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
	/** [서버] 지난 발동의 구간 기록을 비우고, State.Counterable이 붙고 떨어지는 시각을 적기 시작한다 */
	virtual void SkillModuleInit() override;

	/** Event.Combat.Hit이 올 때마다 (맞을 때마다) 카운터 판정. 헤더 [흐름], [판정 시각·공격 방향] */
	virtual void SkillModuleTriggered(const FGameplayEventData* EventData) override;

	/** [서버] 구간 기록을 멈춘다 */
	virtual void SkillModuleEnd(bool bWasCancelled) override;

	/** 카운터 성공 시 보스에게 거는 스턴 GE (GE_CC_Stun) */
	UPROPERTY(EditDefaultsOnly, Category = "Counter")
	TSubclassOf<ULOAGE_Stun> StunEffect;

	/** 카운터 성공 시 스턴 시간 (초). 패턴마다 다르다. 스턴 GE에 SetByCaller.Duration으로 넣는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Counter", meta = (ClampMin = "0.1", Units = "Seconds"))
	float StunDuration = 3.f;

private:
	/** [서버] State.Window.Counterable 개수가 0 ↔ 1 이상으로 바뀔 때 ASC가 부른다 → 열린 / 닫힌 시각을 적는다 */
	void OnCounterableTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 그 서버 시각에 파란 구간이었나 (이번 발동의 기록으로) */
	bool WasCounterableAt(double ServerTime) const;

	/** 이번 발동의 파란 구간들 (오래된 것부터). 한 패턴에 구간이 여러 개일 수 있다 */
	TArray<FLOACounterWindow> CounterWindows;

	/** State.Window.Counterable 구독 번호표 (SkillModuleEnd에서 해제) */
	FDelegateHandle CounterableTagChangedHandle;
};
