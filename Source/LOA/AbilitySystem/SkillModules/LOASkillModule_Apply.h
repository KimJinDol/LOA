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
// [판정은 판정 매니저가] (2026-11-02, 판정 매니저 2026-11-04)
//   이 모듈은 "언제(Trigger), 누구에게(Targeting), 무엇을(Payloads)"만 갖는다. 누가 판정하는지는 모른다.
//   이 모듈이 아는 것은 판정 매니저(ULOACombatJudgeManagerSubsystem) 하나다. 판정 방식(서버 / 피해자 / 공격자)과 저가는 모른다.
//   판정 매니저가 정한다: 원격 플레이어의 적 공격이면 그 플레이어 화면으로 판정(공격자 판정: 공격 보고 + 서버 되감기 검사),
//   그 밖(보스 패턴, 리슨 호스트 본인, 아군, Self)은 서버 판정 (Hit Judge = Victim인 보스 공격이면 원격 플레이어는 피해자 판정)
//   이 모듈이 아는 판정 관련 값은 "이 발동에서 받을 보고 수 = 몽타주의 그 노티파이 수" 하나 (모듈 설정이라)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   SkillModuleInit      : 설정 누락 경고 → 판정 매니저.BeginAttackReports (받을 보고 수 = CountAllowedReports)
//   SkillModuleTriggered : 판정 매니저.JudgeInstantHit (내 컴퓨터와 서버 둘 다 부른다. 할 일은 매니저가 정함)
//   SkillModuleEnd       : 판정 매니저.EndAttackReports
//
// 다단 히트: 몽타주에 같은 태그 노티파이를 여러 개 찍으면 찍은 만큼 적용. (지금 휠윈드와 같음) 보고도 노티파이마다 하나
// Payload 여러 개: 같은 대상에게 한 번에 여러 GE. (예: 데미지 + 이동 속도 감소 디버프)
// 대상이 다른 효과(적에게 데미지 + 나에게 버프)는 Payload가 아니라 Apply 모듈을 하나 더 넣는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크] (자세한 건 LOACombatJudgeManagerSubsystem.h, LOAAttackJudgeSubsystem.h)
//   1 서버만  : GE 적용, 공격 보고 검사
//   2 전달    : 공격 보고 (내 컴퓨터 → 서버, GAS TargetData)
//   3 각자    : 원격 플레이어 컴퓨터의 대상 찾기 (내 화면 기준)
//   다른 플레이어 : Ability가 없다. 결과(체력, 데미지 숫자)만 받는다.

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
	virtual void SkillModuleEnd(bool bWasCancelled) override;
	//~ End ULOASkillModule Interface

public:
	/** 대상에게 줄 GE들 (툴팁이 읽음, 2026-11-14) */
	virtual const TArray<FLOAEffectPayload>* GetPayloads() const override { return &Payloads; }

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
	/** 이 발동에서 받을 보고 수 = 몽타주의 그 노티파이 수 (콤보는 모든 섹션 합. On Skill Start면 1, 몽타주에 없는 이벤트면 1) */
	int32 CountAllowedReports() const;
};
