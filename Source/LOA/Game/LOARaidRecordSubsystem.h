// LOARaidRecordSubsystem.h
// [배관] 레이드 기록 담당. 전투 중에 사람마다 피해·카운터·조력·보호·회복을 더해 두었다가, 클리어 순간 결과(몫 %)와 MVP를 만든다. (2026-11-15 apply115)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] 전부 서버
//   ALOAGameState::StartBattle → BeginRecording (지난 기록을 지우고 새로 시작. 전멸 뒤 다시 도전하면 마지막 도전만 남는다)
//   전투 중: 전투 알림판(ULOACombatNoticeSubsystem)을 듣는다 (스탯창은 이 클래스를 모른다)
//     OnDamageApplied  → 맞은 쪽이 적이면 때린 사람 "준 피해" + 버프 건 사람의 "조력"
//                        맞은 쪽이 플레이어면 "파티가 맞은 양" (방어의 분모)
//     OnShieldAbsorbed → 보호막 GE를 건 사람의 "방어"
//     OnHealApplied    → 회복 GE를 보낸 사람의 "회복"
//     보스 ASC의 Event.Combat.Counter (ULOASkillModule_Counter가 보냄) → "카운터 성공" (카운터 모듈은 이 클래스를 모른다)
//   ALOAGameState 클리어 → BuildClearResult(PlayerArray) → StopRecording
//
//   예) 원래 300짜리 공격, 세라스 낙인 +10% → 330 깎음 → 버서커 준 피해 +330, 세라스 조력 +30 (RecordAssist)
//       보스가 버서커를 500 때림, 세라스 보호막이 200 막고 체력 300 깎임 → 파티가 맞은 양 +500, 세라스 방어 +200
//
// [세는 양 = 실제로 깎인 양] (대신 정함, 사용자 확인 2026-11-15)
//   넘친 양은 뺀다. 예) 마지막 한 방 900, 보스 남은 체력 200 → 200. 회복도 넘친 양은 뺀다 (900/1000에서 300 회복 → 100)
// [자기 버프는 조력에 안 셈] 그 몫은 이미 내 준 피해에 들어 있다 (대신 정함)
//
// [누구로 세나] GE를 보낸 사람(Effect Context의 Instigator). 플레이어 ASC는 PlayerState에 있어서 보통 PlayerState다.
//   몸(Pawn)이나 컨트롤러가 오면 그 PlayerState로 바꾼다 (ResolvePlayerState). 플레이어가 아니면(보스) 세지 않는다
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1 서버만 : 전부 (기록, 결과 만들기). 클라에서 불리면 아무것도 안 한다
//   2 복제   : 하지 않는다. 결과는 ALOAGameState가 클리어 순간 한 번 복제 (한 대 맞을 때마다 보내지 않음)
//   3 각자   : 없음
//
// [월드 서브시스템] 맵마다 엔진이 하나씩 만든다. Get(this)로 꺼낸다 (ULOARaidSubsystem과 같은 방식)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/LOARaidResultTypes.h"
#include "LOARaidRecordSubsystem.generated.h"

class APlayerState;
class ALOABossCharacter;
class UAbilitySystemComponent;
struct FGameplayAttribute;
struct FGameplayEventData;
struct FLOADamageAppliedInfo;
struct FLOAShieldAbsorbedInfo;
struct FLOAHealAppliedInfo;

UCLASS()
class LOA_API ULOARaidRecordSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 기록 담당. 게임 월드가 아니면 nullptr */
	static ULOARaidRecordSubsystem* Get(const UObject* WorldContextObject);

	/** [서버] 지난 기록을 지우고 기록을 시작한다 (전투 시작) */
	void BeginRecording();

	/** [서버] 기록을 멈춘다 (클리어, 전멸). 기록은 다음 BeginRecording까지 남는다 */
	void StopRecording();

	bool IsRecording() const { return bRecording; }

	/**
	 * [서버] 지금까지의 기록으로 결과를 만든다. 칸별 몫, 점수, MVP (LOARaidResultTypes.h)
	 * 걸린 시간·보스 정보는 부르는 쪽(ALOAGameState)이 채운다
	 * @param Players  결과에 넣을 사람들 (GameState의 PlayerArray). 기록이 없는 사람은 모두 0
	 */
	FLOARaidClearResult BuildClearResult(const TArray<TObjectPtr<APlayerState>>& Players) const;

	/** 보낸 사람 → PlayerState (PlayerState 그대로 / 몸 → 그 PlayerState / 컨트롤러 → 그 PlayerState). 아니면 nullptr */
	static const APlayerState* ResolvePlayerState(const AActor* Actor);

protected:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

private:
	/** 한 사람의 누적 (실제 양. 몫은 BuildClearResult가 계산) */
	struct FPlayerRecord
	{
		float Damage = 0.f;		// 보스에게 깎은 양
		int32 Counters = 0;		// 카운터 성공 횟수
		float Assist = 0.f;		// 내 버프 때문에 더 들어간 피해
		float Protect = 0.f;	// 내 보호막이 막은 양
		float Heal = 0.f;		// 내가 채운 양
	};

	/** 서버이고 기록 중일 때만 그 사람의 줄. 아니면 nullptr */
	FPlayerRecord* FindOrAddRecord(const AActor* Instigator);

	// ---------- 전투 알림판 듣기 [서버] ----------
	/** 피해: 적이 맞음 → 준 피해 + 조력 / 플레이어가 맞음 → 파티가 맞은 양. 실제로 깎인 양(RemovedAmount)으로 센다 */
	void HandleDamageApplied(const FLOADamageAppliedInfo& Info);

	/** 조력: 이 피해 중 버프 때문에 더 들어간 몫을 버프 건 사람에게 나눠 더한다 (계산 전부 여기, 예시는 cpp) */
	void RecordAssist(const FLOADamageAppliedInfo& Info, const APlayerState* AttackerState);

	/** 조력에 쓰는 버프 하나: 건 사람과 크기 */
	struct FAssistBuff
	{
		const APlayerState* Giver = nullptr;
		float Bonus = 0.f;
	};

	/**
	 * ASC에 지금 켜져 있는 버프 GE 중 Attribute를 더하기로 올리는 것을 OutBuffs에 더하고, 크기 합을 돌려준다 (apply119: 옛 람다 GatherBuffs)
	 * 꺼진(inhibited) GE와 더하기가 아닌 Modifier는 세지 않는다 (데미지 계산에 들어가지 않거나 역산 공식과 모양이 다름)
	 */
	static float CollectAssistBuffs(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, TArray<FAssistBuff>& OutBuffs);

	/** 보호막 GE 하나가 막은 양 → 그 보호막을 건 사람의 방어 */
	void HandleShieldAbsorbed(const FLOAShieldAbsorbedInfo& Info);

	/** 실제로 채운 회복량 → 회복 GE를 보낸 사람의 회복 */
	void HandleHealApplied(const FLOAHealAppliedInfo& Info);

	FDelegateHandle DamageAppliedHandle;
	FDelegateHandle ShieldAbsorbedHandle;
	FDelegateHandle HealAppliedHandle;

	/** 레이드 상황판 알림: 보스가 바뀜 → 카운터 이벤트를 새 보스에 건다 [서버] */
	UFUNCTION()
	void HandleBossChanged(ALOABossCharacter* NewBoss);

	/** 보스 ASC의 Event.Combat.Counter → 때린 사람 카운터 +1 */
	void HandleCounterEvent(const FGameplayEventData* EventData);

	/** 지금 듣고 있는 보스 ASC의 카운터 이벤트를 뗀다 */
	void UnbindCounterEvent();

	/** 사람별 누적. PlayerState가 사라지면(접속 종료) 그 줄은 결과에 안 나온다 (BuildClearResult는 PlayerArray만 본다) */
	TMap<TWeakObjectPtr<const APlayerState>, FPlayerRecord> Records;

	/** 파티 전원이 맞은 양 (보호막에 막힌 것 + 체력이 깎인 것). 방어의 분모 */
	float PartyDamageTaken = 0.f;

	bool bRecording = false;

	/** 카운터 이벤트를 건 보스 ASC와 그 핸들 */
	TWeakObjectPtr<UAbilitySystemComponent> CounterEventASC;
	FDelegateHandle CounterEventHandle;
};
