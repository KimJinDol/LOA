// LOASkillModule.h
// [배관] 스킬 모듈의 부모. 모듈 = "신호가 오면 자기 일 하나를 하는 부품".
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
// Ability(GA)는 "몽타주를 어떻게 넘기나"(흐름)만 맡고, "무엇을 하나"(행동)는 모듈이 맡는다.
// 한 Ability에 모듈을 여러 개 넣을 수 있고, 모듈끼리는 서로 모른다.
//
//   GA_Berserker_Whirlwind
//    └ Modules
//        [0] Apply : Hit 신호가 오면 → 주변 적에게 → 데미지
//        [1] Cue   : Hit 신호가 오면 → 이펙트 (나중에)
//
// 모듈 종류
//   Apply : 대상에게 GE를 준다 (데미지, 나중에 힐/버프)
//   Spawn : 액터를 소환한다 (투사체, 나중에 장판). 데미지는 소환된 액터가 맞힐 때 준다
//   Move  : 보는 방향으로 이동한다 (돌진, 점프, 관통 대시). 맞히지 않는다
//   Cue / TurnToCursor : 쓰는 스킬이 생길 때 추가
//   몬스터 전용 (SkillModules/Monster/)
//   Counter : 이 패턴이 도는 동안 카운터 속성 공격을 파란 구간에 헤드로 맞으면 → 스턴 (판정은 맞는 쪽)
//
// ─────────────────────────────────────────────────────────────────────────────
// [언제 실행하나 - Trigger] (모든 모듈 공통. 이 부모가 처리한다)
//
//   ▼ [0] [ Apply ▼ ]
//        Trigger            [ On Event ▼ ]          ← 이 부모의 칸
//        Trigger Event Tag  [ Event.Montage.Hit ]   ← On Event일 때만 보임
//        …자식(Apply)의 칸…
//
//   On Skill Start : 스킬이 시작하자마자 1번         (버프, 차지 중 커서 따라 돌기 …)
//   On Event       : 이벤트가 올 때마다 (기본값)     (몽타주 타격 노티파이, GA가 보내는 신호 …)
//
//   기본값이 On Event인 이유: 공격 스킬이 대부분이다. 태그를 깜빡 비우면 "즉시 실행"이 아니라
//   경고 로그 + 실행 안 함 → 실수가 바로 드러난다. 즉시 실행은 On Skill Start를 직접 골라야 한다.
//
//   "멈추는 시점"은 여기 없다. 일정 시간 계속되는 모듈(TurnToCursor 등)만 자기 칸(StopEventTag)으로 가진다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   Ability 발동 → (ULOAPlayerAbility) 커서 회전 → BeginSkill
//      ├ ULOAGameplayAbility::ActivateSkillModules → 모든 모듈의 ActivateModule
//      │     ① 주인 기억
//      │     ② SkillModuleInit()            ← 자식: 발동마다 초기화할 것 (선택. 한 번만이 아니라 매 발동)
//      │     ③ RunOrWaitByTrigger()         ← 이 부모: Trigger 설정대로 바로 실행 또는 이벤트 대기
//      │          On Skill Start → 바로 SkillModuleTriggered()
//      │          On Event       → 이벤트 대기 Task → 올 때마다 SkillModuleTriggered()
//      └ StartSkill → 몽타주 재생
//   Ability 종료 (정상이든 취소든) → 모든 모듈의 DeactivateModule
//      ④ SkillModuleEnd()                   ← 자식: 정리할 것 (선택)
//      ⑤ 주인 잊기
//
// ─────────────────────────────────────────────────────────────────────────────
// [수명 - 누가 정리하나]
// - 이벤트 대기 Task는 주인 Ability 소속이다. → Ability가 끝나거나 취소되면 엔진이 정리한다.
//   자식이 Task를 따로 만들 때도 반드시 GetOwnerAbility() 소속으로 만든다.
// - Ability는 InstancedPerActor라서 인스턴스를 재사용한다. 모듈도 같이 재사용된다.
//   → 발동마다 바뀌는 값은 SkillModuleInit에서 초기화한다. (이전 발동 값이 남지 않게)
//
// [복사본]
// Modules는 Instanced 프로퍼티다. Ability 인스턴스가 만들어질 때 BP(CDO)의 모듈이 인스턴스마다 복사된다.
// → 플레이어 A의 휠윈드와 플레이어 B의 휠윈드는 모듈을 공유하지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// 모듈은 Ability가 도는 곳(조종하는 사람 컴퓨터 + 서버)에서 둘 다 돈다.
// 결과가 남는 일(데미지, 힐, 액터 생성)은 HasSkillModuleAuthority()가 true일 때(서버)만 한다.
// 모듈 자체는 복제되지 않는다. (Ability 안의 부품일 뿐)
//
// ─────────────────────────────────────────────────────────────────────────────
// [새 모듈 만드는 법]
//   1) ULOASkillModule을 상속, UCLASS(meta = (DisplayName = "드롭다운에 보일 이름"))
//   2) SkillModuleTriggered에 할 일을 쓴다. (언제 부를지는 부모가 Trigger 설정대로 정한다)
//   3) 발동마다 초기화할 게 있으면 SkillModuleInit, 정리할 게 있으면 SkillModuleEnd
//   4) 저장 시 검사가 필요한 칸이 있으면 ValidateSkillModule (Super 먼저)
//
// [이벤트 태그 이름 규칙]
//   Event.Montage.* : 몽타주 노티파이가 보내는 신호  → 저장 시 "몽타주에 이 노티파이가 있나" 확인
//   Event.Skill.*   : GA가 코드로 보내는 신호        (예: Event.Skill.Charge.Released) → 확인 안 함
//   Event.Combat.*  : 전투 중 서버가 보내는 신호      (예: Event.Combat.Hit 맞았다) → 확인 안 함

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "LOASkillModule.generated.h"

class ULOAGameplayAbility;
class UAnimMontage;
struct FLOAEffectPayload;

/** 모듈을 언제 실행하나 */
UENUM(BlueprintType)
enum class ELOASkillModuleTrigger : uint8
{
	/** 스킬이 시작하자마자 1번 */
	OnSkillStart	UMETA(DisplayName = "On Skill Start"),

	/** Trigger Event Tag 이벤트가 올 때마다 (몽타주 노티파이, GA가 보내는 신호) */
	OnEvent			UMETA(DisplayName = "On Event"),
};

/**
 * 스킬 모듈 부모.
 *   Abstract         : 이 클래스 자체는 드롭다운에 안 나옴 (자식만)
 *   EditInlineNew    : 디테일 패널에서 바로 만들어 넣을 수 있음
 *   DefaultToInstanced : 이 타입 프로퍼티는 자동으로 Instanced (Ability마다 자기 복사본)
 *   CollapseCategories : 디테일 패널에서 카테고리 접힘 없이 값이 바로 보임
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class LOA_API ULOASkillModule : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Ability가 부른다. 주인 기억 → SkillModuleInit → Trigger 설정대로 대기 시작(또는 바로 실행).
	 * 자식은 이 함수를 덮어쓰지 않는다. (주인 기억과 Trigger 처리를 빠뜨리지 않게)
	 */
	void ActivateModule(ULOAGameplayAbility* InOwnerAbility);

	/** Ability가 끝날 때 부른다. SkillModuleEnd를 부르고 주인을 잊는다. */
	void DeactivateModule(bool bWasCancelled);

	/**
	 * 이 모듈이 대상에게 주는 GE 목록. 없는 모듈(Move 등)은 nullptr. (2026-11-14 툴팁)
	 * 읽기만 한다. 예) 툴팁 {Damage} = 공격력 × 첫 SetByCaller.DamageRate (ULOAPlayerAbility::FindDamageRate)
	 */
	virtual const TArray<FLOAEffectPayload>* GetPayloads() const { return nullptr; }

#if WITH_EDITOR
	/**
	 * [에디터 전용] 저장 시 검사. 주인 GA의 IsDataValid가 모듈마다 부른다.
	 * 부모(여기): Trigger = On Event인데 태그 없음 / Event.Montage.* 태그인데 몽타주에 그 노티파이가 없음
	 * 자식: Super를 부르고 자기 칸을 검사한 뒤 결과를 합친다.
	 *
	 * @param OwnerMontage  주인 GA의 몽타주 (없으면 노티파이 확인을 건너뜀)
	 * @param Prefix        메시지 앞에 붙일 이름 (예: "GA_Whirlwind_C : Modules[0]")
	 */
	virtual EDataValidationResult ValidateSkillModule(
		class FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const;
#endif

protected:
#if WITH_EDITOR
	/**
	 * [에디터 전용] Payloads 칸 검사. Payloads를 가진 모듈(Apply, Spawn)이 같은 규칙을 쓰도록 여기 한 곳에 둔다.
	 *   오류 : Effect를 안 고른 칸
	 *   경고 : Values에 태그를 안 고른 칸 / GE가 LOADamageExecution을 쓰는데 SetByCaller.DamageRate가 없음 (데미지 0)
	 * "Payloads가 비었나"는 여기서 보지 않는다. (Apply는 오류, Spawn은 경고로 모듈마다 다르다)
	 *
	 * @param Name  메시지 앞에 붙일 이름 (예: "GA_Arrow_C : Modules[0] Spawn")
	 */
	static EDataValidationResult ValidateSkillModulePayloads(
		class FDataValidationContext& Context, const TArray<FLOAEffectPayload>& Payloads, const FString& Name);
#endif

	// ---------- 자식이 덮어쓰는 곳 ----------

	/**
	 * 발동마다 초기화할 것. Trigger 처리 전에 불린다. (조종하는 사람 컴퓨터와 서버 각각) 보통 비워 둔다.
	 * "Init"이지만 처음 한 번만이 아니라 스킬을 쓸 때마다 불린다. (모듈은 Ability와 함께 재사용된다 → [수명])
	 * 일은 하지 않는다. 진짜 일(데미지, 소환)은 SkillModuleTriggered. (Trigger의 On Skill Start도 SkillModuleTriggered를 부른다)
	 */
	virtual void SkillModuleInit() {}

	/**
	 * 모듈의 진짜 일. Trigger 설정대로 부모가 부른다. (조종하는 사람 컴퓨터와 서버 각각)
	 * @param EventData  On Event면 받은 이벤트 내용, On Skill Start면 nullptr
	 */
	virtual void SkillModuleTriggered(const FGameplayEventData* EventData) {}

	/** 스킬 끝. Task는 엔진이 정리하므로 보통 할 일 없음. Task가 아닌 걸 붙였을 때만 여기서 정리 */
	virtual void SkillModuleEnd(bool bWasCancelled) {}

	// ---------- 도우미 ----------

	/** 이 모듈이 들어 있는 Ability. 스킬이 돌고 있지 않으면 nullptr */
	ULOAGameplayAbility* GetOwnerAbility() const;

	/** 서버인가. 결과가 남는 일(데미지 등)은 이게 true일 때만 한다. */
	bool HasSkillModuleAuthority() const;

	// ---------- 데이터 (Ability BP의 디테일 패널에서. 모든 모듈 공통) ----------

	/** 언제 실행하나. 기본은 On Event (이벤트를 깜빡 비우면 경고 + 실행 안 함) */
	UPROPERTY(EditDefaultsOnly, Category = "Trigger")
	ELOASkillModuleTrigger Trigger = ELOASkillModuleTrigger::OnEvent;

	/**
	 * 기다릴 이벤트 태그. 몽타주 "LOA Send Gameplay Event" 노티파이(또는 GA가 보내는 신호)의 태그와 같아야 한다.
	 * 같은 태그 이벤트가 여러 번 오면(다단 히트) 올 때마다 실행한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Trigger",
		meta = (Categories = "Event", EditCondition = "Trigger == ELOASkillModuleTrigger::OnEvent", EditConditionHides))
	FGameplayTag TriggerEventTag;

private:
	/**
	 * Trigger 설정을 보고 둘 중 하나를 한다. (ActivateModule에서 발동마다 1번)
	 *   On Skill Start : 지금 바로 SkillModuleTriggered를 1번 부른다.
	 *   On Event       : 이벤트 대기 Task만 켜 둔다. 실제 실행은 이벤트가 올 때마다 (OnTriggerEventReceived)
	 */
	void RunOrWaitByTrigger();

	/** 이벤트 도착 (조종하는 사람 컴퓨터와 서버 각각에서 호출) */
	UFUNCTION()
	void OnTriggerEventReceived(FGameplayEventData EventData);

	/** 지금 스킬을 돌리고 있는 주인 Ability (ActivateModule ~ DeactivateModule 동안만 값이 있음) */
	TWeakObjectPtr<ULOAGameplayAbility> OwnerAbility;
};
