// LOAGA_Identity.h
// [배관] 공용 아이덴티티 켜기/끄기 GA. 게이지 검사·감소·끄기·환급까지만. (2026-11-09)
//   켜진 동안 무엇을 하나는 자식이 정한다 (OnIdentityOn / OnIdentityOff)
//     ULOAGA_Identity_Aura : 일정 간격으로 범위 안 대상에게 효과 (세라스 "축복의 오라")
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙] (2026-11-09 사용자 결정)
//   켜기   : 게이지가 가득(MaxIdentityGauge)일 때만. 키를 누르면 켜짐 (CheckCost)
//   켜진 동안
//     - 게이지 초당 DrainPerSecond(10)씩 감소 (감소 GE, 0.1초마다) → 100이면 10초
//     - 스킬로 게이지가 차지 않음 (State.Identity.Active)
//   끄기
//     - 게이지 0      → 자동으로 꺼짐
//     - 키 다시 누름  → 꺼짐 + 남은 게이지 × RefundRate(0.6). 켠 지 ToggleLockTime(1초) 안에는 안 꺼짐 (연타 방지)
//     - 죽음          → 꺼짐 (사망 GA가 다른 Ability를 취소)
//   죽으면 게이지 0 (켜져 있었든 아니든. 2026-11-09)
//     - 받을 때부터(OnGiveAbility) State.Body.Dead를 지켜본다 → 붙으면 게이지 0. 꺼져 있어도 인스턴스는 살아 있다 (InstancedPerActor)
//   예) 100 켬 → 4초 뒤 60에서 끔 → 36 남음 → 64 더 채우면 다시 켬
//
// [네트워크]
//   발동은 다른 플레이어 스킬처럼 예측 (LocalPredicted). 게이지·감소 GE·자식의 효과는 서버만
//   끄기 키는 서버가 받아서 처리하고(복제된 입력 이벤트, WaitInputPress) 종료를 클라에 알린다
//
// [켜는 동작] (2026-11-09) 이동 금지 + Unlock 노티파이에서 풀림 (GA_sp_Identity)
//   그 뒤 이동해도 꺼지지 않는다 (생성자: bEndOnRecoveryMove = false)
//   노티파이 전에 몽타주가 끊겨도(다른 스킬·CC) 끝나는 순간 잠금을 푼다 (OnStartMontageEnded)
//   켜는 몽타주 동안만 피격 이상·상태 이상 면역 (모든 직업 공통, 2026-11-09 사용자 결정. 태그는 C++ 고정)
//     State.Immune.Push + State.Immune.Status 를 루즈 태그로 붙였다가 몽타주가 끝나면(완료·끊김·취소) 뗀다
//     서버·내 컴퓨터가 각자 붙인다 (복제 안 함. CC 판정은 서버)
//
// [하드코딩 금지] 숫자는 전부 BP 칸

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "GameplayEffectTypes.h"
#include "LOAGA_Identity.generated.h"

struct FOnAttributeChangeData;

UCLASS(Abstract)
class LOA_API ULOAGA_Identity : public ULOAPlayerAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Identity(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UGameplayAbility Interface
	/** 마나 검사(부모) + 게이지가 가득인가 */
	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	/** [서버] 사망 태그 지켜보기 시작 */
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** 사망 태그 지켜보기 끝 */
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	//~ End UGameplayAbility Interface

protected:
	//~ Begin UGameplayAbility Interface
	/** [서버] 자식 정리(OnIdentityOff), 감소 GE 떼기, 키로 껐으면 환급(× RefundRate). 죽음 → 0은 OnDeadTagChanged */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	//~ Begin ULOAPlayerAbility Interface
	/** 켜기: 몽타주(끝나도 Ability는 안 끝남) + [서버] 감소 GE, 게이지 감시, OnIdentityOn + 끄기 키 대기 */
	virtual void StartSkill() override;
	//~ End ULOAPlayerAbility Interface

	/** [서버] 켜졌다. 자식이 켜진 동안의 일을 시작한다 (오라 타이머 등) */
	virtual void OnIdentityOn() {}

	/** [서버] 꺼진다 (끔 / 0 / 죽음 모두). 자식이 OnIdentityOn에서 시작한 것을 정리한다 */
	virtual void OnIdentityOff() {}

	// ---------- 데이터 (Class Defaults) ----------

	/** 켜진 동안 초당 줄어드는 게이지 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Identity", meta = (ClampMin = "0.0"))
	float DrainPerSecond = 10.f;

	/** 키로 끌 때 남은 게이지를 얼마나 돌려주나. 0.6 = 60% (60 남았으면 36) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Identity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RefundRate = 0.6f;

	/** 켠 뒤 이 시간 동안은 키로 끌 수 없다 (연타 방지) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Identity", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ToggleLockTime = 1.f;

private:
	/** 켜는 몽타주가 끝남 (완료·끊김·취소 모두) → 면역 떼기 + 아직 잠겨 있으면 풀기 (Unlock 노티파이 전에 끊긴 경우) */
	UFUNCTION()
	void OnStartMontageEnded();

	/** 끄기 키 기다리기 (키가 눌릴 때마다 한 번. 너무 이르면 다시 기다림) */
	void WaitToggleInput();

	UFUNCTION()
	void OnTogglePressed(float TimeWaited);

	/** [서버] 게이지가 바뀜 → 0이면 다음 프레임에 끈다 (GE 적용 도중에 그 GE를 떼지 않게) */
	void OnGaugeChanged(const FOnAttributeChangeData& Data);

	/** 켜 둔 감소 GE (유효하면 서버에서 켜진 상태) */
	FActiveGameplayEffectHandle DrainEffectHandle;

	/** 게이지 변화 알림 */
	FDelegateHandle GaugeChangedHandle;

	/** [서버] State.Body.Dead가 붙음 → 게이지 0 (떨어질 때 = 부활은 아무것도 안 함) */
	void OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 사망 태그 알림 (받을 때 등록, 뺄 때 해제) */
	FDelegateHandle DeadTagChangedHandle;

	/** 켠 시각 (연타 방지) */
	double ActivatedTime = 0.0;

	/** 키로 껐나 (환급 여부) */
	bool bToggledOff = false;

	// ---------- 켜는 몽타주 동안 면역 ----------
	/** 면역 태그 붙이기 (켜는 몽타주를 재생할 때) */
	void AddStartImmunity();

	/** 붙였으면 떼기 (몽타주 끝, EndAbility). 루즈 태그는 개수를 세므로 한 번만 뗀다 */
	void RemoveStartImmunity();

	/** 면역 태그를 붙였나 */
	bool bStartImmunityAdded = false;
};
