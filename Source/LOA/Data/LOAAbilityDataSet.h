// LOAAbilityDataSet.h
// [배관] "캐릭터에게 한 번에 넣어 줄 것들의 꾸러미" 데이터 에셋.
//
// ─────────────────────────────────────────────────────────────────────────────
// [꾸러미는 두 종류] (2026-10-19 사용자 결정: 보스 꾸러미에 InputTag 칸이 보이지 않도록 나눈다)
//
//   ULOAAbilityDataSetBase (부모, Abstract)   효과 + 스탯창 + 부여 순서
//    ├ ULOAPlayerAbilityDataSet               스킬 줄 = { 스킬, 레벨, InputTag }   ← 직업 데이터 (플레이어)
//    └ ULOAMonsterAbilityDataSet              스킬 줄 = { 스킬, 레벨 }             ← 보스, 허수아비
//
//   스킬 줄을 부모에 두지 않는 이유: 부모 줄에 InputTag가 있으면 보스 꾸러미에도 칸이 보인다.
//   자식마다 자기 줄 모양을 가지고, 부여 순서(① 스탯창 → ② 스킬 → ③ 효과)는 부모 한 곳에서 정한다.
//   "어느 키에 어느 스킬"은 플레이어 꾸러미가 정한다. 스킬(GA)은 자기 키를 모른다. (6장 입력 구조 그대로)
//
//   옛 클래스 ULOAAbilityDataSet = 지금의 ULOAPlayerAbilityDataSet. (DefaultEngine.ini ClassRedirects → 옛 에셋은 그대로 열린다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [꾸러미에 들어가는 것 3종류]
//
//   1) 스킬      : 스킬 클래스 + 스킬 레벨 (+ 플레이어만 입력 슬롯 태그)  (예: 휠윈드 → InputTag.Skill.Q)
//   2) 효과      : GameplayEffect 클래스 + 효과 레벨           (예: GE_Init_Berserker = 초기 체력/마나 세팅)
//   3) 스탯창    : AttributeSet 클래스                          (예: 아이덴티티 전용 게이지 스탯창)
//
// ─────────────────────────────────────────────────────────────────────────────
// [우리 게임에서 꾸러미를 쓰는 방식]
//
//   버서커 직업 데이터 (플레이어 꾸러미)
//    ├─ DA_AbilityDataSet_Common               모든 직업 공통 (회피, 공통 스탯 초기화)     → 항상 장착
//    ├─ DA_AbilityDataSet_Berserker            직업 일반 스킬                               → 항상 장착
//    └─ DA_AbilityDataSet_Berserker_Identity   아이덴티티 (Z/X 스킬, 전용 게이지, 패시브)    → 항상 장착
//
//   보스 BP (몬스터 꾸러미)
//    └─ DA_AbilityDataSet_Boss                 패턴 GA, 사망·스턴 GA, 초기화 GE
//
//   아이덴티티는 직업당 하나다. (각인 선택처럼 둘 중 고르는 구조는 두지 않는다. 밸런스 범위를 줄이기 위해)
//   → 셋 다 직업 데이터의 DefaultAbilityDataSets에 넣으면 한 번에 부여된다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [영수증(FLOAAbilityDataSet_GrantedHandles)이 필요한 이유]
//
// ASC 안에는 여러 꾸러미에서 온 스킬/효과/스탯창이 섞여 있다.
// "이 꾸러미로 받은 것만" 정확히 빼려면, 받을 때 무엇을 받았는지 기록해 둬야 한다.
// 그 기록이 영수증이다. 영수증에 없는 것(직업 일반 스킬 등)은 회수되지 않는다.
// (지금 직업 꾸러미는 회수할 일이 없다. 영수증은 나중에 버프·장비처럼 붙였다 떼는 꾸러미가 생기면 쓴다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [하드코딩 금지]
// 초기 체력 같은 숫자는 이 파일에 없다. 꾸러미에는 "어떤 효과를 적용할지"만 적고,
// 실제 숫자는 그 효과(GameplayEffect) 안에서 커브 테이블을 참조해 정한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - 부여(GiveToAbilitySystem)와 회수(TakeFromAbilitySystem)는 "서버 전용"이다.
//   스킬과 스탯은 게임 결과를 바꾸므로 서버만 줄 수 있다. 클라이언트에서 호출하면 경고 후 아무것도 하지 않는다.
// - 서버가 부여한 결과는 ASC가 알아서 복제한다.
//     · 스킬 목록 → 본인에게만 복제 (본인만 키로 발동하므로)
//     · 효과      → ASC 복제 모드(Mixed) 규칙대로
//     · 스탯창    → 각 스탯창의 복제 설정대로

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "LOAAbilityDataSet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UAttributeSet;

// =============================================================================
// 꾸러미의 "한 줄" 구조체들
// =============================================================================

/**
 * [플레이어 꾸러미] 스킬 한 줄: { 스킬 클래스, 스킬 레벨, 입력 슬롯 태그 }
 * (옛 이름 FLOAAbilityDataSet_GameplayAbility. DefaultEngine.ini StructRedirects)
 */
USTRUCT(BlueprintType)
struct FLOAPlayerAbilityDataSet_Ability
{
	GENERATED_BODY()

	/**
	 * 부여할 스킬 클래스.
	 * GAS 기본 타입(UGameplayAbility)으로 받는다. 우리 스킬 부모 클래스에 의존하지 않으므로 어떤 스킬이든 넣을 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet")
	TSubclassOf<UGameplayAbility> Ability;

	/** 스킬 레벨. 스킬 안에서 레벨별 수치(커브 테이블)를 읽을 때 쓰인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	/**
	 * 이 스킬을 발동할 입력 슬롯 (예: InputTag.Skill.Q, InputTag.Identity.Z)
	 * 비워 두면 "키로 발동하지 않는 스킬"이 된다. (예: 피격 반응, 사망 처리 같은 자동 스킬)
	 *
	 * 이 태그는 부여 시점에 어빌리티(Spec)의 메모지(DynamicSpecSourceTags)에 붙는다.
	 * ASC의 AbilityInputTagPressed()가 이 메모지를 보고 스킬을 찾는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * [몬스터 꾸러미] 스킬 한 줄: { 스킬 클래스, 스킬 레벨 }. 보스는 키로 발동하지 않으므로 InputTag가 없다.
 */
USTRUCT(BlueprintType)
struct FLOAMonsterAbilityDataSet_Ability
{
	GENERATED_BODY()

	/** 부여할 스킬 클래스 (패턴 GA, 사망·스턴 GA) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet")
	TSubclassOf<UGameplayAbility> Ability;

	/** 스킬 레벨 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;
};

/**
 * 효과 한 줄: { GameplayEffect 클래스, 효과 레벨 }
 */
USTRUCT(BlueprintType)
struct FLOAAbilityDataSet_GameplayEffect
{
	GENERATED_BODY()

	/**
	 * 적용할 효과 클래스.
	 * - 즉시형(Instant) : 초기 스탯 세팅 등. 한 번 적용되고 끝나므로 영수증에 남지 않고, 회수 대상도 아니다.
	 * - 지속형(Infinite / Duration) : 패시브 등. 영수증에 남아서 꾸러미 회수 시 함께 제거된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	/** 효과 레벨. 효과 안에서 커브 테이블의 몇 번째 행을 읽을지 결정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet", meta = (ClampMin = "1"))
	float EffectLevel = 1.f;
};

/**
 * 스탯창 한 줄: { AttributeSet 클래스 }
 */
USTRUCT(BlueprintType)
struct FLOAAbilityDataSet_AttributeSet
{
	GENERATED_BODY()

	/**
	 * 추가할 스탯창 클래스 (예: 아이덴티티 전용 게이지 스탯창)
	 * 공통 스탯창(ULOAAttributeSet_Common)은 PlayerState가 이미 기본으로 가지고 있으므로 여기 넣지 않는다.
	 * (넣더라도 중복을 감지해 건너뛰고 경고 로그를 남긴다)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|AbilityDataSet")
	TSubclassOf<UAttributeSet> AttributeSet;
};

// =============================================================================
// 영수증
// =============================================================================

/**
 * "이 꾸러미로 실제로 넣어 준 것들"의 기록.
 * 회수할 때 이 기록에 있는 것만 정확히 제거한다.
 *
 * 누가 보관하나: 꾸러미를 부여한 쪽이 보관한다. (예: 붙였다 뗄 꾸러미를 관리하는 부품이 그 영수증을 보관)
 * 어디서 쓰이나: 서버에서만 의미가 있다. 복제하지 않는다.
 */
USTRUCT(BlueprintType)
struct FLOAAbilityDataSet_GrantedHandles
{
	GENERATED_BODY()

public:
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	/**
	 * [서버 전용] 영수증에 적힌 것들을 ASC에서 전부 회수하고 영수증을 비운다.
	 * 회수 순서는 부여의 반대: 스킬 → 효과 → 스탯창
	 * (스탯창을 먼저 지우면, 그 스탯창을 읽는 효과/스킬이 잠깐 빈 스탯창을 보게 될 수 있으므로 마지막에 제거)
	 */
	void TakeFromAbilitySystem(UAbilitySystemComponent* ASC);

	/** 영수증이 비어 있는지 (= 아무것도 부여되지 않은 상태인지) */
	bool IsEmpty() const;

private:
	/** 부여된 어빌리티의 핸들 (어빌리티를 가리키는 번호표) */
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	/** 적용된 지속형 효과의 핸들 */
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	/** 새로 만들어 붙인 스탯창 (UPROPERTY로 잡아 두어야 GC가 임의로 지우지 않는다) */
	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

// =============================================================================
// 꾸러미 데이터 에셋
// =============================================================================

/**
 * 꾸러미의 부모. 효과·스탯창과 부여 순서를 가진다. 에셋은 자식(플레이어 / 몬스터)으로만 만든다.
 *
 * UPrimaryDataAsset이 아니라 UDataAsset인 이유:
 * 꾸러미는 항상 직업 데이터(또는 보스 BP)가 직접 참조하므로 함께 로드된다. 따로 찾거나 비동기 로드할 필요가 없다.
 */
UCLASS(Abstract, BlueprintType, Const)
class LOA_API ULOAAbilityDataSetBase : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * [서버 전용] 꾸러미 내용을 ASC에 부여한다.
	 *
	 * 처리 순서 (순서가 중요함. 이 함수 한 곳에서만 정한다):
	 *   ① 스탯창 추가  : 효과가 값을 넣을 "그릇"이 먼저 있어야 한다.
	 *   ② 스킬 부여    : 자식이 자기 줄로 부여한다 (GiveAbilities). 플레이어는 메모지에 입력 슬롯 태그도 붙인다.
	 *   ③ 효과 적용    : 초기 스탯 세팅, 패시브 등. ①의 스탯창에 값이 들어간다.
	 *
	 * @param ASC               부여 대상 ASC (플레이어는 PlayerState의 ASC)
	 * @param OutGrantedHandles 부여 결과를 기록할 영수증. 나중에 회수할 필요가 없으면 nullptr 가능
	 * @param SourceObject      "누가 준 것인지" 표시용 객체 (디버깅, 효과 출처 추적용). 없으면 nullptr
	 */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

#if WITH_EDITOR
	/**
	 * 에셋 저장 / 데이터 검증 시 호출된다. 부모는 효과·스탯창 빈 칸을 본다.
	 * 자식은 Super를 부르고 자기 스킬 줄을 본다. (빈 칸, 플레이어는 같은 꾸러미 안의 입력 슬롯 중복)
	 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** ② 자식이 자기 스킬 줄을 부여한다. 한 줄은 GiveOneAbility로. [서버] */
	virtual void GiveAbilities(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const {}

	/**
	 * 스킬 한 줄 부여 (두 자식이 같이 쓰는 도우미).
	 * @param InputTag  비어 있으면 키로 발동하지 않는 스킬 (몬스터는 항상 비움)
	 * @param Index     로그용 줄 번호
	 */
	void GiveOneAbility(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject,
		TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel, const FGameplayTag& InputTag, int32 Index) const;

private:
	/** 적용할 효과 목록 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|AbilityDataSet", meta = (TitleProperty = "GameplayEffect"))
	TArray<FLOAAbilityDataSet_GameplayEffect> GrantedEffects;

	/** 추가할 스탯창 목록 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|AbilityDataSet", meta = (TitleProperty = "AttributeSet"))
	TArray<FLOAAbilityDataSet_AttributeSet> GrantedAttributeSets;
};

/**
 * [플레이어 꾸러미] 직업 데이터의 Default Ability Data Sets에 넣는다.
 * 에디터에서 우클릭 → 기타 → 데이터 에셋 → LOAPlayerAbilityDataSet
 */
UCLASS(BlueprintType, Const)
class LOA_API ULOAPlayerAbilityDataSet : public ULOAAbilityDataSetBase
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void GiveAbilities(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const override;

private:
	/** 부여할 스킬 목록 { 스킬, 레벨, InputTag } */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|AbilityDataSet", meta = (TitleProperty = "Ability"))
	TArray<FLOAPlayerAbilityDataSet_Ability> GrantedAbilities;
};

/**
 * [몬스터 꾸러미] 보스·허수아비 BP의 Ability Data Sets에 넣는다.
 * 에디터에서 우클릭 → 기타 → 데이터 에셋 → LOAMonsterAbilityDataSet
 */
UCLASS(BlueprintType, Const)
class LOA_API ULOAMonsterAbilityDataSet : public ULOAAbilityDataSetBase
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** 부여할 스킬 목록. (읽기 전용. 패턴 테스터 창이 클라에서 보스 패턴 목록을 만들 때 쓴다 — 클라에는 보스 ASC의 스킬 목록이 복제되지 않음) */
	const TArray<FLOAMonsterAbilityDataSet_Ability>& GetGrantedAbilities() const { return GrantedAbilities; }

protected:
	virtual void GiveAbilities(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const override;

private:
	/** 부여할 스킬 목록 { 스킬, 레벨 } */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|AbilityDataSet", meta = (TitleProperty = "Ability"))
	TArray<FLOAMonsterAbilityDataSet_Ability> GrantedAbilities;
};
