// LOAGameplayEffectContext.h
// [배관] 우리 프로젝트의 Effect Context. GE를 줄 때 같이 따라가는 "이 공격의 정보 쪽지"다. (2026-11-02)
// 엔진 쪽지(FGameplayEffectContext: 누가 때렸나, 맞은 지점 …)에 "이번 타격을 판정한 순간의 기록" 칸을 더한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [지금 있는 칸]
//   AttackJudgeRecord (FLOAAttackJudgeRecord, 2026-11-03에 DirectionalAttack 칸을 이 안으로 옮김)
//     AttackSide        : 공격 방향 (앞 / 옆 / 뒤)             → 카운터 (ULOASkillModule_Counter)
//     DirectionalAttack : 백 / 헤드 어택이었나            → ULOADamageExecution (보너스), ULOAAttributeSet_Common (숫자 옆 글자)
//     ServerTime        : 판정 기준 서버 시각              → 카운터 (그 시각에 파란 구간이었나)
//     bCritical         : 치명타였나 (2026-11-13)          → ULOADamageExecution (치명타 배율), ULOAAttributeSet_Common (노란 숫자)
//     정하는 곳 : ULOACombatLibrary의 DecideAttackJudgeRecord 한 곳. 적는 곳은 둘 (데미지 GE의 쪽지 = ApplyPayload, "맞았다" 신호의 쪽지 = SendHitEvent)
//       치명타는 무작위라 ApplyPayloads가 대상마다 한 번 정해 둘에 같은 기록을 넘긴다
//   보스·허수아비도 같은 쪽지를 쓴다. 보스 공격에는 백·헤드 속성이 없어서 칸이 None으로 비어 있을 뿐이다 (사용자 상의 2026-11-02:
//   쪽지 종류를 나누면 읽는 쪽마다 "어느 종류인가"를 따져야 하고, 장판·투사체처럼 같이 쓰는 경로에서 갈린다)
//
// [왜 쪽지에 두나] (사용자 결정 2026-11-02)
//   전에는 데미지 계산(Execution) 안에서 "그 순간 서버가 아는 보스 자리·방향"으로 방향을 정했다.
//   공격자 판정은 "내 화면에서 본 보스 자리" (서버가 되감은 자리)로 정해야 한다. 그 자리를 아는 건 판정하는 쪽뿐이다.
//   → 판정하는 쪽(ApplyPayload)이 정해서 쪽지에 적고, 계산과 데미지 숫자는 읽기만 한다. 계산 쪽에는 각도 계산이 없다.
//   태그가 아니라 값 칸: 결과 하나를 담으려고 태그를 만들지 않는다 (사용자).
//
// [엔진에 알리기]
//   엔진은 쪽지를 UAbilitySystemGlobals::AllocGameplayEffectContext로 만든다.
//   → ULOAAbilitySystemGlobals가 이 쪽지를 만들고, DefaultGame.ini에서 그 클래스를 쓰라고 알린다.
//     [/Script/GameplayAbilities.GameplayAbilitiesDeveloperSettings] AbilitySystemGlobalsClassName=/Script/LOA.LOAAbilitySystemGlobals
//   그러면 ASC->MakeEffectContext()가 이 쪽지를 돌려준다. 쓰는 쪽은 FLOAGameplayEffectContext::Get(핸들)로 꺼낸다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1 서버만 : 칸에 적기(ApplyPayload, SendHitEvent), 읽기(데미지 계산, 데미지 숫자 정보, 카운터). 전부 서버에서 한다.
//   2 복제   : 이 칸은 보내지 않는다. 쪽지 자체는 Gameplay Cue 등으로 복제될 수 있는데, 그때는 엔진 칸만 간다
//              (NetSerialize를 덮어쓰지 않음). 받은 쪽의 이 칸은 None.
//   3 각자   : 없음

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Library/LOACombatTypes.h"   // FLOAAttackJudgeRecord
#include "LOAGameplayEffectContext.generated.h"

USTRUCT()
struct LOA_API FLOAGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

public:
	/**
	 * 핸들 안의 쪽지가 우리 쪽지면 꺼낸다. 아니면(엔진 기본 쪽지, 빈 핸들) nullptr.
	 * 예) DefaultGame.ini 설정이 빠졌으면 엔진 기본 쪽지라 nullptr → 백 / 헤드 보너스가 안 붙는다 (경고 없음, 체크리스트 참고)
	 *     globals 클래스 설정을 바꾼 뒤에는 에디터를 완전히 껐다 켜야 한다 (켜 둔 채 빌드하면 예전 클래스가 남음, 2026-11-03 확인)
	 */
	static FLOAGameplayEffectContext* Get(FGameplayEffectContextHandle& Handle);
	static const FLOAGameplayEffectContext* Get(const FGameplayEffectContextHandle& Handle);

	/** 공격 판정 기록 (공격 방향, 백 / 헤드 결과, 판정 시각). ApplyPayload와 SendHitEvent만 적는다 */
	const FLOAAttackJudgeRecord& GetAttackJudgeRecord() const { return AttackJudgeRecord; }
	void SetAttackJudgeRecord(const FLOAAttackJudgeRecord& InAttackJudgeRecord) { AttackJudgeRecord = InAttackJudgeRecord; }

	//~ Begin FGameplayEffectContext Interface
	/** 엔진이 쪽지 종류를 알아볼 때 쓴다 (복제할 때 같은 종류로 만든다) */
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	/** 쪽지 복사 (엔진이 Spec을 복사할 때). 우리 칸까지 복사하고, 맞은 지점은 엔진처럼 깊은 복사 */
	virtual FLOAGameplayEffectContext* Duplicate() const override;
	//~ End FGameplayEffectContext Interface

protected:
	/** 공격 판정 기록. 기본 비어 있음 (전부 None, 시각 0) */
	UPROPERTY()
	FLOAAttackJudgeRecord AttackJudgeRecord;
};

/**
 * 엔진 쪽지와 같은 성질을 알린다.
 *   WithNetSerializer : 엔진 쪽지의 NetSerialize를 그대로 쓴다 (우리 칸은 안 보냄. 헤더 [네트워크])
 *   WithCopy          : 대입(=)으로 복사할 수 있다 (Duplicate가 쓴다)
 */
template<>
struct TStructOpsTypeTraits<FLOAGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FLOAGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true,
	};
};
