// LOAOverlayWidgetController.h
// [배관] 내 화면(Overlay)용 위젯 컨트롤러. GAS를 지켜보다가 위젯에게 알림을 쏜다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [알림 7가지]
//   OnHealthChanged(현재, 최대)                      : 체력 또는 최대 체력이 바뀜
//   OnHealthBarChanged(체력 %, 보호막 끝 %, 보호막)    : 체력·최대 체력·보호막 중 하나가 바뀜 → 체력바 그리기 (아래 [체력바 · 보호막], 2026-11-07)
//   OnManaChanged(현재, 최대)                        : 마나 또는 최대 마나가 바뀜
//   OnSkillCooldownStarted(InputTag, 남은 시간, 전체 시간) : 그 키의 스킬 쿨타임 시작
//   OnSkillCooldownEnded(InputTag)                   : 그 키의 스킬 쿨타임 끝
//   OnSkillSlotsChanged(슬롯 목록)                   : 슬롯에 놓인 스킬(아이콘·이름)이 바뀜
//   OnDeadChanged(죽었나)                            : 내가 죽음 / 살아남 (State.Body.Dead)  → 부활 UI 보이기/숨기기
//   OnReviveLivesChanged(목숨, 최대)                  : 공용 부활 목숨이 바뀜 (ALOAGameState) → 개수 표시, 버튼 켜기/끄기
//   OnChargeStarted / OnChargeReleased(단계, 퍼펙트) / OnChargeCancelled : 차지 게이지 (아래 [차지 게이지], 2026-10-31)
//   OnIdentityGaugeChanged(현재, 최대)               : 아이덴티티 게이지 또는 최대가 바뀜 (아래 [아이덴티티], 2026-11-09)
//   OnIdentityActiveChanged(켜짐?)                   : 아이덴티티가 켜짐 / 꺼짐 (State.Identity.Active)
//   OnPartyChanged(파티원 컨트롤러 목록)             : 파티원이 들어옴 / 나감 (아래 [파티], 2026-11-10)
//
// [부활 UI가 부를 함수]
//   RequestRevive()                  : 부활 버튼 → 서버에 요청 (ALOAPlayerController)
//   GetSecondsUntilNextReviveLife()  : 다음 목숨까지 남은 초 (-1 = 멈춤). 위젯 Tick이나 타이머에서 불러 글자로
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
//     ④ 툴팁 숫자 (아래 [툴팁 숫자])
//     ③④는 LOATooltipFormat::FillSkillSlotInfo 하나 (로비 스킬 목록과 같이 씀, 2026-11-14 apply113)
//   → 목록 전체를 한 번에 보낸다.
//     예) [ {Q, 휠윈드 아이콘}, {W, 길로틴 아이콘}, {Z, 폭주 아이콘} ]
//   위젯: WBP_SkillSlot(InputTag = Q)은 목록에서 Q를 찾아 아이콘을 바꾼다. 없으면 빈 슬롯.
//         보여 주기는 부모 C++ ULOASkillSlotWidget: 찾으면 ShowSkill(그 줄) = 아이콘 + 툴팁, 없으면 ShowEmpty (2026-11-15 apply114)
//
//   보내는 때:
//     - 처음 (BroadcastInitialValues). 호스트는 이미 스킬이 있다
//     - 스킬 목록이 바뀔 때 (ULOAAbilitySystemComponent::OnAbilitiesChanged)
//         클라: 스킬 목록이 HUD보다 늦게 복제로 도착할 수 있다 → 도착하면 여기로
//         나중에 슬롯 이동(Q → W)도 같은 알림 → 아이콘이 저절로 따라간다
//     - 내 공격력 BaseValue가 바뀔 때 ({Damage}가 바뀜. 버프로 현재 값만 바뀌면 보내지 않음)
//   한 줄만 바뀌어도 목록 전체를 보낸다. (슬롯 10개 남짓, 바뀌는 일도 드묾 → 바뀐 것만 고르는 코드를 두지 않는다)
//
// [툴팁 숫자] (2026-11-14, 사용자 결정 11-13·11-14)
//   레벨 = 그 스킬 Spec의 레벨
//   Cooldown = GA.GetCooldownAtLevel / ManaCost = GA.GetManaCostAtLevel
//   설명의 자리표시를 채워 FilledDescription에 (엔진 FText::Format 이름 자리표시)
//     {Cooldown} 쿨 초    {Mana} 마나
//     {Damage}   공격력 BaseValue × 첫 DamageRate (GA.FindDamageRate) = 한 번 맞을 때, 아무것도 안 붙은 값
//   예) 공격력 1000, 휠윈드 DamageRate 2.5 → {Damage} = 2,500 (공격력 +10% 버프 중에도 2,500)
//   값이 없는 자리표시는 빈칸 (데미지 없는 버프 스킬의 {Damage}, 공격력 스탯창이 없음)
//   BaseValue를 쓰는 이유: 초기화 GE(GE_Init_*)는 Instant + Override → BaseValue. 버프 GE(HasDuration)는 현재 값만 바꾼다
//     BaseValue도 복제된다 (FGameplayAttributeData의 두 칸 모두 UPROPERTY) → 내 컴퓨터에서 읽을 수 있다 (공격력은 본인에게만 복제)
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
// [차지 게이지] (2026-10-31, 사용자 결정: 화면 하단 가운데, 막대 + 퍼펙트 존, 단계마다 0부터 다시, 현재 단계는 게이지 왼쪽 위)
//   차지 GA(ULOAGA_Charge)가 조종하는 사람 컴퓨터에서 ASC로 보내는 신호를 받는다 (OptionalObject = 그 GA)
//     Event.Skill.Charge.Started   → 숫자(FLOAChargeGaugeInfo)와 시작 시각 기억 → OnChargeStarted (보이기)
//     Event.Skill.Charge.Released  → 다 채운 단계 수·퍼펙트 → OnChargeReleased(단계, 퍼펙트) (단계 잠깐 보여 주고 숨기기)
//     Event.Skill.Charge.Cancelled → OnChargeCancelled (숨기기)
//   위젯은 매 프레임 GetChargeGaugeState()로 지금 모습을 받아 그리기만 한다 (계산은 여기)
//
//   [칸 나누기] 막대 한 칸 = 단계 하나. 칸을 다 채우면 다음 단계 칸으로 넘어가고 막대는 0부터 다시
//     칸 경계 = 0, 단계 시각들. 단계가 없으면 0, 최대 차지 시간
//     마지막 단계를 채우면 마지막 칸이 꽉 찬 채 (그 뒤로는 더 눌러도 배율이 안 오름. 최대 시간에 자동 발사)
//     게이지 숫자 = 지금 채우는 단계 (1부터). 배율은 그 단계 칸을 다 채워야 붙는다 (차지 GA [차지 배율 규칙] 그대로)
//     예) 단계 [0.8, 1.6], 최대 2.0 → 칸 [0~0.8] [0.8~1.6]
//         0.4초 → 단계 1 채우는 중, 막대 50% / 1.0초 → 단계 2 채우는 중, 막대 25% / 1.7초 → 단계 2 꽉 참
//     발사 때 보여 주는 숫자 = 다 채운 단계 수 (= 배율이 붙은 단계. 차지 GA GetReachedStageCount)
//     (2026-11-01 사용자 결정: 숫자는 "도달한 단계"가 아니라 "채우는 중인 단계". 다음 단계가 없으면 그대로)
//     비율식 [1.5], 최대 2.0 → 칸 [0~1.5] 하나, 1.5초부터 꽉 참 / 단계 없음 → 칸 [0~최대] 하나
//     (2026-11-01 수정: 마지막 단계 ~ 최대 시간을 칸으로 두던 것을 뺌. 사용자: 퍼펙트 존이 마지막 단계 뒤에 있는 경우는 없음)
//   퍼펙트 존: 지금 칸에 걸쳐 있으면 칸 안의 비율(시작·끝 0~1)로 준다. 예) 1.6 ~ 1.8 → [1.6~2.0] 칸의 0 ~ 0.5
//   시각은 이 컴퓨터의 월드 시각 (차지 GA가 재는 누른 시간과 같은 쪽)
//
// ─────────────────────────────────────────────────────────────────────────────
// [체력바 · 보호막] (2026-11-07, 사용자 결정)
//   막대 두 개를 겹친다: 뒤 = 보호막 막대(흰색), 앞 = 체력 막대. 앞이 왼쪽을 덮어서 체력 끝 뒤로 보호막만 보인다
//   분모 = 최대 체력과 (체력 + 보호막) 중 큰 쪽
//     체력 % = 체력 ÷ 분모 / 보호막 끝 % = (체력 + 보호막) ÷ 분모
//   예) 최대 1000: 체력 400 + 보호막 300 → 분모 1000 → 40% / 70%
//                 체력 800 + 보호막 500 → 분모 1300 → 61.5% / 100% (넘치면 막대 전체가 줄어든다)
//                 체력 800 + 보호막 0   → 80% / 80% (보호막 막대가 체력 막대 뒤에 숨는다)
//   셋 중 하나만 바뀌어도 분모가 바뀔 수 있으므로 Health, MaxHealth, Shield 어느 쪽이 바뀌어도 다시 계산해 보낸다
//
// ─────────────────────────────────────────────────────────────────────────────
// [아이덴티티] (2026-11-09)
//   게이지: IdentityGauge / MaxIdentityGauge 어느 쪽이 바뀌어도 둘 다 다시 읽어 보낸다 (마나와 같음)
//           최대가 0이면 이 직업은 게이지가 없다 → 위젯이 숨는다 (ULOAIdentityGauge)
//   켜짐:   State.Identity.Active 개수가 0↔1이 될 때 (RegisterGameplayTagEvent, NewOrRemoved)
//           이 태그는 서버의 감소 GE가 주고, GE가 복제로 도착하면 내 컴퓨터에서도 태그가 생긴다
//
// ─────────────────────────────────────────────────────────────────────────────
// [파티] (2026-11-10, 사용자 결정: 나도 포함, 이름·직업 아이콘·체력(보호막)·사망·버프 아이콘 + 내 버프 줄은 남은 초)
//   파티원 한 명 = ULOAPartyMemberController 하나 (그 사람 ASC를 지켜봄). 이 컨트롤러가 만들고 지운다
//   ALOAGameState::OnPartyChanged (PlayerArray 들어옴·나감) → RebuildParty
//     이미 있는 사람은 컨트롤러를 그대로 두고, 새 사람만 만들고, 나간 사람은 Shutdown
//     순서 = 나 먼저, 나머지는 PlayerArray 순서 (대신 정함)
//     → OnPartyChanged(목록)
//   위젯: WBP_PartyFrame(파티 틀)은 목록 전부, WBP_MyStatusBar(내 버프 줄)는 그중 나(IsLocalPlayer)
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
#include "AbilitySystem/Abilities/Player/LOAGA_Charge.h"   // FLOAChargeGaugeInfo (값으로 가진다)
#include "LOAOverlayWidgetController.generated.h"

class APlayerController;
class APlayerState;
class UAbilitySystemComponent;
class ALOAGameState;
class ULOAPartyMemberController;
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

	/** 쿨타임 (초, 그 레벨). 0 = 없음. 헤더 [툴팁 숫자] */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Skill")
	float Cooldown = 0.f;

	/** 마나 소모 (그 레벨). 0 = 없음 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Skill")
	float ManaCost = 0.f;

	/** DisplayInfo.Description의 자리표시({Cooldown} {Mana} {Damage})를 채운 글 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Skill")
	FText FilledDescription;
};

/** 체력/마나처럼 "현재 / 최대" 쌍으로 보여 주는 값의 알림 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLOAVitalChangedSignature, float, CurrentValue, float, MaxValue);

/** 체력바 알림: 체력 막대 %, 보호막 막대 끝 % (0 ~ 1), 보호막 양 (글자용). 헤더 [체력바 · 보호막] */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLOAHealthBarChangedSignature, float, HealthPercent, float, ShieldEndPercent, float, Shield);

/** 스킬 쿨타임 시작 알림. 위젯은 InputTag로 자기 슬롯인지 확인한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLOASkillCooldownStartedSignature, FGameplayTag, InputTag, float, TimeRemaining, float, Duration);

/** 스킬 쿨타임 끝 알림 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOASkillCooldownEndedSignature, FGameplayTag, InputTag);

/** 슬롯에 놓인 스킬 목록 알림. 위젯은 자기 InputTag 줄을 찾는다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOASkillSlotsChangedSignature, const TArray<FLOASkillSlotInfo>&, SkillSlots);

/** 내가 죽음 / 살아남 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOADeadChangedSignature, bool, bIsDead);

/** 차지 게이지의 지금 모습 (GetChargeGaugeState). 위젯은 이대로 그리기만 한다. 헤더 [차지 게이지] */
USTRUCT(BlueprintType)
struct FLOAChargeGaugeState
{
	GENERATED_BODY()

	/** 차지 중인가. false면 나머지는 의미 없음 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Charge")
	bool bActive = false;

	/** 지금 채우는 단계 (게이지 왼쪽 위 숫자, 1부터). 마지막 단계를 다 채우면 그 단계 그대로 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Charge")
	int32 Stage = 0;

	/** 지금 칸의 막대 채움 (0 ~ 1). 단계를 채우면 0부터 다시 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Charge")
	float Progress = 0.f;

	/** 지금 칸에 퍼펙트 존이 걸쳐 있나 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Charge")
	bool bShowPerfectZone = false;

	/** 지금 칸 안의 퍼펙트 존 (0 ~ 1). bShowPerfectZone일 때만 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Charge")
	float PerfectZoneStart = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Charge")
	float PerfectZoneEnd = 0.f;

	/** 지금 떼면 퍼펙트인가 (막대를 반짝이게 할 때) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|Charge")
	bool bInPerfectZone = false;
};

/** 차지 시작 (게이지 보이기) / 끊김 (숨기기) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLOAChargeSignature);

/** 차지 발사: 다 채운 단계 수 (배율이 붙은 단계, 0 = 하나도 못 채움), 퍼펙트 존 안이었나 (잠깐 보여 주고 숨기기) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLOAChargeReleasedSignature, int32, Stage, bool, bPerfect);

/** 아이덴티티 켜짐 / 꺼짐 (헤더 [아이덴티티]) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOAIdentityActiveChangedSignature, bool, bActive);

/** 파티원 목록 (나 먼저). 헤더 [파티] */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOAPartyChangedSignature, const TArray<ULOAPartyMemberController*>&, Members);

/** 공용 부활 목숨 (지금 개수, 최대) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLOAReviveLivesChangedSignature, int32, ReviveLives, int32, MaxReviveLives);

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

	/** 체력바 그리기 (체력 막대 %, 보호막 막대 끝 %, 보호막). 헤더 [체력바 · 보호막] */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Vital")
	FLOAHealthBarChangedSignature OnHealthBarChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Vital")
	FLOAVitalChangedSignature OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Skill")
	FLOASkillCooldownStartedSignature OnSkillCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Skill")
	FLOASkillCooldownEndedSignature OnSkillCooldownEnded;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Skill")
	FLOASkillSlotsChangedSignature OnSkillSlotsChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Revive")
	FLOADeadChangedSignature OnDeadChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Revive")
	FLOAReviveLivesChangedSignature OnReviveLivesChanged;

	/** 차지 시작 → 게이지 보이기 (헤더 [차지 게이지]) */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Charge")
	FLOAChargeSignature OnChargeStarted;

	/** 차지 발사 → 다 채운 단계 수(·퍼펙트) 잠깐 보여 주고 숨기기 */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Charge")
	FLOAChargeReleasedSignature OnChargeReleased;

	/** 차지 중 끊김 (회피·CC) → 게이지 숨기기 */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Charge")
	FLOAChargeSignature OnChargeCancelled;

	/** 아이덴티티 게이지 (현재, 최대). 최대 0 = 게이지 없는 직업 (헤더 [아이덴티티]) */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Identity")
	FLOAVitalChangedSignature OnIdentityGaugeChanged;

	/** 아이덴티티 켜짐 / 꺼짐 */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Identity")
	FLOAIdentityActiveChangedSignature OnIdentityActiveChanged;

	/** 파티원 목록 (나 먼저). 헤더 [파티] */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Party")
	FLOAPartyChangedSignature OnPartyChanged;

	// ---------- 위젯이 부를 함수 ----------
	/** 부활 버튼. 서버에 요청만 한다. 되는지는 서버가 정한다 */
	UFUNCTION(BlueprintCallable, Category = "LOA|UI|Revive")
	void RequestRevive();

	/** 다음 공용 목숨까지 남은 초. 멈춰 있으면(전투 전, 가득 참) -1 */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Revive")
	float GetSecondsUntilNextReviveLife() const;

	/** 차지 게이지의 지금 모습. 위젯 Tick에서 불러 그대로 그린다 (헤더 [차지 게이지]) */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Charge")
	FLOAChargeGaugeState GetChargeGaugeState() const;

	/** 체력·최대 체력·보호막 → 체력 막대 %, 보호막 막대 끝 % (헤더 [체력바 · 보호막]) */
	static void ComputeHealthBar(float Health, float MaxHealth, float Shield, float& OutHealthPercent, float& OutShieldEndPercent);

	/** 누른 시간 → 게이지 모습 (칸 나누기 규칙). 차지 중이 아니어도 계산만 한다 */
	static FLOAChargeGaugeState ComputeChargeGaugeState(const FLOAChargeGaugeInfo& Info, float HeldTime);

private:
	// ---------- 체력 / 마나 ----------
	/** ASC에 "이 스탯이 바뀌면 알려 줘"를 걸고, 해제용 번호표를 보관한다 */
	void BindAttributeChanged(const FGameplayAttribute& Attribute, void (ULOAOverlayWidgetController::*Handler)(const FOnAttributeChangeData&));

	/** Health, MaxHealth, Shield 중 무엇이 바뀌어도 같은 함수로 온다. 다시 읽어서 보낸다 (OnHealthChanged + OnHealthBarChanged) */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleManaChanged(const FOnAttributeChangeData& Data);

	void BroadcastHealth() const;
	void BroadcastMana() const;

	// ---------- 스킬 슬롯 ----------
	/** 슬롯 목록을 새로 만들어 OnSkillSlotsChanged로 보낸다. 헤더 [스킬 슬롯 아이콘] */
	void BroadcastSkillSlots();

	/** 이 스킬이 놓인 키. 부여할 때 플레이어 꾸러미가 DynamicSpecSourceTags에 넣어 둔 InputTag. 없으면 빈 태그 */
	static FGameplayTag GetInputTagOfSpec(const FGameplayAbilitySpec& AbilitySpec);

	/** 지금 내 공격력 BaseValue. 공격력 스탯창이 없으면 0 */
	float ReadAttackPowerBase() const;

	/** 공격력이 바뀜 → BaseValue가 바뀌었을 때만 슬롯 목록을 다시 보낸다 */
	void HandleAttackPowerChanged(const FOnAttributeChangeData& Data);

	/** 마지막으로 슬롯 목록에 쓴 공격력 BaseValue */
	float LastAttackPowerBase = 0.f;

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

	// ---------- 아이덴티티 ----------
	void HandleIdentityGaugeChanged(const FOnAttributeChangeData& Data);
	void HandleIdentityActiveTagChanged(const FGameplayTag Tag, int32 NewCount);
	void BroadcastIdentityGauge() const;
	void BroadcastIdentityActive() const;

	// ---------- 사망 / 부활 ----------
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	void BroadcastDead() const;

	/** GameState 알림(인자 없음) → 지금 값을 다시 읽어 위젯 알림으로 */
	void BroadcastReviveLives() const;

	// ---------- 파티 (헤더 [파티]) ----------
	/** PlayerArray에 맞춰 파티원 컨트롤러를 만들고 지운다 → OnPartyChanged */
	void RebuildParty();

	/** 파티원 컨트롤러 전부 Shutdown */
	void ClearParty();

	/** 파티원 컨트롤러 (나 먼저). 이 컨트롤러가 소유 (HUD → 이 컨트롤러 → 파티원 컨트롤러로 GC가 따라감) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULOAPartyMemberController>> PartyMembers;

	/** ALOAGameState::OnPartyChanged 번호표 */
	FDelegateHandle PartyChangedHandle;

	// ---------- 차지 게이지 ----------
	/** 차지 GA의 신호 (Started / Released / Cancelled). 헤더 [차지 게이지] */
	void HandleChargeEvent(const FGameplayEventData* Payload);

	/** 이 컴퓨터의 지금 월드 시각 (초). 월드가 없으면 0 */
	double GetLocalTimeSeconds() const;

	/** 차지 중인가 / 그 차지의 숫자 / 시작한 월드 시각 */
	bool bCharging = false;
	FLOAChargeGaugeInfo ChargeInfo;
	double ChargeStartTime = 0.0;

	/** 차지 신호 알림 번호표 (태그별) */
	TMap<FGameplayTag, FDelegateHandle> ChargeEventHandles;

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

	/** State.Body.Dead 생김/사라짐 알림 번호표 */
	FDelegateHandle DeadTagChangedHandle;

	/** State.Identity.Active 생김/사라짐 알림 번호표 */
	FDelegateHandle IdentityActiveTagChangedHandle;

	/** 공용 부활 목숨이 있는 곳 (모든 컴퓨터에 하나) */
	TWeakObjectPtr<ALOAGameState> RaidGameState;

	/** ALOAGameState::OnReviveLivesChanged 번호표 */
	FDelegateHandle ReviveLivesChangedHandle;
};
