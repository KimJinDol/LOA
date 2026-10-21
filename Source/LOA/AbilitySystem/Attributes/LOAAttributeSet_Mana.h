// LOAAttributeSet_Mana.h
// [콘텐츠] 플레이어만 가지는 "마나 스탯창". 마나 / 최대 마나 / 초당 마나 회복량.
//
// [Common에서 나눈 이유]
// 스탯창은 "누가 가지는가"로 나눈다. (LOAAttributeSet_Common.h [스탯창 나누는 기준])
// 마나는 플레이어 스킬의 비용이다. 보스와 허수아비는 마나를 쓰지 않으므로 이 스탯창을 갖지 않는다.
//   만드는 곳: ALOAPlayerState 생성자 (모든 직업이 마나를 쓰므로 기본으로 만든다)
//   쓰는 곳  : ULOAPlayerAbility::CheckCost(남은 마나 확인), ULOAGE_ManaCost(마나 깎기), Overlay 위젯 컨트롤러(마나 바)
//
// [마나 자연 회복 - ManaRegen]
// 초당 마나 회복량. 이 스탯 자체는 아무 일도 하지 않는다. 회복은 GE_ManaRegen(BP)이 한다.
//   GE_ManaRegen : Infinite + Period 1초 → 1초마다 Mana에 "ManaRegen × 1"을 더한다. (계수 = Period)
//   부여         : 직업 AbilityDataSet의 GrantedEffects. 서버가 캐릭터 초기화 때 한 번 붙인다. (부활해도 다시 안 붙음)
//   초기값       : 직업 초기화 GE(GE_Init_<직업>)에서 넣는다.
// 버프/장비/레벨로 회복 속도가 바뀌면 이 스탯만 바꾸면 된다. (GE는 그대로)
//
// [하드코딩 금지]
// 이 파일에는 초기값이 없다. 초기값은 직업 초기화 GE로 넣는다.
//
// [네트워크]
// - 값 변경은 서버에서만 한다. (GameplayEffect를 통해서. 스킬 비용은 클라가 예측으로 먼저 깎고 서버가 확정)
// - 셋 다 본인에게만 복제 (COND_OwnerOnly) → 로아처럼 파티원 마나는 안 보이므로 네트워크 절약

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LOAAttributeSet_Mana.generated.h"

/** Get / Set / Init 함수 자동 생성. (LOAAttributeSet_Common.h와 같은 매크로. 먼저 include된 쪽이 정의한다) */
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

UCLASS()
class LOA_API ULOAAttributeSet_Mana : public UAttributeSet
{
	GENERATED_BODY()

public:
	ULOAAttributeSet_Mana();

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	/**
	 * 값이 바뀌기 "직전"에 호출된다. (보여지는 값 범위 맞추기)
	 *   Mana              : 0 ~ MaxMana
	 *   MaxMana, ManaRegen : 0 이상 (회복량이 음수면 마나가 계속 줄어든다)
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** GE가 적용된 "직후"에 호출된다. [서버에서만] Mana의 저장된 기본값까지 0 ~ MaxMana로 맞춘다. */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Mana, Mana);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Mana, MaxMana);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Mana, ManaRegen);

protected:
	// ---------- 복제 도착 알림 (본인 컴퓨터) ----------
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ManaRegen(const FGameplayAttributeData& OldValue);

private:
	/** 현재 마나. 스킬 사용 비용으로 소모된다. (ULOAGE_ManaCost) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "LOA|Attribute|Mana", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Mana;

	/** 최대 마나 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "LOA|Attribute|Mana", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxMana;

	/** 초당 마나 회복량. GE_ManaRegen이 1초마다 이 값만큼 Mana를 채운다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ManaRegen, Category = "LOA|Attribute|Mana", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ManaRegen;
};
