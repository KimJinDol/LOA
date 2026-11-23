// LOACombatJudgeManagerSubsystem.h
// [배관] 판정 매니저. "들어온 공격을 알맞은 판정 방식에 맡기고, 올라온 결과를 확정하고, 알린다." (2026-11-04)
// 전에는 판정 담당(ULOAHitJudgeSubsystem) 하나가 서버 판정·피해자 판정·저가·적용을 다 했다. 판정 방식을 따로 떼고 이 매니저가 잇는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [모양]
//
//   부르는 쪽: Apply 모듈 (스킬 타격) · 장판 액터 (장판 타격) · (나중에) 투사체
//      │
//      ▼
//   ULOACombatJudgeManagerSubsystem (이 클래스)
//      ① 맡기기 : 들어온 공격을 알맞은 판정 방식에 (서버 위치로 대상 찾기, 원격 플레이어 나누기 포함)
//      ② 확정   : 판정 방식이 올린 결과마다 (ConfirmHit)
//                   맞을 자리였고 그 플레이어에게 저가 컴포넌트가 있으면 → "막았나?" (ULOAJustGuardComponent::TryJustGuard)
//                   막음 → 효과 없음 / 맞음 → ApplyPayloads / 피함 → 아무것도 안 함
//      ③ 알리기 : 판정 시작 / 한 사람 결과 / 판정 끝 (누가 듣는지 모른다. 지금은 저가 매니저가 듣는다)
//      (클라) 판정 방식이 "내 화면에서 맞을 자리였다"를 올리면 → 저가 먼저 → 못 막았으면 CC 먼저 반응 (ReactLocal)
//      │ 맡김                       ▲ 결과만 올라온다 (판정 방식들은 이 매니저를 모른다)
//      ├──────────────────┬───────┴─────────────┐
//      ▼                  ▼                     ▼
//   서버 판정          피해자 판정            공격자 판정
//   ULOAServerJudge…   ULOAVictimJudge…       ULOAAttackJudge…
//   결과를 돌려준다    알림 OnTargetJudged    알림 OnReportJudged
//
//   판정 쪽이 저가를 아는 곳은 확정의 "막았나?" 한 줄뿐이다. "막기"는 저가 하나뿐이라 인터페이스는 만들지 않았다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [누가 판정하나]
//   스킬 타격 (Apply 모듈) : GetRole
//     공격자 판정 공격 (범위 + Enemy + 플레이어 스킬. ULOAAttackJudgeSubsystem::IsAttackerJudged)
//       서버 + 리슨 호스트 본인 = ServerJudge / 서버 + 원격 플레이어 = WaitReport / 원격 플레이어 컴퓨터 = SendReport / 그 밖 = None
//     그 밖 (보스 패턴, 아군 힐·버프, Self) : 서버 = ServerJudge / 클라 = None
//   서버가 판정하는 공격 (스킬 타격의 ServerJudge, 장판 타격) : JudgeAndApply
//     피해자 판정 (Shape + Hit Judge = Victim + 보스 모션 Animation) → 원격 플레이어는 피해자 판정, 나머지는 서버 판정
//     예고가 꽉 차는 장판 (Telegraph) → Hit Judge 칸과 상관없이 전부 서버 판정
//       예고를 "내 편도 + 여유"만큼 일찍 꽉 차게 그린다 (ULOATelegraphSubsystem [예고 앞당김]) → 서버 판정이 곧 화면
//     그 밖 → 전부 서버 판정
//
// [판정 시작 · 끝] 공격 하나마다 번호 (AttackId)
//   판정할 인원 = 서버 판정 대상 + 피해자 판정 대상 (공격자 판정은 보고의 대상 수)
//   한 사람 결과가 날 때마다 하나씩 줄이고, 0이 되면 "판정 끝". 인원이 0이면 시작하자마자 끝
//   피해자 판정 대상은 보고나 마감 뒤에 결과가 오므로 그때까지 "판정 끝"이 기다린다 (피해자 판정은 대상마다 꼭 한 번 결과를 올린다)
//   순서: "판정 시작"을 먼저 알리고 → 서버 판정 결과 확정 → 피해자 판정 요청 보내기
//     (저가 매니저가 "판정 시작"에서 누르기 마감 "닫힘"을 보낸다 → 클라에 요청보다 먼저 도착. 전과 같은 순서)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름 예] 보스 모션 공격 (저가 공격, 피해자 판정), 원격 B · 리슨 호스트 H
//   [서버] Apply 모듈 → JudgeInstantHit → ServerJudge → JudgeAndApply
//          서버 위치로 대상 찾기 → H 맞을 자리, B는 원격이라 피해자 판정으로 (HoldRemoteVictims)
//          공격 #31, 인원 2 → "판정 시작 #31" → 저가 매니저: 구간 #7의 첫 타격 → 누르기 마감, #31 집계 시작
//          서버 판정.Judge(H) → 확정: H 저가 컴포넌트 막음 → "#31 H 막음"
//          피해자 판정.SendVictimRequests(B)
//   [B]    요청 도착 → 피해자 판정.JudgeLocal: 모양 안 → ReactLocal: B 저가 컴포넌트 막음 → 성공 모션 → "막음" 보고
//   [서버] 피해자 판정: 결과 { B 맞을 자리, 기준 시각 = 판정 시각 + B 편도 } → 확정: B 서버 저가 컴포넌트 막음 → B 클라에 알림 (GA가 두 번째라 무시)
//          → "#31 B 막음" → 남은 인원 0 → "#31 판정 끝" → 저가 매니저: 바로 보스 저가 모듈에 "타격 하나 끝남: 막음 H, B"
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만  : 맡기기, 확정(저가 확인 → 적용), 알리기, 공격 번호·남은 인원
//   2. 전달    : 없음 (요청·보고는 판정 방식들이 ULOACombatRPCComponent로)
//   3. 각자    : 클라 먼저 반응 (저가 → CC)
//   이 서브시스템은 복제되지 않는다. 컴퓨터마다 따로 있다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // FLOASkillModuleContext, FLOAEffectPayload
#include "Combat/LOAHitJudgeTypes.h"                               // FLOAAttackInfo, FLOAJudgeResult, ELOAHitOutcome, ELOAHitJudgeTiming
#include "LOACombatJudgeManagerSubsystem.generated.h"

class ULOAGameplayAbility;
class ULOASkillModule;
class ULOASkillTargeting;

/** 판정 시작 (공격 번호, 공격한 보스, 공격 속성, 서버 판정 시각은 Attack 안에) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAOnJudgeStarted, const FLOAAttackInfo& /*Attack*/);

/** 한 사람 결과 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FLOAOnHitOutcome, int32 /*AttackId*/, AActor* /*Target*/, ELOAHitOutcome /*Outcome*/);

/** 판정 끝 (이 공격의 모든 대상 결과가 나옴) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAOnJudgeFinished, int32 /*AttackId*/);

/** [서버] 결과를 기다리는 공격 하나 (피해자 판정 대상이 있는 동안만 남는다) */
USTRUCT()
struct FLOAJudgingAttack
{
	GENERATED_BODY()

	/** 공격 정보 복사본 (확정할 때 Context·Payloads를 쓴다. UPROPERTY: 시전자가 사라지면 GC가 비운다) */
	UPROPERTY()
	FLOAAttackInfo Attack;

	/** 아직 결과가 안 난 대상 수 */
	int32 NumTargetsLeft = 0;
};

UCLASS()
class LOA_API ULOACombatJudgeManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 판정 매니저. 게임 월드가 아니면 nullptr */
	static ULOACombatJudgeManagerSubsystem* Get(const UObject* WorldContextObject);

	// ---------- 스킬 타격 (Apply 모듈이 부른다. 내 컴퓨터와 서버 둘 다) ----------

	/**
	 * 스킬 시작 (Apply 모듈의 SkillModuleInit). 원격 플레이어의 공격자 판정 공격이면 서버가 이 발동의 보고를 듣기 시작한다
	 * @param NumAllowedReports  이 발동에서 받을 보고 수 (= 몽타주의 그 노티파이 수. 모듈이 센다)
	 */
	void BeginAttackReports(ULOAGameplayAbility* Ability, const ULOASkillModule* Module, const ULOASkillTargeting* Targeting,
		const TArray<FLOAEffectPayload>& Payloads, int32 NumAllowedReports);

	/** 스킬 타격 판정 (Apply 모듈의 SkillModuleTriggered = 노티파이 순간). 이 컴퓨터가 할 일을 정해서 한다 (헤더 [누가 판정하나]) */
	void JudgeInstantHit(ULOAGameplayAbility* Ability, const ULOASkillModule* Module, const ULOASkillTargeting* Targeting,
		const TArray<FLOAEffectPayload>& Payloads);

	/** 스킬 끝 (Apply 모듈의 SkillModuleEnd). 보고 듣기를 끝낸다 (스킬이 끝나면 늦게 온 보고는 받지 않는다) */
	void EndAttackReports(const ULOAGameplayAbility* Ability, const ULOASkillModule* Module);

	// ---------- 서버 판정으로 시작하는 공격 (장판 액터, 스킬 타격의 ServerJudge) ----------

	/**
	 * [서버 전용] Targeting으로 대상을 판정하고, 맞은 대상에게 Payloads를 준다 (피해자 판정 대상은 보고를 받은 뒤 또는 마감에)
	 * 서버 위치로 대상 찾기 → 원격 플레이어는 피해자 판정, 나머지는 서버 판정 → 판정 시작 → 확정 → 피해자 판정 요청
	 * Context와 Payloads는 여기서 복사해 들고 있으므로, 부른 GA나 장판 액터가 먼저 사라져도 된다.
	 * @param Context   누가, 몇 레벨로. SourceActor = 모양을 놓는 액터 (캐릭터 또는 장판 액터)
	 * @param Timing    플레이어가 무엇을 보고 피하나. 예고가 꽉 차는 장판 = Telegraph(항상 서버 판정), 그 밖 = Animation
	 * @return 지금 바로 맞은 대상 수 (로그용). 서버가 아니면 0
	 */
	int32 JudgeAndApply(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
		const TArray<FLOAEffectPayload>& Payloads, ELOAHitJudgeTiming Timing);

	// ---------- 알림 (서버. 누가 듣는지 모른다. 지금은 저가 매니저가 듣는다) ----------

	FLOAOnJudgeStarted OnJudgeStarted;
	FLOAOnHitOutcome OnHitOutcome;
	FLOAOnJudgeFinished OnJudgeFinished;

protected:
	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 만든다 */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	/** 판정 방식 셋의 결과 알림을 묶는다 (판정 방식들이 먼저 만들어지게 한다) */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem Interface

private:
	/** 이 컴퓨터가 이 스킬 타격에서 맡는 일 */
	enum class ERole : uint8
	{
		None,			// 아무것도 안 함 (다른 플레이어 컴퓨터, 클라의 보스 패턴 등)
		ServerJudge,	// 서버가 판정 (보스 패턴, 리슨 호스트 본인, 아군 대상, Self)
		SendReport,		// 내 컴퓨터: 내 화면으로 판정해 보고 (공격자 판정)
		WaitReport,		// 서버: 원격 플레이어의 보고를 기다려 검사 (공격자 판정)
	};

	/** 헤더 [누가 판정하나] */
	static ERole GetRole(const ULOAGameplayAbility* Ability, const ULOASkillTargeting* Targeting);

	/** 피해자 판정 공격인가 (Shape + Hit Judge = Victim + 보스 모션). 예고가 꽉 차는 장판은 아니다 */
	static bool IsVictimJudged(const ULOASkillTargeting* Targeting, ELOAHitJudgeTiming Timing);

	// ---------- 확정 · 알리기 (서버) ----------

	/** 판정 시작: 남은 인원을 적고 "판정 시작"을 알린다. 인원이 0이면 바로 "판정 끝" */
	void BeginJudge(const FLOAAttackInfo& Attack, int32 NumTargets);

	/**
	 * 결과 하나 확정: 피함 / 막음(저가 컴포넌트가 막았나) / 맞음(ApplyPayloads)
	 * @param Attack  공격 정보 복사본 (적용 중에 목록이 바뀌어도 안전하게)
	 */
	ELOAHitOutcome ConfirmHit(const FLOAAttackInfo& Attack, const FLOAJudgeResult& Result) const;

	/** 결과 하나 확정 → "한 사람 결과" 알림 → 남은 인원 하나 줄이기 → 0이면 "판정 끝". @return 그 사람 결과 */
	ELOAHitOutcome FinishTarget(const FLOAAttackInfo& Attack, const FLOAJudgeResult& Result);

	/** [서버] 피해자 판정이 대상 하나의 결과를 올렸다 */
	void OnVictimJudged(int32 AttackId, const FLOAJudgeResult& Result);

	/** [서버] 공격자 판정이 보고 하나를 검사했다 (보고 하나 = 공격 하나) */
	void OnAttackReportJudged(const FLOAAttackInfo& Attack, const TArray<FLOAJudgeResult>& Results);

	// ---------- 클라 ----------

	/**
	 * [클라] 판정 방식이 "내 화면에서 맞을 자리였다"를 올렸다 (피해자 판정 요청 도착 / 예고 장판이 내 화면에서 다 참)
	 * 순서만 정한다: ① 내 저가 컴포넌트가 막았나 → 막았으면 끝 ② CC가 있으면 먼저 반응 (ULOACrowdControlPredictionComponent)
	 * @return 막았나 (피해자 판정이 "막음" / "맞음" 보고를 고른다)
	 */
	bool ReactLocal(const FLOALocalHit& Hit);

	/** [서버] 다음 공격 번호 (1부터, 겹치지 않게) */
	int32 NextAttackId();

	/** [서버] 결과를 기다리는 공격들 (공격 번호 → 공격) */
	UPROPERTY()
	TMap<int32, FLOAJudgingAttack> JudgingAttacks;

	/** [서버] 마지막으로 쓴 공격 번호 (0 = 번호 없음) */
	int32 LastAttackId = 0;
};
