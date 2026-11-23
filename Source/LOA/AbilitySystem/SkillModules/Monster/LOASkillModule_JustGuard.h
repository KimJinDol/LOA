// LOASkillModule_JustGuard.h
// [스킬 모듈 · 몬스터 전용] Just Guard — 이 패턴의 저가 타격마다 "몇 명이 막았나"를 센다. (2026-10-26, 1단계)
// 판정은 하지 않는다. 저가 매니저(ULOAJustGuardSubsystem)의 OnJustGuardResolved를 패턴이 도는 동안만 듣는다.
// 타격 하나 = 판정 매니저의 공격 하나. 저가 매니저가 판정 매니저의 "판정 끝" 알림을 듣고 바로 알려 준다 (2026-11-04)
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 곳] 저가 패턴 GA(부모 ULOAMonsterAbility)
//   Asset Tags : Skill.Attribute.JustGuard
//   Montage    : 공격마다 LOA Just Guard Window 구간
//   Modules    : [0] Apply 또는 Spawn (공격)  [1] Just Guard
//
// [흐름] (서버)
//   패턴 시작 → 구독 / 타격 하나 끝날 때마다 → 내 공격이면 센다 (사람마다 막은 타격 수도) / 패턴 끝 → 구독 해제 + 로그
//   패턴 끝 → 보스 자신에게 Event.JustGuard.PatternResolved (EventMagnitude = 성공 인원) (2026-11-18 apply128)
//     성공 = 그 패턴의 저가 타격을 전부 막은 사람. 피해서 모양 밖이었거나 죽어서 못 막은 사람은 실패
//     예) 3연타 패턴: A 3/3, B 3/3, C 2/3 → 성공 2명 (등급 = Great, ULOAJustGuardSubsystem::GetCoopJustGuardGrade)
//     예) 타격 전에 끊김 → 타격 0 → 성공 0명
//   누가 듣는지 모른다 (지금 듣는 곳: 대난투 그로기 GA → 추가 시간). 평소 패턴에서도 보낸다 (듣는 곳이 없으면 그냥 지나감)
//
// [한계] 패턴이 끝난 뒤에 맞히는 장판은 세지 않는다
// [2단계 남은 것] 그로기, 실패자만 공격 (등급은 apply128)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "LOASkillModule_JustGuard.generated.h"

struct FLOAJustGuardResolved;
class APawn;

UCLASS(meta = (DisplayName = "Just Guard"))
class LOA_API ULOASkillModule_JustGuard : public ULOASkillModule
{
	GENERATED_BODY()

public:
	ULOASkillModule_JustGuard();

#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사: Trigger가 On Skill Start인지, 주인 GA에 Skill.Attribute.JustGuard가 있는지 */
	virtual EDataValidationResult ValidateSkillModule(
		class FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const override;
#endif

protected:
	//~ Begin ULOASkillModule Interface
	/** 발동마다 집계 초기화 */
	virtual void SkillModuleInit() override;
	/** 패턴 시작: 저가 매니저의 알림 구독 (서버) */
	virtual void SkillModuleTriggered(const FGameplayEventData* EventData) override;
	/** 패턴 끝: 구독 해제 + 로그 */
	virtual void SkillModuleEnd(bool bWasCancelled) override;
	//~ End ULOASkillModule Interface

private:
	/** 저가 매니저: 저스트 가드 공격 하나의 판정이 끝났다 */
	void OnJustGuardResolved(const FLOAJustGuardResolved& Result);

	/** 구독 해제 */
	void StopListening();

	/** 패턴 끝: 보스 자신에게 성공 인원을 알린다 (헤더 [흐름]) */
	void SendPatternResolved() const;

	/** 이번 발동의 집계 */
	int32 NumAttacks = 0;
	int32 NumGuarded = 0;
	int32 NumHit = 0;

	/** 이번 발동에서 사람마다 막은 타격 수 (막은 적 없는 사람은 없음). 패턴 끝에 타격 수와 같으면 성공 */
	TMap<TWeakObjectPtr<APawn>, int32> GuardCounts;

	/** 구독 번호표 + 구독한 저가 매니저 */
	FDelegateHandle ResolvedHandle;
	TWeakObjectPtr<class ULOAJustGuardSubsystem> ListenedJustGuardSubsystem;
};
