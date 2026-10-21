// LOABossCharacter.h
// [배관] 레이드 보스의 몸체. 서 있고, 맞고, (나중에) 걷고 패턴을 쓴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [플레이어와 다른 점 - ASC 위치] (허수아비와 같은 방식)
//
//   플레이어 : ASC가 PlayerState에 있다 (Owner = PlayerState, Avatar = 캐릭터) → 죽고 부활해도 스킬·스탯 유지
//   보스     : ASC가 자기 몸에 있다   (Owner = Avatar = 보스)              → 부활도, PlayerState도 없다
//
// 그래서 ALOACharacter(플레이어 몸체)를 상속하지 않고 ACharacter에서 바로 상속한다.
// ALOACharacter는 "PlayerState의 ASC를 빌려 오는 초기화 흐름"이 핵심이라 보스와 나눌 부분이 거의 없다.
// ACharacter를 쓰는 이유: 걷기(CharacterMovement), 스켈레탈 메시, 몽타주 복제를 엔진이 이미 해 준다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [스탯창]
//   Common : 체력 + 데미지 입구     (맞으니까)    생성자에서 기본으로 만든다
//   Combat : 공격력                 (때리니까)    생성자에서 기본으로 만든다. 패턴 데미지 = 보스 공격력 × 패턴 배율
//   마나는 없다. (LOAAttributeSet_Common.h [스탯창 나누는 기준])
//   무력화 게이지 등 보스 전용 스탯은 나중에 AbilityDataSet의 GrantedAttributeSets로 붙인다.
//
// [초기값 - 하드코딩 없음]
// 플레이어·허수아비와 같은 방식으로 AbilityDataSet의 초기화 GE로 넣는다.
//   BP_Boss_<이름> → Class Defaults → LOA|Boss → Ability Data Sets: DA_AbilityDataSet_Boss_<이름>
//   DA_AbilityDataSet_Boss_<이름> → Effects: GE_Init_Boss_<이름> (MaxHealth, Health, AttackPower Override)
// 패턴 GA도 나중에 같은 꾸러미에 넣는다. (InputTag 없음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   BeginPlay (서버 / 모든 클라이언트)
//     ├─ InitAbilityActorInfo(this, this)       ← 각 컴퓨터의 ASC가 "내 몸 = 나"를 알게
//     ├─ 사망 반응 부품, AnimInstance에 ASC 연결  ← State.Dead를 보고 각 컴퓨터에서 시체가 된다
//     ├─ [서버] AbilityDataSets 부여 → 초기화 GE → 체력·공격력 채움 → 복제
//     └─ RaidSubsystem에 "나 보스야" 등록       ← 각 컴퓨터의 HUD가 보스 체력바를 띄운다
//
//   State.Hidden (모든 컴퓨터, 태그 복제를 보고 각자)
//     붙음 → 보스 숨김(SetActorHiddenInGame) + 몸 막이 끔 / 떨어짐 → 다시 보임 + 몸 막이 켬
//     예) 패턴 D: 뛰어올라 사라짐 → (장판들) → 맵 중앙에 쿵. 붙이는 건 몽타주의 LOA Grant Tags 구간.
//     맞지 않는 것은 ULOACombatLibrary::IsTargetable이 같은 태그로 막는다.
//
//   State.Counterable (모든 컴퓨터, 태그 복제를 보고 각자) — 카운터 가능 구간 = 파란 빛
//     붙음 → BP 이벤트 On Counter Window Changed(true) / 떨어짐 → (false). 연출(오버레이 머티리얼, 이펙트)은 BP가 한다.
//     C++은 태그를 듣고 알리기만 한다. (판정과 같은 태그라서 판정 구간과 보이는 구간이 어긋나지 않는다)
//     붙이는 건 카운터 패턴 몽타주의 LOA Grant Tags 구간. 판정은 Counter 모듈(ULOASkillModule_Counter)
//
//   EndPlay (모든 컴퓨터)
//     └─ RaidSubsystem에서 등록 해제            ← HUD가 보스 체력바를 내린다
//
//   사망 (보스 클래스는 사망 코드를 갖지 않는다. 부품과 GA가 나눠 맡는다)
//     [서버] 체력 처음 0 → Event.Death → GA_Death(꾸러미에 넣은 GA_Death_<이름>) → 패턴 취소, State.Dead, 사망 몽타주
//     [모든 컴퓨터] State.Dead 복제 도착
//        → AnimBP: bIsDead → 시체 포즈 (ULOAAnimInstance)
//        → 사망 반응 부품: 캡슐 충돌 끄기, 이동 멈춤 → OnDied → RaidSubsystem OnBossDefeated (토벌 완료)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - 보스 액터는 복제된다. (ACharacter 기본값) 레벨에 미리 놓은 보스도, 나중에 서버가 스폰한 보스도 같다.
// - 등록은 "각 컴퓨터에서 각자" 한다. 보스 액터가 모든 컴퓨터에 존재하므로 따로 복제할 값이 없다.
// - ASC 복제 모드 Minimal: 보스 버프의 상세 정보는 아무에게도 필요 없다. 태그와 GameplayCue는 Minimal에서도 모두에게 간다.
// - 체력은 Common의 COND_None → 모든 클라이언트가 받는다. (보스 체력바)
// - 공격력은 Combat의 COND_OwnerOnly → 보스는 조종하는 플레이어가 없으므로 아무에게도 안 간다. (데미지 계산은 서버)
// - 판단(AI)은 나중에 서버의 AIController + StateTree가 한다. 지금은 엔진 기본 AIController가 붙어 가만히 서 있다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] BP_Boss_<이름> (부모: LOABossCharacter)
//   Mesh                  : 보스 스켈레탈 메시 (예: Grux), 애님 BP (부모: LOAAnimInstance. State.Dead → bIsDead 연결)
//   Capsule               : 보스 크기에 맞게 반지름/높이 (판정이 캡슐 반지름을 쓴다)
//   LOA|Boss              : Boss Display Name, Health Bar Lines, Ability Data Sets (GA_Death_<이름> 포함)
//   DeathReaction 부품    : 기본값 그대로 (충돌 끄기, 이동 멈춤)
//   Event On Counter Window Changed : 파란 빛 켜기/끄기 (예: Mesh → Set Overlay Material 파란 머티리얼 / None)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "LOABossCharacter.generated.h"

class UAbilitySystemComponent;
class ULOAAbilitySystemComponent;
class ULOAAttributeSet_Common;
class ULOAAttributeSet_Combat;
class ULOAMonsterAbilityDataSet;
class ULOADeathReactionComponent;
class ULOABodyBlockerComponent;

UCLASS(Abstract)
class LOA_API ALOABossCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALOABossCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin IAbilitySystemInterface
	/** 데미지 GE를 적용하는 쪽, 판정 함수, UI가 이 함수로 보스의 ASC를 찾는다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	/** 체력바에 보일 이름. (예: "마수군단장 발탄") */
	UFUNCTION(BlueprintPure, Category = "LOA|Boss")
	const FText& GetBossDisplayName() const { return BossDisplayName; }

	/** 체력바 줄 수. 로아의 "x123" 표시용. UI가 "최대 체력 ÷ 줄 수 = 한 줄 체력"으로 계산한다. */
	UFUNCTION(BlueprintPure, Category = "LOA|Boss")
	int32 GetHealthBarLines() const { return HealthBarLines; }

	/** 이 보스의 AbilityDataSet 목록 (BP 기본값). 클라에서도 읽을 수 있다. (패턴 테스터 창의 패턴 목록) */
	const TArray<TObjectPtr<const ULOAMonsterAbilityDataSet>>& GetAbilityDataSets() const { return AbilityDataSets; }

	/** 사망 반응 부품. RaidSubsystem이 "보스가 쓰러졌다"를 여기서 듣는다. */
	UFUNCTION(BlueprintPure, Category = "LOA|Boss")
	ULOADeathReactionComponent* GetDeathReactionComponent() const { return DeathReactionComponent; }

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** State.Hidden 개수가 0 ↔ 1 이상으로 바뀔 때 ASC가 부른다. [모든 컴퓨터] */
	void OnHiddenTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** State.Counterable 개수가 0 ↔ 1 이상으로 바뀔 때 ASC가 부른다. [모든 컴퓨터] → OnCounterWindowChanged */
	void OnCounterableTagChanged(const FGameplayTag Tag, int32 NewCount);

	/**
	 * [모든 컴퓨터, BP에서 구현] 카운터 가능 구간(파란 빛)이 열림 / 닫힘. 연출만 한다.
	 * @param bCounterWindowOpen  true = 열림(파랗게), false = 닫힘(원래대로)
	 * 예) true → Mesh의 Set Overlay Material(M_CounterGlow) / false → Set Overlay Material(None)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Boss", meta = (DisplayName = "On Counter Window Changed"))
	void OnCounterWindowChanged(bool bCounterWindowOpen);

	// ---------- 데이터 (BP 기본값에서 지정) ----------

	/** 체력바에 보일 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Boss")
	FText BossDisplayName;

	/** 체력바 줄 수 (1 이상). 1이면 줄 표시 없이 바 하나 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Boss", meta = (ClampMin = "1"))
	int32 HealthBarLines = 1;

	/** 초기화 GE, 패턴 GA·사망·스턴 GA를 담은 몬스터 꾸러미 목록. [서버] BeginPlay에서 부여 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Boss")
	TArray<TObjectPtr<const ULOAMonsterAbilityDataSet>> AbilityDataSets;

private:
	// ---------- 컴포넌트 ----------
	UPROPERTY(VisibleAnywhere, Category = "LOA|AbilitySystem")
	TObjectPtr<ULOAAbilitySystemComponent> AbilitySystemComponent;

	/** State.Dead가 붙으면 캡슐 충돌을 끄고 이동을 멈춘다. (모든 컴퓨터) */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Boss")
	TObjectPtr<ULOADeathReactionComponent> DeathReactionComponent;

	/** 몸 막이. 플레이어만 막는 두 번째 캡슐 (보스 캡슐은 Pawn을 무시해서 플레이어를 뚫고 지나간다) */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Boss")
	TObjectPtr<ULOABodyBlockerComponent> BodyBlockerComponent;

	/** 체력 칸. 하위 객체로 만들어 두면 ASC가 초기화될 때 자동 등록한다. */
	UPROPERTY()
	TObjectPtr<ULOAAttributeSet_Common> CommonAttributeSet;

	/** 공격력 칸. 패턴 데미지 계산(ULOADamageExecution)이 공격자의 공격력을 읽는다. */
	UPROPERTY()
	TObjectPtr<ULOAAttributeSet_Combat> CombatAttributeSet;

	/** State.Hidden 구독 번호표 (EndPlay에서 해제) */
	FDelegateHandle HiddenTagChangedHandle;

	/** State.Counterable 구독 번호표 (EndPlay에서 해제) */
	FDelegateHandle CounterableTagChangedHandle;
};
