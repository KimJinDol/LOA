// LOAAttributeSet_Common.h
// [콘텐츠] 맞을 수 있는 모든 캐릭터(모든 직업 + 보스 + 허수아비)가 공통으로 가지는 스탯창. 체력 + 데미지 입구.
//
// [로아로 치면]
// 캐릭터 정보창의 체력.
//
// [스탯창 나누는 기준 = "누가 가지는가"]
//   Common   (이 파일) : 맞을 수 있는 모두       체력, 최대 체력, Damage(메타)
//   Player             : 플레이어만             마나, 최대 마나, 마나 회복량, 낙인력 (LOAAttributeSet_Player.h, 옛 Mana)
//   Combat             : 공격하는 모두           공격력 …                     (LOAAttributeSet_Combat.h)
//   직업 전용 (나중에)  : 그 직업만               아이덴티티 게이지 등
// 보스에게 쓸모없는 칸(마나)이 생기지 않도록, 플레이어만 쓰는 값은 여기 넣지 않는다.
//
// [하드코딩 금지]
// 이 파일에는 초기값(예: 체력 1000)이 없다.
// 초기값은 데이터(초기화용 GameplayEffect + 테이블)로 넣는다.
//
// [네트워크]
// - 값 변경은 서버에서만 한다. (GameplayEffect를 통해서)
// - 체력: 모든 플레이어에게 복제 → 파티창, 보스 체력바 표시용
// - 보호막: 모든 플레이어에게 복제 → 파티창 (2026-11-07)
// - 받는 피해 증가: 복제하지 않음 → 데미지 계산은 서버만 한다 (화면 표시는 State.Debuff.Mark 태그로)
// - Damage(메타): 복제하지 않음 → 서버에서 잠깐 들어왔다 바로 비워지는 칸
//
// [데미지 입구 - Damage (메타 Attribute)]
// 데미지 GE(ExecCalc)가 피격자에게 숫자를 전달하는 "입구". 보관하는 값이 아니다.
// GE는 "어떤 Attribute에 얼마를 더한다"로만 출력할 수 있어서, 숫자를 받는 칸이 Attribute여야 한다.
//   ULOADamageExecution → Damage에 +900 → PostGameplayEffectExecute에서 꺼내고 즉시 0으로 비움 → Health -900
// 입구가 하나라서 보호막, 피격 알림, 사망 판정을 이 한곳에서 처리할 수 있다.
//
// [회복 / 보호막] (2026-11-07)
//   Healing (메타)        : 회복 입구. ULOAHealExecution → Healing에 +300 → 꺼내고 0으로 → Health +300 (최대 체력까지)
//   Shield                : 지금 보호막 = 살아 있는 보호막 GE(GE_Shield)들이 얹은 몫의 합. 기본값은 0으로 둔다
//                           보호막은 스킬마다 따로다 (사용자 결정 2026-11-07). 각 GE가 자기 양·시간을 가진다
//                             예) A 500(3초) + B 300(4초) = 800 → A 끝나면 GAS가 A 몫을 빼서 300
//                           각 몫 = 힐러 공격력 × ShieldRate − 깎인 양 (ULOAShieldMagnitude)
//                           데미지가 오면 보호막을 먼저: 곧 끝날 보호막 GE부터 "깎인 양"(SetByCaller.ShieldUsed)을 늘린다 → 그 몫이 줄어든다
//                             예) A 500(1초 남음) + B 300(3초 남음), 400 맞음 → A 100, B 300. 다 깎인 보호막 GE는 지운다
//                           같은 스킬로 다시 걸면 덮어쓰기 (ULOAGEComponent_ShieldOverride)
//   받는 피해 증가(낙인)는 2026-11-13 ULOAAttributeSet_Combat::IncomingDamageBonus로 옮겼다 (옛 칸은 GE_Mark를 옮긴 뒤 지움, apply108)
//   시체(IsTargetable 아님)는 회복을 받지 않는다. (보호막 GE는 판정이 시체를 고르지 않아서 안 감)
//
// [전투 알림판] (2026-11-15 apply115)
//   입구 3곳이 일어난 일을 전투 알림판(ULOACombatNoticeSubsystem)에 알리기만 한다. 누가 듣는지는 모른다 (데미지 숫자, 레이드 기록 …)
//     데미지 입구   : 들어온 데미지 + 실제로 깎인 양 (보호막 + 체력, 넘친 양 뺌) + 맞은 지점·공격 판정 기록
//     ConsumeShield : 보호막 GE마다 막은 양 + 그 GE를 건 사람
//     회복 입구     : 들어온 회복량 + 실제로 채운 양 (넘친 회복 뺌)
//
// [사망 - 이 클래스는 "알리기만" 한다]
//   PreGameplayEffectExecute  : GE 적용 직전 체력을 적어 둔다 (예: 300)
//   PostGameplayEffectExecute : 적용 후 체력이 0이면 (300 → 0) "처음 0이 됨" → 자기 ASC에 Event.Trigger.Death 이벤트
//                                이미 0이었으면 (0 → 0) 보내지 않는다. → 같은 죽음에 이벤트가 두 번 가지 않는다.
//   그다음은 이벤트를 받는 쪽의 일이다. (GA 취소, State.Body.Dead, 사망 애니 → GA_Death)
//   GA_Death가 없는 대상(허수아비)은 이벤트를 받아도 아무 일 없다 → 죽지 않는다.
//
// [시체에는 데미지를 넣지 않는다]
//   맞을 수 없는 대상(State.Body.Dead)에게 온 Damage는 버린다. → 남은 장판·도트가 시체 위에 데미지 숫자를 띄우지 않는다.
//   판단은 ULOACombatLibrary::IsTargetable 하나로 한다. (판정이 대상을 고를 때와 같은 규칙)

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LOAAttributeSet_Common.generated.h"

/**
 * 스탯마다 Get / Set / Init 함수를 자동으로 만들어 주는 매크로.
 * 예) ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Common, Health) →
 *     GetHealth(), SetHealth(), InitHealth(), GetHealthAttribute() 생성
 */
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

UCLASS()
class LOA_API ULOAAttributeSet_Common : public UAttributeSet
{
	GENERATED_BODY()

public:
	ULOAAttributeSet_Common();

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	/**
	 * 스탯 값이 바뀌기 "직전"에 호출된다.
	 * 체력이 최대 체력을 넘거나 0 아래로 내려가지 않도록 범위를 맞춘다.
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/**
	 * GameplayEffect가 적용되기 "직전"에 호출된다. [서버에서만]
	 * 지금 체력을 적어 둔다. 적용 후와 비교해서 "처음 0이 됨"을 알아내기 위해.
	 */
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;

	/**
	 * GameplayEffect(데미지, 회복 등)가 적용된 "직후"에 호출된다. [서버에서만]
	 * - Damage(메타)가 들어왔으면: 꺼내서 0으로 비우고 보호막 → Health 순으로 깎는다. (받는 쪽 처리)
	 * - Healing(메타)이 들어왔으면: 꺼내서 0으로 비우고 Health에 더한다.
	 * - 그 밖에는 최종 값을 한 번 더 범위 안으로 맞춘다.
	 * - 마지막에 체력이 "처음 0이 됨"이면 Event.Death를 보낸다.
	 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Common, Health);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Common, MaxHealth);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Common, Damage);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Common, Healing);
	ATTRIBUTE_ACCESSORS(ULOAAttributeSet_Common, Shield);

protected:
	// ---------- 복제 도착 알림 (클라이언트) ----------
	// GAS에게 "값이 바뀌었다"고 알려서 UI 갱신, 예측 보정 등이 정상 동작하게 한다.
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

private:
	/**
	 * [서버] 체력이 이번 GE로 "처음 0이 됐으면" 자기 ASC에 Event.Death를 보낸다.
	 * HealthBeforeExecute > 0 이고 지금 체력 <= 0 일 때만.
	 */
	void SendDeathEventIfJustDied(const FGameplayEffectModCallbackData& Data, float LastDamage) const;

	/**
	 * [서버] 보호막 Amount만큼 깎기. 곧 끝날 보호막 GE부터 SetByCaller.ShieldUsed를 늘린다 (2026-11-07)
	 * 다 깎인 보호막 GE는 지운다 (아이콘도 사라지게). @return 실제로 깎은 양
	 */
	float ConsumeShield(float Amount) const;

	/**
	 * GE 적용 직전 체력. PreGameplayEffectExecute가 적고 PostGameplayEffectExec\ute가 읽는다. (서버, 복제 안 함)
	 * 한 GE 적용 안에서만 의미가 있다. (Pre → Post가 같은 호출 안에서 바로 이어진다)
	 */
	float HealthBeforeExecute = 0.f;

	/** 현재 체력. 0이 되면 사망 처리는 다른 시스템이 담당한다. (이 클래스는 숫자만 관리) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "LOA|Attribute|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	/** 최대 체력 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "LOA|Attribute|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	/**
	 * [메타] 이번에 들어온 데미지. 서버에서 PostGameplayEffectExecute가 꺼내자마자 0으로 비운다.
	 * 복제하지 않는다. (ReplicatedUsing 없음)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Attribute|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Damage;

	/** [메타] 이번에 들어온 회복량. 서버에서 꺼내자마자 0으로 비운다. 복제하지 않는다 (2026-11-07) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Attribute|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Healing;

	/** 지금 보호막 (보호막 GE들의 몫의 합, 기본값 0). 데미지를 체력보다 먼저 받는다. 모두에게 복제 (파티창) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "LOA|Attribute|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Shield;
};