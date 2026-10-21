// LOAOverlayWidgetController.h
// [배관] 내 화면(Overlay)용 위젯 컨트롤러. GAS를 지켜보다가 위젯에게 알림을 쏜다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [알림 5가지]
//   OnHealthChanged(현재, 최대)                      : 체력 또는 최대 체력이 바뀜
//   OnManaChanged(현재, 최대)                        : 마나 또는 최대 마나가 바뀜
//   OnSkillCooldownStarted(InputTag, 남은 시간, 전체 시간) : 그 키의 스킬 쿨타임 시작
//   OnSkillCooldownEnded(InputTag)                   : 그 키의 스킬 쿨타임 끝
//   OnSkillSlotsChanged(슬롯 목록)                   : 슬롯에 놓인 스킬(아이콘·이름)이 바뀜
//
// 위젯은 "어떤 키(InputTag)"만 알면 된다. 쿨타임 태그(Cooldown.Berserker.Whirlwind)나 GE는 몰라도 된다.
// 예) WBP_SkillSlot(InputTag = InputTag.Skill.Q)는 InputTag가 Q인 알림만 골라서 원을 돌린다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [스킬 슬롯 아이콘 - OnSkillSlotsChanged]
//
//   ASC의 스킬 목록에서 스킬마다
//     ① InputTag가 있나 (플레이어 꾸러미가 붙인 키)                  없으면 뺀다 (사망 같은 자동 스킬)
//     ② 플레이어 GA(ULOAPlayerAbility)이고 아이콘이 있나             없으면 뺀다 (회피, 평타)
//     ③ { InputTag, 그 GA의 UI 정보(아이콘·이름·설명) } 한 줄
//   → 목록 전체를 한 번에 보낸다.
//     예) [ {Q, 휠윈드 아이콘}, {W, 길로틴 아이콘}, {Z, 폭주 아이콘} ]
//   위젯: WBP_SkillSlot(InputTag = Q)은 목록에서 Q를 찾아 아이콘을 바꾼다. 없으면 빈 슬롯.
//
//   보내는 때:
//     - 처음 (BroadcastInitialValues). 호스트는 이미 스킬이 있다
//     - 스킬 목록이 바뀔 때 (ULOAAbilitySystemComponent::OnAbilitiesChanged)
//         클라: 스킬 목록이 HUD보다 늦게 복제로 도착할 수 있다 → 도착하면 여기로
//         나중에 슬롯 이동(Q → W)도 같은 알림 → 아이콘이 저절로 따라간다
//   한 줄만 바뀌어도 목록 전체를 보낸다. (슬롯 10개 남짓, 바뀌는 일도 드묾 → 바뀐 것만 고르는 코드를 두지 않는다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [쿨타임을 어떻게 알아내나]
//
//   시작: ASC에 GE가 붙을 때마다 알림이 온다. (OnActiveGameplayEffectAddedDelegateToSelf)
//         ① 붙은 GE가 ULOAGE_Cooldown인가?            아니면 무시
//         ② 그 GE가 주는 태그 = 쿨타임 태그            (예: Cooldown.Berserker.Whirlwind)
//         ③ 그 쿨타임 태그를 쓰는 Ability 찾기 → 그 Ability Spec의 "지금" InputTag (예: InputTag.Skill.Q)
//         ④ 남은 시간 / 전체 시간을 ASC에 물어봄
//         ⑤ OnSkillCooldownStarted(Q, 남은 시간, 전체 시간)
//
//   끝:   쿨타임 태그 개수가 0이 되는 순간 알림 (RegisterGameplayTagEvent)
//         → OnSkillCooldownEnded(Q)
//         처음 보는 쿨타임 태그일 때 한 번만 등록하고, 등록 번호표를 보관했다가 Shutdown에서 해제한다.
//
// ③에서 "쿨타임이 시작될 때마다" Ability를 찾는 이유:
//   - 클라이언트에서는 Ability 목록이 HUD 생성보다 늦게 도착할 수 있다.
//     쿨타임이 시작됐다는 건 그 Ability가 이미 발동됐다는 뜻이므로, 이 시점엔 목록에 반드시 있다.
//   - 결과를 저장(캐시)하지 않는다. 나중에 스킬 슬롯 위치를 바꾸면(Q → W) 저장해 둔 답이 틀려지기 때문.
//     스킬 몇 개를 훑는 비용은 스킬을 쓸 때만 생기고 매우 작다.
//
// 남은 시간을 GE에서 읽는 이유:
//   쿨타임 시간은 Ability 설정값(CooldownDuration)이지만, 나중에 쿨감 버프가 붙으면 실제 시간이 달라진다.
//   "실제로 붙은 GE의 시간"을 읽어야 화면이 항상 맞다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - 본인 컴퓨터에만 있다. (HUD가 만든다)
// - 체력/마나: 서버가 바꾼 값이 복제로 도착하면 ASC가 알림을 준다.
// - 쿨타임: 스킬이 LocalPredicted라서 내 컴퓨터에서 먼저(예측) GE가 붙는다 → 지연 없이 바로 원이 돈다.
//
// [수명]
// HUD가 만들고 HUD가 정리한다. ASC는 PlayerState 것이라 이 컨트롤러보다 오래 살 수 있으므로,
// Shutdown에서 ASC에 걸어 둔 알림을 전부 직접 해제한다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "LOAOverlayWidgetController.generated.h"

class APlayerController;
class APlayerState;
class UAbilitySystemComponent;
struct FGameplayEffectSpec;
struct FGameplayAbilitySpec;

/** 스킬 슬롯 한 칸: 어느 키에 어떤 스킬이 놓였나 */
USTRUCT(BlueprintType)
struct FLOASkillSlotInfo
{
	GENERATED_BODY()

	/** 슬롯 키 (예: InputTag.Skill.Q) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Skill")
	FGameplayTag InputTag;

	/** 그 키에 놓인 스킬의 UI 정보 (아이콘, 이름, 설명). 스킬 GA 기본값을 복사한 것 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Skill")
	FLOASkillDisplayInfo DisplayInfo;
};

/** 체력/마나처럼 "현재 / 최대" 쌍으로 보여 주는 값의 알림 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLOAVitalChangedSignature, float, CurrentValue, float, MaxValue);

/** 스킬 쿨타임 시작 알림. 위젯은 InputTag로 자기 슬롯인지 확인한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLOASkillCooldownStartedSignature, FGameplayTag, InputTag, float, TimeRemaining, float, Duration);

/** 스킬 쿨타임 끝 알림 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOASkillCooldownEndedSignature, FGameplayTag, InputTag);

/** 슬롯에 놓인 스킬 목록 알림. 위젯은 자기 InputTag 줄을 찾는다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOASkillSlotsChangedSignature, const TArray<FLOASkillSlotInfo>&, SkillSlots);

UCLASS(Blueprintable, BlueprintType)
class LOA_API ULOAOverlayWidgetController : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 지켜볼 대상을 받고, ASC에 알림을 건다. HUD가 한 번 호출한다. [본인 컴퓨터]
	 * 아직 위젯에 값을 보내지는 않는다. (위젯이 바인딩을 끝낸 뒤 BroadcastInitialValues로 보낸다)
	 */
	void Init(APlayerController* InPlayerController, APlayerState* InPlayerState, UAbilitySystemComponent* InASC);

	/**
	 * 지금 값을 한 번 보낸다. 위젯이 처음 화면을 채우는 용도.
	 * 위젯이 OnWidgetControllerSet에서 바인딩을 끝낸 "다음"에 HUD가 호출한다.
	 */
	void BroadcastInitialValues();

	/** ASC에 걸어 둔 알림을 전부 해제한다. HUD가 사라질 때 호출 */
	void Shutdown();

	// ---------- 위젯이 바인딩할 알림 ----------
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Vital")
	FLOAVitalChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Vital")
	FLOAVitalChangedSignature OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Skill")
	FLOASkillCooldownStartedSignature OnSkillCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Skill")
	FLOASkillCooldownEndedSignature OnSkillCooldownEnded;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Skill")
	FLOASkillSlotsChangedSignature OnSkillSlotsChanged;

private:
	// ---------- 체력 / 마나 ----------
	/** ASC에 "이 스탯이 바뀌면 알려 줘"를 걸고, 해제용 번호표를 보관한다 */
	void BindAttributeChanged(const FGameplayAttribute& Attribute, void (ULOAOverlayWidgetController::*Handler)(const FOnAttributeChangeData&));

	/** Health, MaxHealth 둘 중 무엇이 바뀌어도 같은 함수로 온다. 현재/최대를 다시 읽어서 보낸다 */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleManaChanged(const FOnAttributeChangeData& Data);

	void BroadcastHealth() const;
	void BroadcastMana() const;

	// ---------- 스킬 슬롯 ----------
	/** 슬롯 목록을 새로 만들어 OnSkillSlotsChanged로 보낸다. 헤더 [스킬 슬롯 아이콘] */
	void BroadcastSkillSlots();

	/** 이 스킬이 놓인 키. 부여할 때 플레이어 꾸러미가 DynamicSpecSourceTags에 넣어 둔 InputTag. 없으면 빈 태그 */
	static FGameplayTag GetInputTagOfSpec(const FGameplayAbilitySpec& AbilitySpec);

	// ---------- 쿨타임 ----------
	/** ASC에 GE가 붙을 때마다 호출. 쿨타임 GE만 골라서 시작 알림을 보낸다 */
	void HandleEffectAdded(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle ActiveHandle);

	/** 쿨타임 태그 개수가 바뀔 때 호출(생김/사라짐). 0이 되면 끝 알림을 보낸다 */
	void HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount);

	/** 처음 보는 쿨타임 태그면 "사라짐" 알림을 건다. 이미 걸었으면 아무것도 안 함 */
	void ListenCooldownTagRemoved(const FGameplayTag& CooldownTag);

	/**
	 * 쿨타임 태그 → 지금 그 스킬이 놓인 키. (예: Cooldown.Berserker.Whirlwind → InputTag.Skill.Q)
	 * ASC의 Ability 목록에서 매번 찾는다. (슬롯 이동 대응)
	 * 못 찾으면 빈 태그. (InputTag가 없는 Ability, 예: 자동 발동 스킬)
	 */
	FGameplayTag FindInputTagForCooldownTag(const FGameplayTag& CooldownTag) const;

	/** 이 쿨타임 태그를 주는 GE들 중 가장 오래 남은 것의 남은 시간 / 전체 시간. 없으면 false */
	bool GetCooldownTime(const FGameplayTag& CooldownTag, float& OutTimeRemaining, float& OutDuration) const;

	// ---------- 지켜보는 대상 (소유하지 않음) ----------
	/** 지금은 쓰지 않지만, 나중에 레벨/이름 표시, 입력 관련 UI에서 쓴다 */
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<APlayerState> PlayerState;

	/** 실제로 알림을 거는 대상. PlayerState 소유 */
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	// ---------- 해제용 번호표 ----------
	/** 스탯 변경 알림 번호표. 해제할 때 "어느 스탯의" 번호표인지도 필요해서 같이 보관 */
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeChangedHandles;

	/** GE 붙음 알림 번호표 */
	FDelegateHandle EffectAddedHandle;

	/** 스킬 목록 바뀜 알림 번호표 (ULOAAbilitySystemComponent::OnAbilitiesChanged) */
	FDelegateHandle AbilitiesChangedHandle;

	/** 쿨타임 태그별 "생김/사라짐" 알림 번호표 */
	TMap<FGameplayTag, FDelegateHandle> CooldownTagEventHandles;
};
