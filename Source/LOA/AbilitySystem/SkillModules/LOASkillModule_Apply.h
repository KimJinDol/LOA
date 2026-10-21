// LOASkillModule_Apply.h
// [배관] Apply 모듈. "[언제] 신호가 오면, [누구]에게, [무엇]을 준다."
// (기존 LOAGA_ShapeAttack의 OnHitEvent를 부품으로 떼어 낸 것)
//   언제     : 부모(ULOASkillModule)의 Trigger 칸
//   누구에게 : Targeting
//   무엇을   : Payloads
//
// ─────────────────────────────────────────────────────────────────────────────
// [디테일 패널]
//
//   ▼ [0]  [ Apply ▼ ]
//        Trigger            [ On Event ▼ ]          ← 언제 (부모 칸)
//        Trigger Event Tag  [ Event.Montage.Hit ]
//        ▼ Targeting  [ Shape ▼ ]                    ← 누구에게
//             Shape       Sphere / Radius 300 / Forward 0
//        ▼ Payloads   [ + ]                          ← 무엇을 (여러 개 가능)
//           ▼ [0]
//             Effect   [ GE_Damage_Basic ]
//             Values   [0] SetByCaller.DamageRate = 2.5
//
//   버프처럼 타이밍이 필요 없으면 Trigger = On Skill Start.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   SkillModuleInit      : 설정 누락 경고만 (Targeting / Payloads 비어 있음)
//   SkillModuleTriggered : (부모가 Trigger 설정대로 부름) → ApplyToTargets
//
//   ApplyToTargets [서버만]
//      Context = 주인->MakeSkillModuleContext()
//      ULOAHitJudgeSubsystem::JudgeAndApply(Targeting, Context, Payloads, Animation) → 대상 판정 + 대상마다, Payload마다 GE 적용
//        Animation: 예고가 없으니 플레이어는 시전자의 모션을 보고 피한다. (피해자 판정 시각 = 내 화면에서 공격이 닿은 순간)
//        판정 방식(서버 / 피해자)은 Targeting의 Hit Judge 칸. 이 모듈은 모른다.
//        Victim이면 원격 플레이어는 보고를 받은 뒤 적용된다. (이 GA가 먼저 끝나도 판정 담당이 복사본으로 적용)
//
// 다단 히트: 몽타주에 같은 태그 노티파이를 여러 개 찍으면 찍은 만큼 적용. (지금 휠윈드와 같음)
// Payload 여러 개: 같은 대상에게 한 번에 여러 GE. (예: 데미지 + 이동 속도 감소 디버프)
// 대상이 다른 효과(적에게 데미지 + 나에게 버프)는 Payload가 아니라 Apply 모듈을 하나 더 넣는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   조종하는 사람 컴퓨터 : 신호를 받지만 아무것도 안 함 (나중에 Cue 모듈이 이펙트를 예측 재생)
//   서버                 : 신호를 받으면 판정 담당에 대상 판정 + GE 적용을 맡긴다
//   다른 플레이어        : Ability가 없다. 결과(체력, 데미지 숫자)만 받는다.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // FLOAEffectPayload를 값으로 가지므로 include
#include "LOASkillModule_Apply.generated.h"

class ULOASkillTargeting;

UCLASS(meta = (DisplayName = "Apply"))
class LOA_API ULOASkillModule_Apply : public ULOASkillModule
{
	GENERATED_BODY()

protected:
	//~ Begin ULOASkillModule Interface
	virtual void SkillModuleInit() override;
	virtual void SkillModuleTriggered(const FGameplayEventData* EventData) override;
	//~ End ULOASkillModule Interface

public:
#if WITH_EDITOR
	/**
	 * [에디터 전용] 저장 시 검사. (부모의 Trigger 검사 + 아래)
	 *   오류 : Targeting 없음 / Payloads 비어 있음 / Payload의 Effect 없음
	 *   경고 : Values에 태그를 안 고른 칸
	 *   경고 : GE가 LOADamageExecution을 쓰는데 SetByCaller.DamageRate가 없음 (데미지 0)
	 */
	virtual EDataValidationResult ValidateSkillModule(
		class FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const override;
#endif

protected:

	// ---------- 데이터 (Ability BP의 디테일 패널에서) ----------

	/** 누구에게. 드롭다운에서 종류를 고른다. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Apply")
	TObjectPtr<ULOASkillTargeting> Targeting;

	/**
	 * 무엇을. 찾은 대상 모두에게 이 GE들을 준다. (데미지, 힐, 버프 …)
	 * 비어 있으면 아무것도 안 함 (설정 누락 → 경고 로그).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Apply")
	TArray<FLOAEffectPayload> Payloads;

private:
	/** [서버] 대상 찾기 + GE 적용 */
	void ApplyToTargets();
};
