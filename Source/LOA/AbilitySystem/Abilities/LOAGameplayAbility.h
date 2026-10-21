// LOAGameplayAbility.h
// [배관] 모든 LOA Ability의 부모 클래스. (플레이어 스킬, 몬스터/보스 패턴 공통)
//
// ─────────────────────────────────────────────────────────────────────────────
// [클래스 구조]
//
//   ULOAGameplayAbility          ← 이 파일. 모든 Ability 공통             Abilities/
//    ├─ ULOAPlayerAbility        플레이어 스킬 공통 (예측 실행, 이동 정지, 커서)  Abilities/Player/
//    │    └─ ULOAGA_Dodge        회피 (모든 직업 공용)
//    │         └─ ULOAGA_WakeUp  기상기 (누워 있을 때 스페이스, 회피 움직임 재사용)
//    ├─ ULOAGA_Airborne          띄움 (플레이어 공용, 띄움 태그가 붙으면 발동, 서버만)  Abilities/Player/
//    ├─ ULOAGA_Death           사망 (플레이어, 보스 공용)                   Abilities/
//    ├─ ULOAGA_Stun              스턴 (보스 카운터, 나중에 플레이어 기절)      Abilities/
//    └─ ULOAMonsterAbility       몬스터/보스 패턴 공통 (서버만, 노릴 대상)    Abilities/Monster/
//
// 폴더 규칙: 공통 부모는 Abilities/ 바로 아래, 플레이어 전용은 Player/, 몬스터/보스 전용은 Monster/.
// (C++ 파일 위치를 옮겨도 클래스 이름이 같으면 BP 에셋은 영향 없음)
//
// 여기에는 "플레이어든 보스든 쓰는 것"만 둔다.
//   공통 기본값 : 인스턴스 정책, 죽어 있으면 발동 불가 (ActivationBlockedTags에 State.Dead)
//   공통 도우미 : 내 LOA ASC 가져오기, 위치 쪽으로 돌기(FaceLocation)
//                 다른 Ability 모두 취소(CancelOtherAbilities), 상태 태그 붙이기(ApplyStateTagsToSelf) — 사망·스턴처럼 "몸 상태" Ability가 쓴다
//   공통 동작   : 몽타주 재생 + 끝나면 Ability 종료
//   공통 규칙   : 쿨타임 (보스 패턴도 "같은 패턴 연속 사용 금지"에 쓴다)
//   공통 부품   : 스킬 모듈 (Modules) — 행동(데미지 등)을 담는 부품. 보스 패턴도 같은 모듈을 쓴다.
//
// 전투 판정/데미지는 여기 두지 않는다. → ULOACombatLibrary (Ability가 아닌 장판/투사체도 써야 하므로)
//
// ─────────────────────────────────────────────────────────────────────────────
// [기본값 설명]
//
// InstancingPolicy = InstancedPerActor
//   캐릭터 한 명당 Ability 인스턴스를 하나 만들어 두고 계속 재사용한다.
//   Ability 안에 변수(예: 콤보 단계)를 둘 수 있고, 매번 새로 만들지 않아서 가볍다.
//   Spec이 제거되면(아이덴티티 교체 등) 인스턴스도 사라진다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [쿨타임 - CooldownDuration / CooldownTags]
// GAS의 쿨타임 = "일정 시간 붙어 있다가 사라지는 GE + 그동안 붙는 태그"
//
//   Q 발동 → CommitAbility → ApplyCooldown
//              공용 GE(ULOAGE_Cooldown)에 이 Ability의 값을 넣어 나에게 적용
//                시간 : SetByCaller.Cooldown = CooldownDuration
//                태그 : CooldownTags (예: Cooldown.Berserker.Whirlwind)
//   쿨타임 동안 ASC에 Cooldown.Berserker.Whirlwind 태그가 있음
//   → 다시 누르면 GAS가 GetCooldownTags()의 태그가 ASC에 있는지 확인 → 있으면 발동 거부
//   시간이 지나면 GE와 태그가 사라짐 → 다시 발동 가능
//
// - 스킬마다 쿨타임 GE를 만들지 않는다. (직업 3 × 스킬 6 = 18개) GE는 하나, 값은 Ability BP가 가진다.
// - 쿨타임은 발동하는 순간(CommitAbility) 시작한다. 취소돼도 이미 돌고 있다.
// - CooldownDuration이 0이면 쿨타임 없음. (평타)
//
// ─────────────────────────────────────────────────────────────────────────────
// [몽타주 - Montage / PlayMontageAndEnd]
// 대부분의 Ability가 몽타주를 쓰므로 설정값과 재생 코드를 여기 한 곳에 둔다.
//   - Montage가 비어 있으면 몽타주를 쓰지 않는 Ability다. (예: 아이덴티티 교체)
//   - "언제" 재생할지는 자식이 정한다. 부모는 재생 도구만 준다.
//       평타: 커서 위치를 받은 뒤 재생 / 차지 스킬: 키를 뗀 뒤 재생 …
//   - 몽타주가 여러 개인 Ability(콤보, 차지 단계)는 자식이 자기 설정값을 따로 추가한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [스킬 모듈 - Modules]
// GA는 "몽타주를 어떻게 넘기나"(흐름)만, 모듈은 "신호가 오면 무엇을 하나"(행동)만 맡는다.
// 자세한 설명은 SkillModules/LOASkillModule.h.
//
//   시작 : 자식의 발동 흐름이 ActivateSkillModules()를 부른다. (몽타주 재생보다 먼저 → 첫 노티파이를 놓치지 않음)
//            플레이어: ULOAPlayerAbility::BeginSkill / 보스: ULOAMonsterAbility::ActivateAbility
//   종료 : EndAbility에서 이 클래스가 모든 모듈의 DeactivateModule을 부른다. (자식이 신경 쓸 것 없음)
//
//   모듈 창구 (모듈이 주인에게 묻는 것) : HasSkillModuleAuthority, MakeSkillModuleContext
//   차지 배율 창구 : GetChargeMultiplier (기본 1, 차지 GA만 덮어씀)

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"
#include "LOAGameplayAbility.generated.h"

class ULOAAbilitySystemComponent;
class ULOASkillModule;
class UAnimMontage;

UCLASS(Abstract)
class LOA_API ULOAGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 도우미 (Ability 안에서 자주 쓰는 것) ----------

	/** 이 Ability가 들어 있는 LOA ASC. 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "LOA|Ability")
	ULOAAbilitySystemComponent* GetLOAAbilitySystemComponentFromActorInfo() const;

	//~ Begin UGameplayAbility Interface
	/** 이 Ability의 쿨타임 태그. GAS가 "쿨타임 중인지" 검사할 때, UI가 남은 시간을 물을 때 쓴다. */
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	/** 공용 쿨타임 GE에 시간과 태그를 넣어 나에게 적용한다. (CommitAbility가 부름) */
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

#if WITH_EDITOR
	/**
	 * [에디터 전용] 저장할 때(또는 Validate Assets) 설정 누락을 검사한다. 게임 빌드에는 들어가지 않는다.
	 *   GA      : 쿨타임 시간은 있는데 태그 없음 / Modules에 종류를 안 고른 빈 칸
	 *   모듈    : 각 모듈의 ValidateSkillModule이 자기 칸을 검사 (이 GA의 Montage를 넘겨 노티파이도 확인)
	 * 결과는 메시지 로그에 "GA 이름 : 무엇이 문제인지"로 뜬다.
	 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End UGameplayAbility Interface

	// ---------- 모듈 창구 (모듈이 주인 Ability에게 묻는 것) ----------

	/** 서버인가. 모듈은 결과가 남는 일(데미지 등)을 이게 true일 때만 한다. */
	bool HasSkillModuleAuthority() const;

	/** 지금 발동의 실행 정보 상자 (누가, 몇 레벨로, 차지 배율). Targeting과 데미지 적용에 넘긴다. */
	FLOASkillModuleContext MakeSkillModuleContext() const;

	/**
	 * 지금 발동의 차지 배율. 기본 1 (차지 안 하는 스킬). 차지 GA(ULOAGA_Charge)만 덮어쓴다.
	 * MakeSkillModuleContext가 이 값을 상자에 넣는다 → 모든 Payload GE에 SetByCaller.ChargeMultiplier로 들어간다.
	 */
	virtual float GetChargeMultiplier() const { return 1.f; }

	/**
	 * 지금부터 이 Ability 몽타주의 EventTag 노티파이(LOA Send Gameplay Event)까지 남은 시간 (초, 재생 속도 반영).
	 * 몽타주가 재생 중이면 지금 위치부터, 아직 시작 전이면(On Skill Start 모듈) 처음부터 센다.
	 *   예) 노티파이가 1.2초 지점, 재생 속도 0.8 → 1.5
	 * 쓰는 곳: Spawn 모듈의 First Hit Event (보스 장판의 판정 시각 = 도끼가 닿는 순간)
	 * 섹션 점프나 반복 없이 처음부터 그대로 재생하는 몽타주를 가정한다.
	 * @return 몽타주가 없거나, 지금 이후에 그 노티파이가 없으면 false
	 */
	bool GetTimeUntilMontageEvent(const FGameplayTag& EventTag, float& OutSeconds) const;

#if WITH_EDITOR
	/**
	 * [에디터 전용] Montage 칸을 코드로 채운다. 스킬 만들기 도구(LOAEditor 모듈의 ULOASkillFactory)가
	 * 새 GA BP의 기본값(CDO)에 새 몽타주를 연결할 때만 쓴다. 게임 코드에서는 쓰지 않는다. (게임 빌드에는 없음)
	 * Montage가 protected라서 에디터 모듈이 직접 쓸 수 없다. 이름으로 찾는 방식(리플렉션)은 오타를 컴파일러가 못 잡으므로 이 함수를 둔다.
	 */
	void SetMontageForEditor(UAnimMontage* InMontage) { Montage = InMontage; }
#endif

protected:
	/** 끝날 때 (정상이든 취소든) 모든 모듈의 DeactivateModule을 부른다. */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// ---------- 스킬 모듈 ----------

	/**
	 * 모든 모듈의 ActivateModule을 부른다. 자식의 발동 흐름이 몽타주 재생 "전에" 한 번 부른다.
	 * 이번 발동에서 이미 불렀으면 다시 부르지 않는다.
	 */
	void ActivateSkillModules();

	/**
	 * 스킬 모듈 목록. 디테일 패널에서 [+]를 누르고 종류(Apply 등)를 고른다.
	 * Instanced: Ability 인스턴스마다 자기 복사본을 가진다. (다른 플레이어와 공유하지 않음)
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "LOA|Ability|Modules")
	TArray<TObjectPtr<ULOASkillModule>> Modules;

	// ---------- 공통 도우미 ----------

	/**
	 * 몽타주를 재생하고, 끝나면 Ability를 종료한다.
	 *   BlendOut / Completed    → 정상 종료
	 *   Interrupted / Cancelled → 취소 종료 (다른 몽타주에 끊김, 기절 등)
	 * @return Montage가 비어 있으면 false (재생하지 않음)
	 */
	bool PlayMontageAndEnd();

	/** 몸(Avatar)을 목표 위치 방향으로 돌린다. (수평 방향만, 높이 무시) */
	void FaceLocation(const FVector& TargetLocation) const;

	/**
	 * 나를 뺀 모든 Ability를 취소한다. [서버]
	 * TagsToKeep 중 하나라도 Asset Tags에 가진 Ability는 남긴다. (비어 있으면 나 말고 전부)
	 *   예) 사망·스턴: 하던 패턴·스킬·차지를 끊는다. 패시브는 TagsToKeep으로 남긴다
	 */
	void CancelOtherAbilities(const FGameplayTagContainer& TagsToKeep);

	/**
	 * 상태 태그를 GE로 나에게 붙인다. [서버] 태그는 GAS가 모든 컴퓨터에 복제한다.
	 *   Duration > 0  : 그 시간 뒤 스스로 사라짐 (ULOAGE_TimedTags)  예) 스턴 3초 → State.CrowdControl.Debuff.Stunned
	 *   Duration <= 0 : 지울 때까지 남음        (ULOAGE_InfiniteTags) 예) 사망 → State.Dead (지우는 쪽 = 부활)
	 * 발동 중(ActivateAbility 이후)에만 부른다.
	 * @return GE 번호표 (일찍 지울 때 RemoveActiveGameplayEffect). 실패하면 무효 번호표
	 */
	FActiveGameplayEffectHandle ApplyStateTagsToSelf(const FGameplayTagContainer& Tags, float Duration = 0.f);

	// ---------- 데이터 (각 Ability BP의 Class Defaults에서 조정) ----------

	/** 재생할 몽타주. 비어 있으면 몽타주를 쓰지 않는 Ability */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Montage")
	TObjectPtr<UAnimMontage> Montage;

	/** 몽타주 재생 속도. (공격 속도 스탯이 생기면 그 값으로 대체 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Montage", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.f;

	/** 쿨타임 시간 (초). 0이면 쿨타임 없음. 캐릭터 레벨과 상관없이 고정 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Cooldown", meta = (ClampMin = "0.0", Units = "Seconds"))
	float CooldownDuration = 0.f;

	/**
	 * 쿨타임 동안 ASC에 붙는 태그. 스킬마다 달라야 한다. 이름 규칙: Cooldown.<주인>.<스킬 이름>
	 * 플레이어 스킬: Cooldown.Berserker.Whirlwind / 공용 회피: Cooldown.Dodge / 보스: Cooldown.<보스 이름>.<패턴 이름>
	 * 키(Q, W)가 아니라 스킬 기준이다. 스킬 슬롯을 옮겨도 바꾸지 않는다.
	 * 여러 개를 넣으면 공유 쿨타임 (예: 이동기 두 개가 같은 태그를 함께 가짐)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Cooldown", meta = (Categories = "Cooldown"))
	FGameplayTagContainer CooldownTags;

	// ---------- 몽타주 결과 ----------
	/** 몽타주가 끝까지 재생됨 (BlendOut / Completed) → 정상 종료 */
	UFUNCTION()
	void OnMontageFinished();

	/**
	 * 몽타주가 끊김 (Interrupted / Cancelled) → 기본은 취소 종료.
	 * 자식이 덮어쓸 수 있다. 예: ULOAPlayerAbility는 후딜 중에 끊기면 정상 종료로 바꾼다.
	 * (덮어쓸 때 자식 쪽에는 UFUNCTION()을 다시 붙이지 않는다)
	 */
	UFUNCTION()
	virtual void OnMontageCancelled();

private:
	/** 모든 모듈의 DeactivateModule을 부른다. (ActivateSkillModules를 부른 발동에서만) */
	void DeactivateSkillModules(bool bWasCancelled);

	/**
	 * 지금 모듈이 켜져 있는지. (ActivateSkillModules ~ DeactivateSkillModules)
	 * EndAbility가 두 번 불려도(BlendOut 후 Completed 등) 모듈 끄기를 한 번만 하기 위해 기억한다.
	 */
	bool bSkillModulesActive = false;
};