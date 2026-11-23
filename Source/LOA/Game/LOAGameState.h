// LOAGameState.h
// [배관] 레이드 전체가 함께 쓰는 상태. 레이드 단계(전투 전 / 전투 중 / 전멸 / 화면 전환 / 정비소)와 공용 부활 목숨.
//
// ─────────────────────────────────────────────────────────────────────────────
// [공용 부활 목숨] (사용자 결정 2026-10-22)
//   - 파티 전체가 목숨 주머니 하나를 같이 쓴다. 누가 쓰면 다른 사람은 다음 목숨이 찰 때까지 못 쓴다.
//   - 최대 개수 = 지금 레이드 인원수 (PlayerArray 수). 3명이면 최대 3개
//   - 전투 시작 때 0개. 60초마다 1개씩 찬다.
//   - 최대까지 차면 시계가 멈춘다. 누가 하나 쓰면 그때부터 다시 60초. (사용자 결정 2026-10-23: A안)
//
//   예) 3인. 전투 시작 0초 → 60초 1개 → 120초 2개 → 180초 3개(가득, 시계 멈춤)
//       200초에 A가 부활(2개) → 시계 다시 시작 → 260초 3개
//
// [흐름]
//   보스 StateTree "전투" 상태 진입 (LOAStateTreeTask_BattleClock) → StartBattle()  [서버, 전투 전일 때만 받음]
//   타이머 → 목숨 +1 → 아직 최대가 아니면 다음 60초 / 최대면 멈춤                     [서버]
//   죽은 플레이어가 부활 버튼 → ALOAPlayerController::ServerRequestRevive
//     → TryConsumeReviveLife() 성공이면 → ALOACharacter::ReviveFromDeath()         [서버]
//   인원이 들어오고 나가면 최대 개수를 다시 계산                                     [서버]
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1 서버만   : 목숨 차기·쓰기, 최대 개수 계산, 타이머
//   2 복제     : ReviveLives, MaxReviveLives, NextReviveLifeServerTime → 모든 컴퓨터
//               (GameState는 모든 컴퓨터에 하나씩 있고 항상 복제된다 → 공용 상태를 두는 곳)
//   3 각자     : 남은 시간 표시 = NextReviveLifeServerTime − 지금 서버 시각 (LOANetTime). 매 초 복제하지 않는다.
//
// [알림]
//   OnReviveLivesChanged : 세 값 중 하나라도 바뀌면 (서버: 바꾼 직후 / 클라: 복제 도착). 이 컴퓨터 안에서만.
//   받는 쪽은 Getter로 다시 읽는다. (예: ULOAOverlayWidgetController → 부활 UI)
//   OnPartyChanged       : PlayerArray에 들어옴 / 빠짐 (모든 컴퓨터. 클라는 PlayerState 복제 도착·삭제 때). 받는 쪽은 PlayerArray를 다시 읽는다
//                          (예: ULOAOverlayWidgetController → 파티 틀, 2026-11-10)
//
// ─────────────────────────────────────────────────────────────────────────────
// [레이드 단계와 전멸] (사용자 결정 2026-10-25)
//   전투 전(Idle) ──보스 "전투" 상태 진입(StartBattle)──▶ 전투 중(Battle)
//   전투 중 ──살아 있는 플레이어 0명(보스는 살아 있음)──▶ 전멸(Wiped)
//   전멸 ──3초 뒤──▶ 화면 전환(Transferring) ──▶ 정비소(Repair)
//   정비소 ──살아 있는 전원이 출구 상자(ALOARepairExit) 안──▶ 화면 전환 ──▶ 전투 전. 다시 보스에게 다가가면 전투 시작
//
//   전멸 순간 [서버]
//     ① 단계 = 전멸, "정비소로 떠나는 서버 시각" = 지금 + 3초 (복제 → 각 컴퓨터가 남은 초를 계산. 서버 시각 원칙)
//     ② 공용 목숨 초기화 (0개, 시계 멈춤. 다음 전투 시작 때 다시 0부터)
//     ③ 보스 리셋 (ALOABossCharacter::ResetForWipe: AI 멈춤, 패턴 취소, 상태·쿨타임 GE 제거, 체력 가득, 처음 자리)
//     ④ 바닥에 남은 장판·투사체 지우기 (ALOASkillActor 전부 → 장판의 예고도 같이 사라짐. 예고 풀은 그대로)
//   3초 뒤 [서버] → 화면 전환 (목적지 = 정비소)
//
//   언제 전멸을 확인하나: 플레이어가 죽을 때(ALOACharacter → NotifyPlayerDied, 다음 프레임), 플레이어가 나갈 때(RemovePlayerState)
//   전멸로 보지 않는 때: 보스가 이미 쓰러짐 / 전투 중이 아님 (정비소에서 낙사해도 전멸 아님)
//   마지막 생존자가 나가도(접속 종료) 전멸로 본다
//
// [다음 프레임에 확인하는 이유] (2026-10-25 상의)
//   마지막 사망은 보통 장판 판정 함수 안쪽에서 일어난다. 바로 전멸하면 그 장판을 지우고 그 장판을 깐 보스 패턴을 취소한 뒤
//   장판 함수로 돌아간다. 기준: 실행 중인 함수의 주인(액터·GA)을 그 함수 안쪽에서 없애거나 끊을 때만 미룬다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [화면 전환과 정비소] (사용자 결정 2026-10-25)
//   정비소 = 같은 레벨의 떨어진 구역. 정비소 자리 = 위치 표시 Location.Raid.Respawn (없으면 각자 PlayerStart)
//   전투 구역 입구 = 위치 표시 Location.Raid.Entrance 하나. 플레이어마다 표시의 오른쪽 방향으로 150cm씩 벌려 세운다
//     예) 3명 → -150 / 0 / +150 , 2명 → -75 / +75
//   처음 게임 시작은 지금처럼 PlayerStart (정비소는 전멸했을 때만)
//
//   화면 전환 예) 전환 시작 서버 시각 100.0, 어두워짐 0.5 / 검은 채 1.0 / 밝아짐 0.5
//     100.0 [서버] 단계 = 화면 전환, 시작 시각 100.0 복제   [각 컴퓨터] 어두워지기 시작
//     100.5 [서버] 전원 순간이동 (+ 정비소 갈 때는 부활)      [각 컴퓨터] 완전히 검음
//     101.5 [서버] 단계 = 정비소 / 전투 전 (전투 전이면 보스 AI 다시 켬)   [각 컴퓨터] 밝아지기 시작
//     102.0 다 밝아짐
//   각 컴퓨터는 "지금 서버 시각 − 시작 시각"으로 어둡기를 계산한다 (GetTransferFadeAlpha, ALOAPlayerController가 매 프레임 읽음).
//   핑 200이면 0.1초 늦게 받아 어두워지기 20% 지점부터 시작. 순간이동 결과도 검은 1초 안에 도착한다.
//   검은 채 1초는 "렌더링 등 정리 시간"의 자리. 지금은 고정. 실제 로딩이 생기면 각자 준비 끝 보고로 바꾼다.
//
//   정비소 출구 [서버]: ALOARepairExit가 겹침이 바뀔 때마다 CheckRepairExit → 살아 있는 플레이어 전원이 상자 안이면 출발.
//     사람이 나가도(접속 종료) 다시 확인. 정비소에 도착한 순간에도 한 번 확인 (도착 자리가 상자 안인 경우)
//   정비소 단계에서는 전투가 시작되지 않는다 (StartBattle은 전투 전일 때만 받음 + 보스 AI는 멈춰 있음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [클리어] (사용자 결정 2026-11-15, apply115)
//   전투 중 ──보스 쓰러짐──▶ 클리어(Cleared). 끝 단계 (다시 전투가 시작되지 않는다. 각자 결과 화면의 "나가기"로 로비)
//   보스 쓰러짐 [서버] (레이드 상황판 OnBossDefeated) → 다음 프레임 BeginClear  (전멸과 같은 이유: 보스는 보통 장판·스킬 판정 함수 안쪽에서 죽는다)
//   BeginClear [서버]
//     ① 결과 = 기록 담당(ULOARaidRecordSubsystem)이 만든 몫·MVP + 걸린 시간(전투 시작 서버 시각부터) + 보스 이름·얼굴
//     ② 기록 멈춤, 목숨 시계 멈춤, 남은 장판·투사체 지우기 (사용자 결정: 바로 지움)
//     ③ 단계 = 클리어 (결과를 먼저 넣고 단계를 바꾼다 → 서버(리슨 호스트)도 단계 알림 때 결과가 있다)
//   결과(ClearResult)와 단계(RaidPhase)는 같은 액터의 복제 칸 → 같이 도착하고, 엔진은 칸을 다 넣은 뒤 OnRep을 부른다
//     → 클라도 단계 알림(OnRaidPhaseChanged) 때 결과가 이미 있다 (따로 두면 화면이 먼저 뜨고 숫자가 0인 순간이 생길 수 있음)
//   클리어 순간 죽어 있던 사람은 그대로 둔다 (대신 정함)
//
//   예) 전투 시작 서버 시각 100.0, 보스 쓰러짐 322.4 → 걸린 시간 222.4초 = "3분 42초"
//   전멸 뒤 다시 도전해서 깨면 마지막 도전만 (StartBattle이 시각과 기록을 새로 시작. 사용자 결정 ⑥)
//
// [BP에서 설정할 것]
//   BP_LOAGameMode → Game State Class = LOAGameState (C++ 기본값으로 이미 들어가 있음. BP에서 바꿨으면 되돌리기)
//   목숨 주기를 바꾸려면 BP로 자식을 만들어 Revive Life Interval을 고치고 그 BP를 Game State Class로

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/LOARaidResultTypes.h"
#include "LOAGameState.generated.h"

/** 공용 부활 목숨이 바뀌었다 (개수, 최대, 다음 시각 중 하나). 이 컴퓨터 안에서만 */
DECLARE_MULTICAST_DELEGATE(FLOAReviveLivesChangedNative);

/** 파티(PlayerArray)가 바뀌었다. 이 컴퓨터 안에서만 */
DECLARE_MULTICAST_DELEGATE(FLOAPartyChangedNative);

/** 레이드 단계 */
UENUM(BlueprintType)
enum class ELOARaidPhase : uint8
{
	Idle	UMETA(DisplayName = "전투 전"),
	Battle	UMETA(DisplayName = "전투 중"),
	Wiped	UMETA(DisplayName = "전멸"),
	Transferring	UMETA(DisplayName = "화면 전환"),
	Repair	UMETA(DisplayName = "정비소"),
	Cleared	UMETA(DisplayName = "클리어"),
};

/** 화면 전환이 끝나면 어디로 가나 (서버만 앎) */
enum class ELOATransferDestination : uint8
{
	RepairArea,	// 정비소 (전멸 뒤)
	BattleArea,	// 전투 구역 입구 (정비소 출구)
};

/** 레이드 단계가 바뀌었다. 이 컴퓨터 안에서만 (나중에 전멸 UI가 구독) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOARaidPhaseChangedNative, ELOARaidPhase /*NewPhase*/);

class ALOACharacter;
class ALOABossCharacter;

UCLASS()
class LOA_API ALOAGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALOAGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AActor Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** [서버] 레이드 상황판의 보스 쓰러짐 알림을 듣는다 (헤더 [클리어]) */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin AGameStateBase Interface
	/** 인원이 바뀌면 최대 목숨을 다시 계산한다 [서버만 처리] + OnPartyChanged [모든 컴퓨터] */
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	//~ End AGameStateBase Interface

	/**
	 * [서버] 전투 시작. 단계 = 전투 중, 목숨 0개, 최대 = 지금 인원수, 60초 시계 시작.
	 * 전투 전(Idle)일 때만 받는다. (전투 중에 보스가 전투 상태에 다시 들어와도 목숨이 초기화되지 않게)
	 */
	void StartBattle();

	/** [서버] 플레이어가 죽었다. (ALOACharacter가 사망 반응 부품의 OnDied에서 부름) → 전멸인지 확인 */
	void NotifyPlayerDied(ALOACharacter* DeadCharacter);

	UFUNCTION(BlueprintPure, Category = "LOA|Raid")
	ELOARaidPhase GetRaidPhase() const { return RaidPhase; }

	/** [모든 컴퓨터] 전멸 뒤 정비소로 떠나기까지 남은 초. 전멸 상태가 아니면 -1 */
	UFUNCTION(BlueprintPure, Category = "LOA|Raid")
	float GetSecondsUntilWipeRespawn() const;

	/** 헤더 [레이드 단계와 전멸] */
	FLOARaidPhaseChangedNative OnRaidPhaseChanged;

	/** [모든 컴퓨터] 클리어 결과. 클리어 단계가 아니면 비어 있음 (IsValid false). 헤더 [클리어] */
	const FLOARaidClearResult& GetClearResult() const { return ClearResult; }

	/**
	 * [모든 컴퓨터] 화면 전환의 어둡기 0(그대로) ~ 1(검음). 전환 중이 아니면 0.
	 * 시작 서버 시각에서 지난 시간으로 계산한다 (헤더 [화면 전환과 정비소]). ALOAPlayerController가 매 프레임 읽는다.
	 */
	float GetTransferFadeAlpha() const;

	/** [서버] 정비소 출구 상자의 겹침이 바뀌었다 (ALOARepairExit가 부름). 살아 있는 전원이 상자 안이면 전투 구역으로 */
	void CheckRepairExit();

	/**
	 * [서버] 목숨 하나를 쓴다. 0개면 false.
	 * 가득 차서 멈춰 있던 시계는 여기서 다시 시작한다. (예: 3/3 → 2/3, 지금부터 60초 뒤 3/3)
	 */
	bool TryConsumeReviveLife();

	UFUNCTION(BlueprintPure, Category = "LOA|Revive")
	int32 GetReviveLives() const { return ReviveLives; }

	UFUNCTION(BlueprintPure, Category = "LOA|Revive")
	int32 GetMaxReviveLives() const { return MaxReviveLives; }

	UFUNCTION(BlueprintPure, Category = "LOA|Revive")
	bool IsBattleStarted() const { return RaidPhase == ELOARaidPhase::Battle; }

	/**
	 * [모든 컴퓨터] 다음 목숨까지 남은 초. 시계가 멈춰 있으면(전투 전, 가득 참) -1.
	 * 위젯이 매 프레임 불러도 된다. (빼기 한 번)
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Revive")
	float GetSecondsUntilNextReviveLife() const;

	/** 헤더 [알림] */
	FLOAReviveLivesChangedNative OnReviveLivesChanged;

	/** 헤더 [알림] */
	FLOAPartyChangedNative OnPartyChanged;

	/**
	 * [서버] 남아 있는 장판·투사체를 전부 지운다 (전멸, 클리어)
	 * public (2026-11-18 apply122): 대난투 경기장(ALOABrawlArena)도 시작할 때 부른다 (사용자 결정: 지움). 같은 일을 두 곳에 쓰지 않으려고
	 */
	void DestroyAllSkillActors();

private:
	/** [서버] 지금부터 Interval 뒤에 목숨이 차도록 시계를 건다 */
	void StartLifeTimer();

	/** [서버] 시계를 멈춘다 (가득 참) */
	void StopLifeTimer();

	/** [서버] 시계가 다 됐다 → 목숨 +1 → 아직 최대가 아니면 다음 시계 */
	void OnLifeTimerFired();

	/** [서버] 최대 = 지금 인원수. 넘친 목숨은 버리고, 빈자리가 생겼는데 시계가 멈춰 있으면 다시 건다 */
	void RefreshMaxReviveLives();

	/** 서버는 값을 바꾼 직후, 클라는 OnRep에서 → OnReviveLivesChanged */
	void BroadcastReviveLivesChanged();

	UFUNCTION()
	void OnRep_ReviveLives();

	// ---------- 전멸 [서버] ----------
	/** 전투 중인데 살아 있는 플레이어가 0명이고 보스는 살아 있으면 전멸 */
	void CheckWipe();

	/** 전멸 순간: ① ~ ④ (헤더 [레이드 단계와 전멸]) */
	void BeginWipe();

	/** 3초 뒤: 정비소로 화면 전환 */
	void FinishWipe();

	// ---------- 클리어 [서버] ----------
	/** 레이드 상황판: 보스가 쓰러짐 (모든 컴퓨터에서 울리지만 서버만 처리) → 다음 프레임 BeginClear */
	UFUNCTION()
	void HandleBossDefeated(ALOABossCharacter* Boss);

	/** 헤더 [클리어] ① ~ ③. 전투 중일 때만 */
	void BeginClear();

	// ---------- 화면 전환 [서버] ----------
	/** 단계 = 화면 전환, 시작 서버 시각 기록 → 어두워짐 뒤 순간이동, 검은 채 기다림 뒤 FinishTransfer */
	void BeginTransfer(ELOATransferDestination Destination);

	/** 완전히 검을 때: 목적지로 전원 순간이동 (정비소 갈 때는 부활도) */
	void TeleportForTransfer();

	/** 밝아지기 시작할 때: 목적지에 맞는 단계로 (정비소 / 전투 전 + 보스 AI 다시 켬) */
	void FinishTransfer();

	/** 전투 구역 입구 자리. 위치 표시 Location.Raid.Entrance를 중심으로 오른쪽으로 벌림. 없으면 PlayerStart → 없으면 false */
	bool FindEntranceLocation(AController* ForController, int32 Index, int32 Count, FVector& OutLocation) const;

	/** 공용 목숨을 전투 전 상태로 (0개, 최대 0, 시계 멈춤) */
	void ResetReviveLives();

	/** 정비소 자리. 위치 표시 Location.Raid.Respawn → 없으면 그 플레이어의 PlayerStart → 그것도 없으면 false */
	bool FindRespawnLocation(AController* ForController, FVector& OutLocation) const;

	/** [서버] 단계를 바꾸고 알린다 */
	void SetRaidPhase(ELOARaidPhase NewPhase);

	/** [모든 컴퓨터] 단계가 바뀜 → OnRaidPhaseChanged + 디버그 문구 */
	void HandleRaidPhaseChanged();

	UFUNCTION()
	void OnRep_RaidPhase();

	// ---------- 설정 ----------
	/** 목숨 하나가 차는 데 걸리는 시간 (초, 게임 시간) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Revive", meta = (ClampMin = "1.0", Units = "Seconds"))
	float ReviveLifeInterval = 60.f;

	// ---------- 복제되는 상태 ----------
	/** 지금 쓸 수 있는 목숨 */
	UPROPERTY(ReplicatedUsing = OnRep_ReviveLives, VisibleInstanceOnly, Category = "LOA|Revive")
	int32 ReviveLives = 0;

	/** 최대 목숨 = 레이드 인원수 */
	UPROPERTY(ReplicatedUsing = OnRep_ReviveLives, VisibleInstanceOnly, Category = "LOA|Revive")
	int32 MaxReviveLives = 0;

	/** 다음 목숨이 차는 서버 시각 (LOANetTime 기준). 0 = 시계 멈춤 */
	UPROPERTY(ReplicatedUsing = OnRep_ReviveLives, VisibleInstanceOnly, Category = "LOA|Revive")
	double NextReviveLifeServerTime = 0.0;

	/** [서버] 목숨 시계 */
	FTimerHandle ReviveLifeTimerHandle;

	// ---------- 전멸 ----------
	/** 전멸 뒤 부활까지 (초, 게임 시간). 나중에 전멸 UI가 끝나는 때로 바뀐다 (사용자 결정: 지금은 3초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid", meta = (ClampMin = "0.0", Units = "Seconds"))
	float WipeRespawnDelay = 3.f;

	/** 지금 레이드 단계 */
	UPROPERTY(ReplicatedUsing = OnRep_RaidPhase, VisibleInstanceOnly, Category = "LOA|Raid")
	ELOARaidPhase RaidPhase = ELOARaidPhase::Idle;

	/** 전멸 뒤 부활하는 서버 시각 (LOANetTime). 전멸이 아니면 0. 단계와 같은 때 복제 */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "LOA|Raid")
	double WipeRespawnServerTime = 0.0;

	/** [서버] 전멸 → 정비소로 화면 전환 타이머 */
	FTimerHandle WipeRespawnTimerHandle;

	// ---------- 화면 전환 ----------
	/** 어두워지는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid|Transfer", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TransferFadeOutTime = 0.5f;

	/** 완전히 검은 채 기다리는 시간 (초). 순간이동한 자리의 렌더링 정리 시간 (지금은 고정) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid|Transfer", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TransferBlackHoldTime = 1.f;

	/** 밝아지는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid|Transfer", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TransferFadeInTime = 0.5f;

	/** 전투 구역 입구에서 플레이어 사이 간격 (cm) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Raid|Transfer", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float EntranceSpacing = 150.f;

	/** 화면 전환 시작 서버 시각 (LOANetTime). 0 = 아직 없음. 전환이 끝나도 지우지 않는다 (밝아짐 계산에 씀, 다음 전환이 덮어씀) */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "LOA|Raid")
	double TransferStartServerTime = 0.0;

	// ---------- 클리어 ----------
	/** [서버] 이번 전투를 시작한 서버 시각 (LOANetTime). 걸린 시간 계산용. 복제 안 함 */
	double BattleStartServerTime = 0.0;

	/** 클리어 결과. BeginClear가 단계와 같은 때 넣는다 → 같이 복제 (헤더 [클리어]) */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "LOA|Raid")
	FLOARaidClearResult ClearResult;

	/** [서버] 이번 화면 전환의 목적지 */
	ELOATransferDestination TransferDestination = ELOATransferDestination::RepairArea;

	/** [서버] 순간이동 / 전환 끝 타이머 */
	FTimerHandle TransferTeleportTimerHandle;
	FTimerHandle TransferFinishTimerHandle;
};
