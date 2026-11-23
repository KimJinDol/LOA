// LOACombatNoticeSubsystem.h
// [배관] 전투 알림판. 스탯창이 "피해가 들어갔다 / 보호막이 막았다 / 회복했다"를 알리면, 구독한 쪽 모두에게 그대로 전한다. (2026-11-15 apply115)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜] 알리는 쪽은 누가 듣는지 모른다 (작업 규칙 "아래는 위를 모른다")
//   전: ULOAAttributeSet_Common → ULOACombatLibrary::NotifyDamageDealt → PC를 찾아 FindComponentByClass → 데미지 숫자 RPC
//       레이드 기록까지 붙이면 스탯창이 UI 경로와 레이드 기록을 둘 다 알게 된다
//   후: 스탯창은 이 알림판만 안다. 듣는 쪽이 스스로 구독한다 → 듣는 쪽이 늘어도 스탯창·알림판은 고치지 않는다
//   예) 회복 숫자(초록, 2026-11-16 apply116): ULOACombatRPCComponent가 "받은 사람별" 회복을 구독했다. 스탯창·알림판의 알리기는 그대로
//
// [듣는 쪽] (구독하는 곳이 늘면 여기에 적는다. 따라가기 쉽게)
//   피해 - 때린 사람별 (OnDamageDealtBy)         : ULOACombatRPCComponent (그 플레이어 PC. 자기 플레이어가 때린 것만 받아 데미지 숫자 RPC)
//   피해 - 전부 (OnDamageApplied)                 : ULOARaidRecordSubsystem (준 피해, 조력, 파티가 맞은 양)
//   보호막이 막음 (OnShieldAbsorbed)              : ULOARaidRecordSubsystem (파티 방어)
//   피해 - 받은 사람별 (OnDamageReceivedBy)       : ULOACombatRPCComponent (그 플레이어 PC. 자기 플레이어가 맞은 피해만 받아 받은 데미지 숫자 RPC, 빨강, 2026-11-16 apply118)
//   회복 - 받은 사람별 (OnHealReceivedBy)         : ULOACombatRPCComponent (그 플레이어 PC. 자기 플레이어가 받은 회복만 받아 회복 숫자 RPC, 2026-11-16)
//   회복 - 전부 (OnHealApplied)                   : ULOARaidRecordSubsystem (파티 회복)
//   피해 - 받은 사람별 (OnDamageReceivedBy, 보스)  : ALOABrawlArena (대난투 진행 중 누적·단계 게이지, 2026-11-18 apply125)
//
// [알리는 쪽] (2026-11-18 apply125) 피해 알림 정보는 MakeDamageInfo 한 곳에서 만들고, 입구가 자기 사정을 덧붙여 NotifyDamageApplied로 보낸다
//   ULOAAttributeSet_Common : 체력 입구 (Damage) / ULOAAttributeSet_Boss : 대난투 입구 (BrawlDamage, 체력은 안 깎음, bBrawlDamage = true)
//
// [때린 사람별 구독] 데미지 숫자는 때린 사람 한 명만 필요하다 → 모두에게 보내고 거르지 않고, 때린 사람으로 구독자 하나를 찾아 그쪽에만 보낸다
//   열쇠 = 알림의 Instigator (Effect Context의 Instigator. 플레이어 스킬이면 그 플레이어의 PlayerState = ASC의 주인)
// [받은 사람별 구독] (2026-11-16 apply116) 회복 숫자는 받은 사람 한 명만 필요하다 (사용자) → 같은 방식으로 받은 사람의 구독자에게만
//   받은 데미지 숫자도 같다 (2026-11-16 apply118). 피해 알림의 열쇠 = TargetASC의 주인 (알림판이 꺼낸다. 알림 정보 칸은 늘리지 않음)
//   열쇠 = 받은 쪽 ASC의 주인 (플레이어면 PlayerState). 때린 사람별 열쇠와 같은 종류라 PC 쪽은 같은 PlayerState로 둘 다 구독한다
//
// [흐름] 예) 버서커가 보스를 330 때림
//   ULOAAttributeSet_Common 데미지 입구: 체력을 깎음 → NotifyDamageApplied(때린 사람 = 버서커 PlayerState, 보스, 330 …)
//     → 버서커 PlayerState로 구독자를 찾음 → 버서커 PC의 ULOACombatRPCComponent → 숫자 330 RPC (세라스 PC는 알림을 받지 않는다)
//     → 전부 구독자: 레이드 기록 → 준 피해 +330
//   같은 프레임 안에서 듣는 쪽 함수가 바로 불린다 (지금 직접 부르는 것과 시점이 같다)
//   듣는 쪽끼리는 서로 상관없다 → 부르는 순서는 정하지 않는다 (멀티캐스트는 순서를 보장하지 않음)
//   데미지 알림은 사망 이벤트보다 먼저 → 보스를 죽인 마지막 한 방도 레이드 기록에 들어간다
//
// [알림 정보의 수명] 포인터(액터, ASC, HitResult)는 알리는 그 순간에만 믿는다. 듣는 쪽은 저장하지 않고 그 자리에서 쓴다
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1 서버만 : 전부. 스탯창 입구는 서버에서만 돈다. 클라에서 불리면 알리지 않는다
//   2 복제   : 없음 (알림판은 전하기만. 화면으로 보내는 일은 듣는 쪽이 RPC로)
//   3 각자   : 없음
//
// [월드 서브시스템] 맵마다 엔진이 하나씩 만든다. Get(this)로 꺼낸다 (ULOARaidSubsystem과 같은 방식)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Library/LOACombatTypes.h"
#include "LOACombatNoticeSubsystem.generated.h"

class UAbilitySystemComponent;
struct FHitResult;
struct FGameplayEffectModCallbackData;

/** 피해가 들어감 (데미지 입구가 체력을 깎은 뒤) */
struct FLOADamageAppliedInfo
{
	/** 보낸 사람 (Effect Context의 Instigator. 플레이어면 PlayerState) / 그 ASC (조력: 주는 피해 증가 GE를 찾는 곳) */
	AActor* Instigator = nullptr;
	const UAbilitySystemComponent* InstigatorASC = nullptr;

	/** 맞은 몸 / 맞은 쪽 ASC */
	AActor* Target = nullptr;
	const UAbilitySystemComponent* TargetASC = nullptr;

	/** 들어온 피해 전체 (보호막 몫 포함, 넘친 양 포함). 데미지 숫자에 보이는 값 */
	float IncomingDamage = 0.f;

	/** 실제로 깎인 양 (보호막 + 체력, 넘친 양 뺌). 레이드 기록. 예) 체력 200에 900 → 200 */
	float RemovedAmount = 0.f;

	/** 맞은 지점. 히트 판정이 없는 공격이나 주기 GE(도트)의 틱이면 nullptr */
	const FHitResult* HitResult = nullptr;

	/** 백 / 헤드 어택, 치명타 (ApplyPayload가 Effect Context에 적은 공격 판정 기록) */
	FLOAAttackJudgeRecord AttackJudgeRecord;

	/** 대난투 피해인가 (2026-11-18 apply125). 체력이 아니라 보스 스탯창의 대난투 입구로 들어옴. 데미지 숫자 색 (치명타 빨강 / 아니면 분홍) */
	bool bBrawlDamage = false;
};

/** 보호막 GE 하나가 피해를 막음 (ConsumeShield에서 GE마다) */
struct FLOAShieldAbsorbedInfo
{
	/** 그 보호막 GE를 보낸 사람 */
	AActor* ShieldGiver = nullptr;

	/** 보호막을 가진 몸 */
	AActor* Target = nullptr;

	/** 막은 양 */
	float Amount = 0.f;
};

/** 회복함 (회복 입구가 체력을 채운 뒤) */
struct FLOAHealAppliedInfo
{
	/** 회복 GE를 보낸 사람 */
	AActor* Healer = nullptr;

	/** 회복된 몸 */
	AActor* Target = nullptr;

	/** 회복된 쪽 ASC의 주인 (플레이어면 PlayerState). 받은 사람별 구독의 열쇠 (2026-11-16) */
	AActor* TargetOwner = nullptr;

	/** 들어온 회복량 전체 (넘친 양 포함). 나중에 회복 숫자에 보일 값 */
	float IncomingHealing = 0.f;

	/** 실제로 채운 양 (넘친 양 뺌). 레이드 기록. 예) 900/1000에서 300 → 100 */
	float HealedAmount = 0.f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FLOADamageAppliedNative, const FLOADamageAppliedInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAShieldAbsorbedNative, const FLOAShieldAbsorbedInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAHealAppliedNative, const FLOAHealAppliedInfo&);

UCLASS()
class LOA_API ULOACombatNoticeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 알림판. 게임 월드가 아니면 nullptr */
	static ULOACombatNoticeSubsystem* Get(const UObject* WorldContextObject);

	// ---------- 알리기 [서버] (스탯창만 부른다: ULOAAttributeSet_Common, 대난투 피해는 ULOAAttributeSet_Boss) ----------
	void NotifyDamageApplied(const FLOADamageAppliedInfo& Info);

	/**
	 * [서버] 데미지 입구(메타)의 GE 결과로 피해 알림 정보를 만든다. 보내지는 않는다 (2026-11-18 apply125: Common에 있던 것을 옮김)
	 *   알림에 무엇을 담을지는 여기 한 곳: 때린 사람·ASC, 맞은 몸·ASC, 맞은 지점(도트 틱이면 없음), 공격 판정 기록
	 *   부르는 쪽이 자기 입구의 사정을 덧붙여 NotifyDamageApplied로 보낸다
	 *     ULOAAttributeSet_Common (체력 입구): 그대로 / ULOAAttributeSet_Boss (대난투 입구): bBrawlDamage = true
	 * @param IncomingDamage  들어온 피해 전체 (데미지 숫자)
	 * @param RemovedAmount   실제로 깎인 양 (레이드 기록·대난투 누적). 예) 체력 200에 900 → 200
	 */
	static FLOADamageAppliedInfo MakeDamageInfo(const FGameplayEffectModCallbackData& Data, float IncomingDamage, float RemovedAmount);
	void NotifyShieldAbsorbed(const FLOAShieldAbsorbedInfo& Info);
	void NotifyHealApplied(const FLOAHealAppliedInfo& Info);

	// ---------- 구독 [서버] (헤더 [듣는 쪽]) ----------
	/** 피해 - 전부. 모든 피해 알림이 온다 */
	FLOADamageAppliedNative OnDamageApplied;
	FLOAShieldAbsorbedNative OnShieldAbsorbed;
	FLOAHealAppliedNative OnHealApplied;

	/**
	 * 피해 - 때린 사람별. Instigator가 때린 피해 알림만 오는 목록을 꺼내 준다 (없으면 새로). 헤더 [때린 사람별 구독]
	 *   구독: OnDamageDealtBy(내 PlayerState).AddUObject(this, &ThisClass::Handle…)  /  떼기: OnDamageDealtBy(내 PlayerState).Remove(핸들)
	 * @param Instigator  때린 사람 = 알림의 Instigator와 같은 것 (플레이어면 PlayerState)
	 */
	FLOADamageAppliedNative& OnDamageDealtBy(const AActor* Instigator);

	/**
	 * 회복 - 받은 사람별. Receiver가 받은 회복 알림만 오는 목록을 꺼내 준다 (없으면 새로). 헤더 [받은 사람별 구독] (2026-11-16)
	 * @param Receiver  받은 쪽 ASC의 주인 = 알림의 TargetOwner와 같은 것 (플레이어면 PlayerState)
	 */
	FLOAHealAppliedNative& OnHealReceivedBy(const AActor* Receiver);

	/**
	 * 피해 - 받은 사람별. Receiver가 맞은 피해 알림만 오는 목록을 꺼내 준다 (없으면 새로). 헤더 [받은 사람별 구독] (2026-11-16 apply118)
	 * @param Receiver  맞은 쪽 ASC의 주인 (플레이어면 PlayerState). 보스면 구독자가 없어 아무도 안 받는다
	 */
	FLOADamageAppliedNative& OnDamageReceivedBy(const AActor* Receiver);

protected:
	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

private:
	/** 서버(또는 혼자 하기)인가. 클라에서는 알리지 않는다 */
	bool IsServerWorld() const;

	/** 때린 사람 → 그 사람이 때린 피해만 듣는 쪽. 플레이어마다 한 줄 (3인 파티면 3줄). 떼도 빈 줄은 남는다 (최대 3줄이라 둠) */
	TMap<TWeakObjectPtr<const AActor>, FLOADamageAppliedNative> DamageDealtListeners;

	/** 받은 사람 → 그 사람이 받은 회복만 듣는 쪽. 위와 같은 방식 (2026-11-16) */
	TMap<TWeakObjectPtr<const AActor>, FLOAHealAppliedNative> HealReceivedListeners;

	/** 맞은 사람 → 그 사람이 맞은 피해만 듣는 쪽. 위와 같은 방식 (2026-11-16 apply118) */
	TMap<TWeakObjectPtr<const AActor>, FLOADamageAppliedNative> DamageReceivedListeners;
};
