// LOAAttributeSet_Player.h
// [콘텐츠] 플레이어만 가지는 스탯창 (2026-11-07: 옛 이름 ULOAAttributeSet_Mana)
//   마나    : 마나 / 최대 마나 / 초당 마나 회복량
//   낙인력  : 낙인 효과를 키우는 비율 (로아처럼 모든 직업이 가짐)
//   (백 / 헤드 어택 추가 피해는 ULOAAttributeSet_Combat에 둔다. 데미지 계산이 보스 공격에도 쓰여서,
//    보스에게 없는 스탯창의 스탯을 캡처하면 엔진이 멈춘다: GameplayEffect.cpp 3834·3253줄)
//
// [Common에서 나눈 이유]
// 스탯창은 "누가 가지는가"로 나눈다. (LOAAttributeSet_Common.h [스탯창 나누는 기준])
// 보스와 허수아비는 마나·낙인력을 쓰지 않으므로 이 스탯창을 갖지 않는다.
//   만드는 곳: ALOAPlayerState 생성자 (모든 직업이 가지므로 기본으로 만든다)
//   쓰는 곳  : ULOAPlayerAbility::CheckCost(남은 마나 확인), ULOAGE_ManaCost(마나 깎기), Overlay 위젯 컨트롤러(마나 바)
//              GE_Mark(시전자 낙인력. 낙인은 플레이어만 걸므로 시전자는 항상 이 스탯창을 가진다)
//
// [낙인력] (사용자 결정 2026-11-07)
//   MarkPower. 0.08 = 8%. 최종 낙인(받는 피해 증가) = 기본 낙인 × (1 + 낙인력). 합연산이 아니라 기본값에 곱해서 더함
//   예) 기본 10%, 낙인력 8% → 10% + 10% × 8% = 10.8% (0.108)
//   계산은 GE_Mark의 값 칸 (Attribute Based: 계수 0.1 × (시전자 MarkPower + 1), 엔진 FAttributeBasedFloat)
//
// [이동속도 증가 - MoveSpeedBonus] (2026-11-13)
//   0.1 = +10%. 기본 0 → 초기화 GE 필요 없음. 최대 속도 = BP 걷기 속도 × (1 + 값) (ULOACharacterMovementComponent::GetMaxSpeed)
//   첫 사용: 버서커 시너지 스킬을 쓰면 나에게 8초 (State.Buff.*). 감소는 지금 다루지 않는다 (0 아래로 안 내려감)
//   본인에게만 복제: 이동은 내 컴퓨터(예측)와 서버(확인)가 같은 값으로 계산해야 한다. 다른 사람은 위치 복제로 본다
//
// [치명타 적중률 추가분 - ExtraCritRate] (2026-11-13, 사용자 결정: 플레이어만 치명타를 내므로 여기)
//   0.1 = +10%p. 기본 0. 기본 70%(Project Settings → LOA Combat)에 더해진다. 각인·버프 몫
//   읽는 곳: ULOACombatLibrary::RollCritical (캡처 없이 그 순간 ASC 값 → 보스 공격엔 안 읽혀서 보스에게 없어도 됨)
//   보스 쪽 치명타 저항(약점 노출이 깎음)은 ULOAAttributeSet_Combat::CritResistance
//
// [마나 자연 회복 - ManaRegen]
// 초당 마나 회복량. 이 스탯 자체는 아무 일도 하지 않는다. 회복은 GE_ManaRegen(BP)이 한다.
//   GE_ManaRegen : Infinite + Period 1초 → 1초마다 Mana에 "ManaRegen × 1"을 더한다. (계수 = Period)
//   부여         : 직업 AbilityDataSet의 GrantedEffects. 서버가 캐릭터 초기화 때 한 번 붙인다. (부활해도 다시 안 붙음)
//   초기값       : 직업 초기화 GE(GE_Init_<직업>)에서 넣는다.
// 버프/장비/레벨로 회복 속도가 바뀌면 이 스탯만 바꾸면 된다. (GE는 그대로)
//
// [하드코딩 금지]
// 이 파일에는 초기값이 없다. 마나는 직업 초기화 GE로 넣는다. 낙인력은 기본 0
//
// [네트워크]
// - 값 변경은 서버에서만 한다. (GameplayEffect를 통해서. 스킬 비용은 클라가 예측으로 먼저 깎고 서버가 확정)
// - 전부 본인에게만 복제 (COND_OwnerOnly) → 로아처럼 파티원 마나는 안 보이므로 네트워크 절약. 낙인 계산은 서버

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LOAAttributeSet_Player.generated.h"

/** Get / Set / Init 함수 자동 생성. (LOAAttributeSet_Common.h와 같은 매크로. 먼저 include된 쪽이 정의한다) */
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

UCLASS()
class LOA_API ULOAAttributeSet_Player : public UAttributeSet
{
	GENERATED_BODY()

public:
	ULOAAttributeSet_Player();

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	/**
	 * 값이 바뀌기 "직전"에 호출된다. (보여지는 값 범위 맞추기)
	 *   Mana              : 0 ~ MaxMana
	 *   MaxMana, ManaRegen : 0 이상 (회복량이 음수면 마나가 계속 줄어든다)
	 *   MarkPower : 0 이상 (감소 디버프가 커도 기본 낙인보다 약해지지 않게)
	 *   IdentityGauge : 0 ~ MaxIdentityGauge / MaxIdentityGauge : 0 이상
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** GE가 적용된 "직후"에 호출된다. [서버에서만] Mana·IdentityGauge의 저장된 기본값까지 0 ~ 최대로 맞춘다. */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, Mana);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, MaxMana);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, ManaRegen);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, MarkPower);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, IdentityGauge);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, MaxIdentityGauge);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, MoveSpeedBonus);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Player, ExtraCritRate);

protected:
	// ---------- 복제 도착 알림 (본인 컴퓨터) ----------
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ManaRegen(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MarkPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_IdentityGauge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxIdentityGauge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExtraCritRate(const FGameplayAttributeData& OldValue);

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

	/** 낙인력. 0.08 = 8%. 최종 낙인 = 기본 낙인 × (1 + 낙인력) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MarkPower, Category = "LOA|Attribute|Support", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MarkPower;

	/**
	 * 아이덴티티 게이지 (2026-11-09). 스킬로 차고(ULOAPlayerAbility의 Identity Gain), 가득이면 아이덴티티를 켤 수 있다 (ULOAGA_Identity)
	 * 켜진 동안 줄어든다. 예) 최대 100, 초당 10 → 10초
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IdentityGauge, Category = "LOA|Attribute|Identity", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IdentityGauge;

	/** 아이덴티티 게이지 최대. 직업 초기화 GE가 넣는다 (0이면 아이덴티티를 켤 수 없음) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxIdentityGauge, Category = "LOA|Attribute|Identity", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxIdentityGauge;

	/** 이동속도 증가 (버프, 기본 0). 0.1 = +10%. 최대 속도 = BP 걷기 속도 × (1 + 값) (2026-11-13) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedBonus, Category = "LOA|Attribute|Movement", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MoveSpeedBonus;

	/** 치명타 적중률 추가분 (각인·버프, 기본 0). 0.1 = +10%p. 기본 70%(LOA Combat 설정)에 더해진다 (2026-11-13) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExtraCritRate, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ExtraCritRate;
};
