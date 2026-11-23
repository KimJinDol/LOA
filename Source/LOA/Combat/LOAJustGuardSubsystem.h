// LOAJustGuardSubsystem.h
// [배관] 저가 매니저. 저가 구간(ID) 열고 닫기, 원격 누름 검사, 누르기 마감, 성공 인원 집계. (2026-10-26)
// "막았나"는 여기서 판단하지 않는다. 맞은 플레이어의 저가 컴포넌트(ULOAJustGuardComponent)가 각자 판단한다. (2026-11-04)
//
// ─────────────────────────────────────────────────────────────────────────────
// [누가 무엇을] (2026-11-04 판정 구조 개편)
//
//   보스 몽타주 구간 (LOA Just Guard Window)        ── 구간을 연다·닫는다 ──────▶ 이 서브시스템
//   플레이어 GA (LOAGA_JustGuard)                   ── 누름·뗌·취소 ─────────────▶ 이 서브시스템 → 그 플레이어 저가 컴포넌트에 적기
//   서버 저가 컴포넌트 (ULOAJustGuardComponent)     ── "이 보스, 이 시각의 구간?" ─▶ 이 서브시스템 (FindWindowAt)
//   판정 매니저 (ULOACombatJudgeManagerSubsystem)   ── 판정 시작 / 한 사람 결과 / 판정 끝 알림 ──▶ 이 서브시스템이 듣기만 한다
//   보스 모듈 (LOASkillModule_JustGuard)            ◀── 타격 하나 끝남 ──────────── 이 서브시스템 (OnJustGuardResolved)
//
//   판정 쪽과 직접 대화하지 않는다. 판정 매니저는 누가 듣는지 모른다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [저가 구간]
//   보스 몽타주의 구간 하나 = 저가 ID 하나 = 막을 공격 하나 (홀딩은 연타 전체)
//
//   9.00  [서버] OpenWindow → #7 → 모든 플레이어에게 "#7 열림 (단타, 보스 A)"
//   9.75  [클라] G → 장부에 "#7 누름 9.75" → 서버에 보고 → ReceivePress가 검사 후 사본에 적음
//  10.00  [서버] 판정 매니저 "판정 시작 #31 (보스 A, 저가 공격)" → #7의 첫 타격 → 누르기 마감 → 모든 플레이어에게 "#7 닫힘"
//               → 판정 매니저의 확정이 맞은 사람 저가 컴포넌트에 "막았나?" → 컴포넌트가 FindWindowAt(보스 A, 10.00) = #7로 판단
//  10.20  [서버] CloseWindow → 모든 플레이어에게 "#7 닫힘" (홀딩 중이면 저가 GA가 끝난다. 2026-11-04)
//
// [누르기 마감] (2026-10-27)
//   구간의 첫 타격 순간 = G를 누를 수 있는 끝. 그 뒤에 누르면 어차피 못 막는다 (단타 기준 = 이 타격, 홀딩 기준 = 첫 타)
//   → 헛모션이 나가지 않게 첫 타격 때 클라에 "닫힘"을 보낸다.
//   마감 알림은 보스 공격과 같은 길(서버 → 클라)로 같이 늦게 와서, 클라 화면에서 보스가 친 순간과 거의 맞는다.
//   서버 쪽 구간은 노티파이 끝까지 열려 있다 (홀딩 2타·3타도 막을 수 있는 공격이어야 하니까)
//   클라가 받는 알림은 구간마다 3번: 열림 → 누르기 마감(첫 타격) → 닫힘(구간 끝). 마감이 없으면 2번 (2026-11-04. 전에는 마감 뒤 닫힘을 안 보냄)
//     "더 못 누름"(마감)과 "구간 끝"(닫힘)을 나눠서 알린다 → 홀딩 중인 플레이어는 구간이 끝나면 저가 자세를 푼다 (ELOAJustGuardWindowState)
//   홀딩 (중간부터)는 마감하지 않는다: 첫 타 뒤에 눌러도 다음 타격부터 막는 방식이라 (2026-11-04)
//   순서: 판정 매니저가 "판정 시작"을 알린 뒤에 피해자 판정 요청을 보낸다 → "닫힘"이 요청보다 먼저 도착한다 (전과 같음)
//
// [원격 누름 검사] ReceivePress
//   모르는 ID              → 버림
//   닫힌 지 0.3초 넘음      → 버림 (클라는 닫힘을 편도만큼 늦게 보니까 0.3초까지는 받음)
//   누른 시각이 미래        → 지금으로
//   열리기 전 / 0.3초 넘게 옛날 → 그쪽으로 당김 (시각 조작 방지)
//
// [집계] 판정 매니저 알림을 듣는다 (2026-11-04)
//   판정 시작 (저가 공격 + 그 보스에게 그 시각 열린 구간) → 공격 번호로 집계를 연다
//   한 사람 결과 (플레이어만)                              → 막음 / 맞음 목록에 적는다 (피함은 어느 쪽에도 없다)
//   판정 끝                                                → 바로 OnJustGuardResolved (협동 저가 집계는 판정이 끝나는 즉시)
//   패턴 하나의 성공 인원·등급은 보스 Just Guard 모듈이 모아 센다 → 등급 규칙만 여기 (GetCoopJustGuardGrade, 2026-11-18 apply128)
//   피해자 판정 대상은 보고(또는 마감) 뒤에 결과가 오므로 "판정 끝"이 그때까지 기다린다 (판정 매니저가 센다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만  : 구간 ID, 누름 검사, 누르기 마감, 집계
//   2. 전달    : 구간 열림·닫힘 + 그 보스 + 구간 길이 (서버 → 모든 플레이어). ULOACombatRPCComponent
//                ("막음" 알림은 서버 저가 컴포넌트가 직접 보낸다)
//   3. 각자    : 없음 (장부 원본은 플레이어 컴퓨터의 ULOAJustGuardComponent)
//   복제되지 않는다. 서버에서만 쓴다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Library/LOACombatTypes.h"   // ELOAJustGuardMode
#include "Combat/LOAHitJudgeTypes.h"  // FLOAAttackInfo, ELOAHitOutcome (판정 매니저 알림)
#include "LOAJustGuardSubsystem.generated.h"

class APawn;

/** 저가 구간 하나 */
struct FLOAJustGuardWindow
{
	/** 저가 ID (1부터) */
	int32 Id = 0;

	/** 구간을 연 보스 */
	TWeakObjectPtr<AActor> Caster;

	/** 단타 / 홀딩 (첫 타부터) / 홀딩 (중간부터) */
	ELOAJustGuardMode Mode = ELOAJustGuardMode::None;

	/** 열린 서버 시각 */
	double OpenTime = 0.0;

	/** 닫힌 서버 시각. 음수 = 열려 있음 */
	double CloseTime = -1.0;

	/** 누르기 마감 서버 시각 (이 구간의 첫 타격). 음수 = 아직. 헤더 [누르기 마감] */
	double PressCloseTime = -1.0;

	/** 구간 길이 (초, 몽타주 재생 속도 반영). 클라의 홀딩 게이지가 쓴다. 0 = 모름 (2026-11-05) */
	float Duration = 0.f;

	bool IsOpen() const { return CloseTime < 0.0; }
	bool IsPressClosed() const { return PressCloseTime >= 0.0; }
};

/** 저가 공격의 타격 하나가 끝났다 (OnJustGuardResolved로 알린다) */
struct FLOAJustGuardResolved
{
	/** 공격한 보스. 듣는 쪽이 "내 공격인가"를 거를 때 */
	TWeakObjectPtr<AActor> Caster;

	/** 저가 ID. 홀딩은 같은 ID로 여러 번 온다 */
	int32 WindowId = 0;

	/** 그 공격의 속성 */
	FGameplayTagContainer AttackTags;

	/** 막은 플레이어 */
	TArray<TWeakObjectPtr<APawn>> Guarded;

	/** 맞은 플레이어 (모양 밖으로 피한 사람은 어느 쪽에도 없다) */
	TArray<TWeakObjectPtr<APawn>> Hit;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FLOAOnJustGuardResolved, const FLOAJustGuardResolved& /*Result*/);

UCLASS()
class LOA_API ULOAJustGuardSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 저가 매니저. 게임 월드가 아니면 nullptr */
	static ULOAJustGuardSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * 협동 저가 등급 (2026-11-18 apply128). 성공 인원 → 등급 (Project Settings → LOA Combat → Just Guard)
	 *   예) 기본 3 / 2 → 3명 Perfect, 2명 Great, 1명·0명 Bad
	 */
	static ELOAJustGuardGrade GetCoopJustGuardGrade(int32 SuccessCount);

	// ---------- 저가 구간 [서버] ----------

	/**
	 * 구간을 연다 (보스 몽타주 구간 시작). ID를 매기고 모든 플레이어에게 알린다. @return 저가 ID, 서버가 아니면 0
	 * @param Duration  구간 길이 (초). "열림" 알림에 같이 보낸다 → 클라의 홀딩 게이지 (2026-11-05)
	 */
	int32 OpenWindow(AActor* Caster, ELOAJustGuardMode Mode, float Duration);

	/** 구간을 닫는다 (구간 끝, 몽타주가 끊겨도). 모든 플레이어에게 "닫힘"을 알린다 (누르기 마감 뒤여도) */
	void CloseWindow(int32 WindowId);

	/**
	 * 이 보스의 구간 중 ServerTime에 열려 있던 것 (가장 최근에 열린 것부터). 없으면 nullptr (2026-11-04)
	 * 서버 저가 컴포넌트가 "이 타격은 어느 구간인가"를 물을 때 (판정 시각으로 찾으므로 보고가 늦게 와도 같은 구간)
	 */
	const FLOAJustGuardWindow* FindWindowAt(const AActor* Caster, double ServerTime) const;

	// ---------- 원격 누름 [서버] (ULOACombatRPCComponent가 부른다) ----------

	/** 누름 보고. 검사한 뒤 그 플레이어 장부(사본)에 적는다. 헤더 [원격 누름 검사] */
	void ReceivePress(APawn* Player, int32 WindowId, double ReportedPressTime, float PressYaw);

	/** 뗌·취소 보고. 누른 시각 ~ 지금 사이로 맞춰 장부(사본)에 적는다 */
	void ReceiveRelease(APawn* Player, int32 WindowId, double ReportedTime, bool bCancelled);

	/** 타격 하나의 판정이 모두 끝났다. 보스 Just Guard 모듈이 듣는다 */
	FLOAOnJustGuardResolved OnJustGuardResolved;

protected:
	//~ Begin UWorldSubsystem Interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	/** 판정 매니저 알림 구독 (판정 매니저가 먼저 만들어지게 한다) */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem Interface

private:
	// ---------- 판정 매니저 알림 듣기 [서버] (헤더 [집계]) ----------

	/** 판정 시작: 저가 공격이고 그 보스에게 그 시각 열린 구간이 있으면 집계를 연다 + 그 구간의 첫 타격이면 누르기 마감 */
	void OnJudgeStarted(const FLOAAttackInfo& Attack);

	/** 한 사람 결과: 플레이어면 막음 / 맞음 목록에 적는다 */
	void OnHitOutcome(int32 AttackId, AActor* Target, ELOAHitOutcome Outcome);

	/** 판정 끝: 바로 OnJustGuardResolved */
	void OnJudgeFinished(int32 AttackId);

	/** ID로 구간 (닫힌 것도). 없으면 nullptr */
	const FLOAJustGuardWindow* FindWindow(int32 WindowId) const;
	FLOAJustGuardWindow* FindWindowMutable(int32 WindowId);

	/** 첫 타격: 누르기 마감 + 모든 플레이어에게 "닫힘" (클라는 이제 G를 못 누름). 이미 마감했으면 아무것도 안 함 */
	void ClosePress(FLOAJustGuardWindow& Window);

	/** 모든 플레이어에게 구간 상태(열림 / 누르기 마감 / 닫힘) + 그 보스 + 구간 길이 */
	void SendWindowToAll(const FLOAJustGuardWindow& Window, ELOAJustGuardWindowState State) const;

	/** 구간들 (열린 것 + 닫힌 지 얼마 안 된 것). 오래된 것이 앞 */
	TArray<FLOAJustGuardWindow> Windows;
	int32 LastWindowId = 0;

	/** 집계들 (공격 번호 → 그 타격의 결과). 저가 공격만 */
	TMap<int32, FLOAJustGuardResolved> Tallies;

	/** 판정 매니저 알림 구독 번호 (Initialize ~ Deinitialize) */
	FDelegateHandle JudgeStartedHandle;
	FDelegateHandle HitOutcomeHandle;
	FDelegateHandle JudgeFinishedHandle;
};
