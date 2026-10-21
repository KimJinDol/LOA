// LOAPlayerState.h
// [배관] 플레이어 1명의 "신분증". ASC와 스탯창을 들고 있는다.
//
// [왜 캐릭터가 아니라 여기에 ASC를 두나]
// 캐릭터(몸)는 죽거나 교체될 수 있지만 PlayerState는 플레이어가 접속해 있는 동안 계속 살아 있다.
// 그래서 스킬, 스탯, 버프가 캐릭터가 바뀌어도 그대로 유지된다.
// 캐릭터는 이 ASC에 "내가 지금 몸이야"라고 등록(Avatar)만 한다.
//
// [보스]
// 보스는 PlayerState가 없다. 보스는 자기 몸에 ASC를 직접 붙인다. (같은 ASC 클래스 재사용)
//
// [네트워크]
// - PlayerState는 모든 플레이어에게 복제된다. (파티원 체력 표시 가능)
// - ASC 복제 모드는 Mixed:
//   본인에게는 버프/디버프 상세 정보까지, 다른 플레이어에게는 필요한 최소 정보만 보낸다.
// - PlayerState는 엔진 기본 복제 빈도가 매우 낮다(초당 1회).
//   체력 등이 늦게 보이지 않도록 BP 기본값에서 Net Update Frequency를 올려서 사용한다. (코드에 숫자를 쓰지 않음)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "LOAPlayerState.generated.h"

class UAbilitySystemComponent;
class ULOAAbilitySystemComponent;
class ULOAAttributeSet_Common;
class ULOAAttributeSet_Mana;
class ULOAAttributeSet_Combat;
class ULOACharacterClassData;

UCLASS()
class LOA_API ALOAPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALOAPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin IAbilitySystemInterface
	/** GAS와 캐릭터가 ASC를 찾을 때 쓰는 표준 통로 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	/** 우리 프로젝트 전용 ASC 타입으로 받고 싶을 때 */
	UFUNCTION(BlueprintPure, Category = "LOA|PlayerState")
	ULOAAbilitySystemComponent* GetLOAAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 공통 스탯창 (체력) */
	UFUNCTION(BlueprintPure, Category = "LOA|PlayerState")
	const ULOAAttributeSet_Common* GetCommonAttributeSet() const { return CommonAttributeSet; }

	/** 마나 스탯창 (마나, 최대 마나, 마나 회복량). 플레이어만 가진다. */
	UFUNCTION(BlueprintPure, Category = "LOA|PlayerState")
	const ULOAAttributeSet_Mana* GetManaAttributeSet() const { return ManaAttributeSet; }

	/** 전투 스탯창 (공격력) */
	UFUNCTION(BlueprintPure, Category = "LOA|PlayerState")
	const ULOAAttributeSet_Combat* GetCombatAttributeSet() const { return CombatAttributeSet; }

	/**
	 * [서버 전용] 직업 데이터의 기본 AbilityDataSet(DefaultAbilityDataSets)을 ASC에 부여한다.
	 *
	 * 규칙: 맵 안에서 직업은 바뀌지 않는다. 그래서 부여는 딱 한 번만 한다.
	 *   - 처음 호출       → 부여
	 *   - 같은 직업 재호출 → 부활한 캐릭터의 요청. ASC에 이미 남아 있으므로 건너뜀
	 *   - 다른 직업 호출   → 규칙 위반. 경고 로그만 남기고 무시
	 *
	 * 기본 AbilityDataSet은 회수할 일이 없으므로 번호표를 보관하지 않는다.
	 * 아이덴티티도 DefaultAbilityDataSets 안의 꾸러미 하나라서 여기서 함께 부여된다. (직업당 하나, 교체 없음)
	 */
	void GrantClassAbilityDataSets(const ULOACharacterClassData* ClassData);

private:
	/** 기본 AbilityDataSet 부여를 마친 직업 데이터. 비어 있으면 아직 안 받음. [서버 전용, 복제 안 함] */
	UPROPERTY(Transient)
	TObjectPtr<const ULOACharacterClassData> GrantedClassData;

	/** ASC. PlayerState와 수명을 같이 한다. */
	UPROPERTY(VisibleAnywhere, Category = "LOA|AbilitySystem")
	TObjectPtr<ULOAAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * 공통 스탯창. 모든 직업이 가지므로 여기서 기본으로 만든다.
	 * 직업 전용 스탯창(폭주 게이지 등)은 여기 추가하지 않고, 직업 데이터로 런타임에 붙인다.
	 */
	UPROPERTY()
	TObjectPtr<ULOAAttributeSet_Common> CommonAttributeSet;

	/** 마나 스탯창. 모든 직업이 마나로 스킬을 쓰므로 여기서 기본으로 만든다. (보스는 없음 → Common과 분리) */
	UPROPERTY()
	TObjectPtr<ULOAAttributeSet_Mana> ManaAttributeSet;

	/** 전투 스탯창. 모든 직업이 공격하므로 Common과 같이 여기서 기본으로 만든다. */
	UPROPERTY()
	TObjectPtr<ULOAAttributeSet_Combat> CombatAttributeSet;
};