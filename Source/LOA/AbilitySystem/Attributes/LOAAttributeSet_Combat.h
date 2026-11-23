// LOAAttributeSet_Combat.h
// [콘텐츠] 전투에 참여하는 모든 몸(모든 직업 + 보스 + 잡몹 + 허수아비)이 가지는 "전투 스탯창".
//
// [Common과 나눈 이유] (2026-11-13 사용자 결정: 앞으로 Combat = 전투 관련 스탯창)
//   Common : 몸의 기본 (체력, 보호막) + 데미지·회복 입구(메타 Damage / Healing)
//   Combat : 전투 계산에 쓰는 것. 때리는 쪽 칸(공격력, 백·헤드·치명타 피해 추가분, 주는 피해)과
//            맞는 쪽 칸(치명타 저항, 받는 피해 증가 = 낙인)이 같이 있다
//            → 허수아비도 가진다 (데미지 계산이 맞는 쪽 칸을 캡처하므로. 없으면 엔진이 멈춘다)
//   플레이어만 쓰는 전투 값 중 데미지 계산이 캡처하지 않는 것(치명타 적중률 추가분)은 ULOAAttributeSet_Player
//   전투 스탯은 계속 늘어나므로 Common에 다 넣으면 비대해진다.
//
// [로아로 치면]
// 캐릭터 정보창의 공격력. (나중에 치명, 특화, 신속 같은 전투 특성도 여기에)
//
// [데미지 계산과의 관계]
// ULOADamageExecution(ExecCalc)이 "공격자"의 AttackPower를 읽어서 스킬의 DamageRate를 곱한다.
//   최종 데미지 = 공격자 AttackPower × 스킬 DamageRate × (1 + 백/헤드 어택 보너스) × (1 + OutgoingDamageBonus) × …
//
// [주는 피해 증가 - OutgoingDamageBonus] (2026-11-10)
//   버프로 "더해지는" 몫. 기본 0 → 초기화 GE 필요 없음. 0.15 = +15%
//   첫 사용: 세라스 아이덴티티 오라가 범위 안 아군에게 거는 GE (State.Buff.DamageDealt)
//   받는 피해 증가(낙인, 이 스탯창의 IncomingDamageBonus)와 반대쪽: 이건 때리는 쪽 값
//
// [백 / 헤드 어택 추가 피해] (사용자 결정 2026-10-25)
//   ExtraBackAttackDamage / ExtraHeadAttackDamage = 각인(기습의 대가 등)·버프로 "더해지는" 몫. 기본 0 → 초기화 GE·CurveTable 필요 없음.
//   기본 보너스(백 +5%, 헤드 +20%)는 모든 직업 공통 규칙이라 여기가 아니라 Project Settings → LOA Combat (ULOACombatSettings).
//   최종 보너스 = 설정의 기본값 + 이 칸.  예) 0.05 + 0.25(기습의 대가) = 0.30
//   그 속성이 붙은 스킬로 뒤 / 앞에서 쳤을 때만 쓴다 (판단 = ULOACombatLibrary::ApplyPayload → Effect Context, 2026-11-02).
//   (로아의 헤드 어택 무력화 +10%는 무력화가 생기면 그때. 백 어택 치명타 +10%는 2026-11-13 치명타와 같이 넣음)
//
// [치명타] (2026-11-13, 사용자 결정)
//   CritResistance  = 치명타 저항. 맞는 쪽(보스)의 값. 기본 0. 로아 "약점 노출" 디버프가 깎는다 (−0.1 → 이 보스를 때리는 플레이어 치명타 +10%p)
//                     감소 디버프로 음수가 되는 게 정상이라 0 아래로 막지 않는다
//   ExtraCritDamage = 치명타 피해 추가분 (각인·버프, 기본 0). 0.5 = +50%p. 기본 200%(LOA Combat 설정)에 더해진다
//                     데미지 계산이 공격자에게서 캡처한다 → 보스가 때릴 때도 같은 계산을 써서, 보스에게 없는 Player 스탯창에 두면 엔진이 멈춘다
//                     (백·헤드 추가분을 여기 둔 이유와 같음). 그래서 여기
//   치명타 적중률 추가분(ExtraCritRate)은 플레이어만 쓰므로 ULOAAttributeSet_Player (굴리는 곳이 캡처 없이 ASC에서 읽음)
//   기본값(적중률 70%, 피해 200%)은 모든 직업 공통이라 Project Settings → LOA Combat (Critical)
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
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Combat, ExtraBackAttackDamage);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Combat, ExtraHeadAttackDamage);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Combat, OutgoingDamageBonus);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Combat, ExtraCritDamage);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Combat, CritResistance);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Combat, IncomingDamageBonus);

protected:
	// ---------- 복제 도착 알림 (클라이언트) ----------
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExtraBackAttackDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExtraHeadAttackDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_OutgoingDamageBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExtraCritDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CritResistance(const FGameplayAttributeData& OldValue);

private:
	/** 공격력. 스킬 데미지 = AttackPower × 스킬의 DamageRate */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData AttackPower;

	/** 백 어택 추가 피해 (각인·버프로 더해지는 몫, 기본 0). 0.25 = +25%p. 기본 +5%(LOA Combat 설정)에 더해진다 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExtraBackAttackDamage, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ExtraBackAttackDamage;

	/** 헤드 어택 추가 피해 (각인·버프로 더해지는 몫, 기본 0). 기본 +20%(LOA Combat 설정)에 더해진다 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExtraHeadAttackDamage, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ExtraHeadAttackDamage;

	/** 주는 피해 증가 (버프로 더해지는 몫, 기본 0). 0.15 = +15%. 최종 데미지에 × (1 + 값) (2026-11-10) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OutgoingDamageBonus, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData OutgoingDamageBonus;

	/** 치명타 피해 추가분 (각인·버프, 기본 0). 0.5 = +50%p. 기본 200%(LOA Combat 설정)에 더해진다 (2026-11-13) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExtraCritDamage, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ExtraCritDamage;

	/**
	 * 치명타 저항 (맞는 쪽, 기본 0). 이 대상을 때리는 플레이어의 치명타 확률에서 뺀다 (2026-11-13)
	 *   약점 노출 같은 디버프가 깎는다: −0.1 → 치명타 +10%p. 음수가 정상 (0 아래로 막지 않음)
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritResistance, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData CritResistance;

	/**
	 * 받는 피해 증가 (맞는 쪽, 기본 0, 0.1 = +10%). 낙인 같은 디버프 GE가 올린다. 서버 계산에만 써서 복제하지 않는다
	 * (2026-11-13 Common에서 옮김. Common의 옛 칸은 GE_Mark를 이 칸으로 옮긴 뒤 지운다)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Attribute|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingDamageBonus;
};
