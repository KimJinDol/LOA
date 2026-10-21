// LOAAttributeSet_Combat.h
// [콘텐츠] 공격하는 모든 캐릭터(모든 직업 + 보스 + 잡몹)가 공통으로 가지는 "전투 스탯창".
//
// [Common과 나눈 이유]
//   Common : 맞을 수 있는 데 필요한 것 (체력) + 데미지 입구(메타 Damage). 허수아비도 가짐
//   Combat : 공격하는 데 필요한 것 (공격력, 나중에 치명타 확률, 방어력 …). 공격하지 않는 허수아비는 안 가짐
//   전투 스탯은 계속 늘어나므로 Common에 다 넣으면 비대해진다.
//
// [로아로 치면]
// 캐릭터 정보창의 공격력. (나중에 치명, 특화, 신속 같은 전투 특성도 여기에)
//
// [데미지 계산과의 관계]
// ULOADamageExecution(ExecCalc)이 "공격자"의 AttackPower를 읽어서 스킬의 DamageRate를 곱한다.
//   최종 데미지 = 공격자 AttackPower × 스킬 DamageRate
//
// [하드코딩 금지]
// 초기값은 데이터(초기화 GE + CT_ClassStats의 Berserker.AttackPower 행)로 넣는다.
//
// [네트워크]
// - 값 변경은 서버에서만 한다. (GameplayEffect를 통해서)
// - 공격력: 본인에게만 복제 → 내 스탯창 표시용. 데미지 계산은 서버가 하므로 다른 사람은 알 필요 없다.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LOAAttributeSet_Combat.generated.h"

/** Get / Set / Init 함수 자동 생성. (LOAAttributeSet_Common.h와 같은 매크로. 먼저 include된 쪽이 정의한다) */
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

UCLASS()
class LOA_API ULOAAttributeSet_Combat : public UAttributeSet
{
	GENERATED_BODY()

public:
	ULOAAttributeSet_Combat();

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	/** 값이 바뀌기 직전. 공격력이 음수가 되지 않게 맞춘다. (디버프가 아무리 커도 0까지) */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	//~ End UAttributeSet Interface

	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Combat, AttackPower);

protected:
	// ---------- 복제 도착 알림 (클라이언트) ----------
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

private:
	/** 공격력. 스킬 데미지 = AttackPower × 스킬의 DamageRate */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData AttackPower;
};
