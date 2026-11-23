// LOAAttributeSet_Boss.h
// [콘텐츠] 보스만 가지는 스탯창. 지금은 무력화 게이지. (2026-11-18 apply120)
//
// [로아로 치면]
// 보스 체력바 아래의 무력화 게이지. 무력화가 있는 스킬로 때리면 줄어들고, 다 깎으면 그로기.
// 이 프로젝트에서는 다 깎으면 "대난투"(데미지 뻥튀기 이벤트)가 시작된다 (apply122). 리스크 없는 이벤트 기믹이라 실패 벌칙은 없다.
//
// [스탯창 나누는 기준 = "누가 가지는가"] (LOAAttributeSet_Common.h 같은 줄)
//   보스만 무력화 게이지를 가진다 → Common·Combat에 넣지 않는다 (플레이어·허수아비에 쓸모없는 칸이 생기지 않게)
//   붙이는 곳: 보스 몬스터 꾸러미의 Granted Attribute Sets (ALOABossCharacter.h [스탯창])
//   무력화가 없는 보스는 이 스탯창을 안 넣으면 된다 → 게이지 UI도 최대가 0이라 숨는다
//
// [칸]
//   Stagger       : 남은 무력화 게이지. 처음 = 최대 (초기화 GE). 0 ~ MaxStagger
//   MaxStagger    : 최대 무력화 게이지
//   StaggerDamage : [메타] 무력화 입구. ULOAGE_Stagger가 여기 더하면 꺼내서 Stagger를 깎고 0으로 비운다 (Common의 Damage와 같은 방식)
//   BrawlDamage   : [메타] 대난투 입구 (2026-11-18 apply125). 대난투 진행 중(State.Brawl.Active) 피해는 데미지 계산이 Common의 Damage 대신 여기로 보낸다
//                   체력은 건드리지 않고 전투 알림판에 "피해 들어감"만 알린다 → 데미지 숫자·레이드 기록·경기장 누적
//                   체력은 원래 차원에 돌아온 뒤 경기장이 누적만큼 한 번에 깎는다 (사용자 11-18: 사망 연출은 대난투를 벗어난 뒤)
//                   예) 버서커 750 → ×2 = 1,500 → 체력 그대로, 숫자 1,500 (대난투 색), 기록 +1,500, 경기장 누적 +1,500
//
// [흐름] 예: 최대 100, 버서커 스킬 무력화 20
//   [서버] 스킬 적중 → Payloads = [ GE_Damage_Basic, LOAGE_Stagger { SetByCaller.Stagger = 20 } ]
//          → StaggerDamage +20 → PostGameplayEffectExecute: 꺼내고 0으로 → Stagger 100 → 80
//          … 20 → 0 이 되는 첫 순간 → 자기 ASC에 Event.Trigger.StaggerBroken (사망 이벤트와 같은 방식)
//          0인 동안 들어온 무력화는 버린다 (다시 채울 때까지 이벤트도 다시 안 감)
//   다시 채우기: ALOABossCharacter::RefillStagger (전멸, 대난투가 끝날 때)
//   누가 이벤트를 듣는지 모른다 (대난투 GA가 들을 예정)
//
// [하지 않는 일]
//   데미지 계산을 하지 않는다. 무력화 수치는 스킬 데이터(CT_SkillValues 행 → Payload의 SetByCaller.Stagger)
//   헤드 어택 무력화 보너스(로아 +10%)는 아직 없다 (대신 정함, 나중에)
//   차지 배율(SetByCaller.ChargeMultiplier)은 무력화에 곱하지 않는다 (대신 정함)
//
// [하드코딩 금지] 초기값 없음. 보스 초기화 GE에서 MaxStagger, Stagger를 같은 값으로 Override
//
// [네트워크]
// - 값 변경은 서버에서만 (GE로)
// - Stagger, MaxStagger: 모든 플레이어에게 복제 (보스 체력바 아래 게이지)
// - StaggerDamage, BrawlDamage(메타): 복제하지 않음

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LOAAttributeSet_Boss.generated.h"

#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

UCLASS()
class LOA_API ULOAAttributeSet_Boss : public UAttributeSet
{
	GENERATED_BODY()

public:
	ULOAAttributeSet_Boss();

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	/** 값이 바뀌기 직전: Stagger는 0 ~ 최대, MaxStagger는 0 이상 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** [서버] GE 적용 직전 Stagger를 적어 둔다 ("처음 0이 됨" 판단용. Common의 체력과 같은 방식) */
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;

	/**
	 * [서버] StaggerDamage(메타)가 들어왔으면 꺼내서 Stagger를 깎는다. 처음 0이 되면 Event.Trigger.StaggerBroken
	 * BrawlDamage(메타)가 들어왔으면 꺼내서 피해 알림만 보낸다 (체력은 그대로)
	 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Boss, Stagger);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Boss, MaxStagger);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Boss, StaggerDamage);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Boss, BrawlDamage);

protected:
	UFUNCTION()
	void OnRep_Stagger(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStagger(const FGameplayAttributeData& OldValue);

private:
	/** [서버] 이번 GE로 Stagger가 처음 0이 됐으면 자기 ASC에 Event.Trigger.StaggerBroken */
	void SendStaggerBrokenEventIfJustBroken(const FGameplayEffectModCallbackData& Data) const;

	/** GE 적용 직전 Stagger. Pre가 적고 Post가 읽는다 (서버, 복제 안 함, 한 GE 적용 안에서만 의미 있음) */
	float StaggerBeforeExecute = 0.f;

	/** 남은 무력화 게이지. 모두에게 복제 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stagger, Category = "LOA|Attribute|Boss", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Stagger;

	/** 최대 무력화 게이지. 모두에게 복제 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStagger, Category = "LOA|Attribute|Boss", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxStagger;

	/** [메타] 이번에 들어온 무력화. 서버에서 꺼내자마자 0으로 비운다. 복제하지 않는다 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Attribute|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData StaggerDamage;

	/** [메타] 이번에 들어온 대난투 피해 (2026-11-18 apply125). 서버에서 꺼내자마자 0으로 비운다. 복제하지 않는다 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Attribute|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData BrawlDamage;
};
