// LOACombatRPCComponent.h
// [배관] 전투 RPC 컴포넌트. 서버와 "이 플레이어 컴퓨터" 사이의 전투 RPC를 한곳에 모은 통로.
// (예전 이름: ULOACombatFeedbackComponent. 피해자 판정 보고가 들어오면서 "서버 → 화면"만이 아니게 되어 이름을 바꿈)
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙] 통로만 한다.
//   받아서 주인에게 넘기기만 한다. 확인, 계산, 상태(변수·타이머)는 넣지 않는다. 판단은 주인 클래스가 한다.
//   데미지 숫자도 같다 (2026-11-15): 전투 알림판이 "이 플레이어가 때린 피해"만 골라 보내 준다 → 숫자 정보로 옮겨 담아 RPC만 한다
//   회복 숫자도 같다 (2026-11-16): 전투 알림판이 "이 플레이어가 받은 회복"만 골라 보내 준다 → 같은 RPC, 종류만 Heal
//   받은 데미지 숫자도 같다 (2026-11-16 apply118): "이 플레이어가 맞은 피해" → 같은 RPC, 종류 DamageTaken (빨강)
//   어떤 기능이 자기 상태를 갖게 되면 그 기능만 따로 컴포넌트로 뺀다. (예: 클릭 이동은 경로·Tick이 있어서 따로)
//
// [하는 일]
//   서버 → 이 플레이어 컴퓨터 (Client RPC)
//     - 전투 글자 (내가 준 데미지 숫자 · 내가 받은 데미지 숫자 · 내가 받은 회복 숫자)   ClientShowCombatText → HUD (2026-11-16 이름 바꿈, 옛 ClientShowDamageNumber)
//     - 보스 패턴 예고 장판          ClientShowTelegraph / ClientHideTelegraph → ULOATelegraphSubsystem
//     - 피해자 판정 요청             ClientRequestHitJudge    → ULOAVictimJudgeSubsystem
//     - 저가 구간 상태 + 그 보스     ClientJustGuardWindow    → 내 캐릭터의 ULOAJustGuardComponent (2026-10-26, 보스·상태 셋 2026-11-04)
//     - 저스트 가드 성공 (서버 사본)  ClientJustGuardResult    → 내 캐릭터의 ULOAJustGuardComponent
//   이 플레이어 컴퓨터 → 서버 (Server RPC)
//     - 피해자 판정 보고             ServerReportHitJudgeHit / ServerReportHitJudgeMiss / ServerReportHitJudgeGuarded → ULOAVictimJudgeSubsystem
//     - 저스트 가드 누름·뗌·취소     ServerReportJustGuardPress / ServerReportJustGuardRelease → ULOAJustGuardSubsystem (검사 후 장부 사본)
//       누름 보고와 "막았음" 보고가 같은 이 컴포넌트(같은 액터 채널)라서 누름이 항상 먼저 도착한다. (ASC는 PlayerState라 채널이 달라 순서 보장 없음)
//   나중: 받은 데미지, "면역" 같은 상태 글자, 카운터 성공, 무력화 표시 …
//
// [하지 않는 일]
//   - 전투와 무관한 RPC (알림, 채팅 등) → 생기면 그때 따로 만든다. (이 컴포넌트가 잡동사니 통이 되지 않게)
//   - 화면 그리기, 판정, 보고 확인 (HUD, 예고 서브시스템, 판정 서브시스템 담당)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 PlayerController에 붙나]
// RPC는 "그 플레이어 연결이 소유한 액터"에서만 오갈 수 있다. (서버가 특정 플레이어 한 명에게 보내는 통로, 그 플레이어가 서버에 보내는 통로)
// HUD와 서브시스템은 RPC를 받을 수 없다. (액터가 아님) → PC에 붙은 이 컴포넌트가 오가며 넘긴다.
// Pawn이 아니라 PC인 이유: 사망/부활로 Pawn이 바뀌어도 PC는 그대로라 그 사이 도트 숫자도 유실되지 않는다.
//
// [왜 PC에 직접 RPC를 두지 않나]
// 전투 RPC가 늘어나면 PC가 비대해진다. 여기에 모아 PC는 입력/HUD 연결만 담당한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   - 컴포넌트에 RPC를 쓰려면 컴포넌트가 복제되도록 켜야 한다. (생성자의 SetIsReplicatedByDefault)
//     안 켜면 RPC가 오류 없이 조용히 안 간다.
//   - 이 컴포넌트를 부르는 쪽은 PC 클래스를 몰라도 된다. PC에서 FindComponentByClass로 찾는다.
//     (예고 = ULOATelegraphSubsystem, 피해자 판정 = ULOAVictimJudgeSubsystem)
//     전투 글자만 반대로 이 컴포넌트가 듣는다: 서버의 BeginPlay에서 전투 알림판에 "주인 플레이어가 때린 피해"(2026-11-15)와 "받은 회복"(2026-11-16)을 구독
//   - 전투 글자 = Unreliable: 연출용이라 유실돼도 실제 결과(체력 등)에는 영향 없음.
//   - 예고 = Reliable: 예고를 보고 피하는 게임이다. 즉사기 예고가 빠지면 억울한 죽음이 생긴다.
//   - 판정 요청·보고 = Reliable: 빠지면 마감 뒤 서버 판정으로 처리되지만, 피한 사람이 억울하게 맞지 않게 반드시 보낸다.
//     "누가 맞았나"는 인자로 받지 않는다. 보낸 연결의 캐릭터가 곧 맞은 사람 → 남을 대신 보고할 수 없다.
//
// [흐름 - 전투 글자 (데미지 · 회복 숫자)]
//   서버: AttributeSet(데미지 확정) → 전투 알림판(ULOACombatNoticeSubsystem)이 때린 사람(PlayerState)으로 구독자를 찾음
//         → 그 플레이어 PC의 이 컴포넌트 HandleDamageDealt → ClientShowCombatText (2026-11-15. 전에는 ULOACombatLibrary::NotifyDamageDealt가 PC를 찾아 부름)
//   서버: AttributeSet(회복 확정) → 전투 알림판이 받은 사람(PlayerState)으로 구독자를 찾음 → HandleHealReceived → ClientShowCombatText (종류 Heal, 들어온 양, 2026-11-16)
//   서버: AttributeSet(데미지 확정) → 전투 알림판이 맞은 사람(PlayerState)으로 구독자를 찾음 → HandleDamageReceived → ClientShowCombatText (종류 DamageTaken, 들어온 양, apply118)
//   본인 컴퓨터: ClientShowCombatText → ALOAHUD::ShowCombatText → 전투 글자 레이어
//
// [흐름 - 예고]
//   서버: ALOAAreaActor → ULOATelegraphSubsystem::ShowToAll → 플레이어마다 ClientShowTelegraph
//   각 컴퓨터: ClientShowTelegraph → ULOATelegraphSubsystem::ShowTelegraph → 풀에서 꺼내 그림
//   예) 플레이어 3명 → 서버가 3번 보냄 (리슨 호스트 본인은 바로 실행)
//
// [흐름 - 피해자 판정]
//   서버: ULOAVictimJudgeSubsystem → 판정 대상인 원격 플레이어에게 ClientRequestHitJudge(요청 묶음: 번호, 모양, 자리, 방향, 판정 시각, CC GE, 공격한 보스, 공격 속성)
//   클라: ULOAVictimJudgeSubsystem::JudgeLocal → 받은 순간의 내 위치로 판정 → 맞음: ServerReportHitJudgeHit(번호) / 안 맞음: ServerReportHitJudgeMiss(번호, 그 위치)
//   서버: 보낸 PC의 캐릭터를 찾아 → ULOAVictimJudgeSubsystem::ReceiveReport (받아 줄지는 피해자 판정이 정함)
//
// [흐름 - 저스트 가드] (2026-10-26)
//   서버: 보스 몽타주의 저가 구간 시작 → ClientJustGuardWindow(#7, 단타, Open, 보스, 구간 길이) → 모든 플레이어 장부 / 첫 타격 → (PressClosed) / 끝 → (Closed)
//   클라: GA가 G 누름 → 장부 원본 #7 + ServerReportJustGuardPress(#7, 누른 시각, 방향) / 뗌·취소 → ServerReportJustGuardRelease
//   서버: 저가 매니저(ULOAJustGuardSubsystem)가 검사한 뒤 보낸 PC 캐릭터의 장부(사본)에 적는다
//   피해자 판정 공격 : 클라가 내 장부로 막았으면 ServerReportHitJudgeGuarded(판정 번호) → 서버 장부(사본)가 다시 판단
//   서버 장부가 막았다고 보면 ClientJustGuardResult(#7) → 클라 GA Success 모션 (이미 띄웠으면 GA가 두 번째 알림을 버린다)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/Combat/CombatText/LOACombatTextTypes.h"       // RPC 인자로 값을 받으므로 전방 선언이 아니라 include
#include "Combat/Telegraph/LOATelegraphTypes.h"             // 같은 이유 (FLOATelegraphData)
#include "Library/LOACombatTypes.h"                         // 같은 이유 (FLOAHitShape, ELOAJustGuardMode)
#include "Combat/LOAHitJudgeTypes.h"                         // 같은 이유 (FLOAHitJudgeRequestData)
#include "LOACombatRPCComponent.generated.h"

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOACombatRPCComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOACombatRPCComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UActorComponent Interface
	/** [서버] 전투 알림판에 "주인 플레이어가 때린 피해"와 "받은 회복"을 구독한다 (전투 글자) */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ---------- 서버 → 이 플레이어 컴퓨터 ----------

	/**
	 * [서버 → 이 플레이어 컴퓨터] 전투 글자를 띄워라 (내가 때린 데미지 숫자 / 내가 받은 회복 숫자). 2026-11-16 이름 바꿈 (옛 ClientShowDamageNumber)
	 * Info: 종류 + 숫자 + 맞은 지점(대상 기준 상대 위치). 항목이 늘어도 이 함수 모양은 그대로.
	 */
	UFUNCTION(Client, Unreliable)
	void ClientShowCombatText(AActor* Target, const FLOACombatTextInfo& Info);

	/**
	 * [서버 → 이 플레이어 컴퓨터] 예고를 그려라. 받은 값을 이 컴퓨터의 ULOATelegraphSubsystem에 넘기기만 한다.
	 * Reliable: 반드시 도착한다. (같은 컴포넌트의 Reliable RPC끼리는 보낸 순서대로 도착 → Hide가 Show보다 먼저 오지 않음)
	 */
	UFUNCTION(Client, Reliable)
	void ClientShowTelegraph(const FLOATelegraphData& Data);

	/** [서버 → 이 플레이어 컴퓨터] 번호의 예고를 지워라. (판정 전에 패턴이 취소됨) */
	UFUNCTION(Client, Reliable)
	void ClientHideTelegraph(int32 TelegraphId);

	/**
	 * [서버 → 이 플레이어 컴퓨터] 피해자 판정 요청. 이 컴퓨터의 피해자 판정(JudgeLocal)에 넘기기만 한다.
	 * 내용(번호, 모양, 자리, 방향, 판정 시각, CC GE, 공격한 보스, 공격 속성)은 묶음 하나로 (FLOAHitJudgeRequestData, 직접 직렬화)
	 */
	UFUNCTION(Client, Reliable)
	void ClientRequestHitJudge(const FLOAHitJudgeRequestData& Request);

	/**
	 * [서버 → 이 플레이어 컴퓨터] 저가 구간 상태(열림 / 누르기 마감 / 닫힘) + 그 구간을 연 보스. 내 캐릭터의 장부에 넘기기만 한다
	 * 열린 동안만 G가 나간다. 닫힘이면 홀딩 중인 저가 GA가 끝난다 (ELOAJustGuardWindowState, 2026-11-04)
	 * Reliable + 같은 채널: 판정 요청보다 먼저 보낸 "열림"이 먼저 도착한다
	 * 보스: 장부가 "이 타격은 어느 구간인가"를 보스로 찾는다 (판정 요청·예고에는 구간 번호가 없다. 2026-11-04)
	 * Duration: 구간 길이 (초). 홀딩 게이지가 쓴다 (2026-11-05)
	 */
	UFUNCTION(Client, Reliable)
	void ClientJustGuardWindow(int32 WindowId, ELOAJustGuardMode Mode, ELOAJustGuardWindowState State, AActor* Attacker, float Duration);

	/**
	 * [서버 → 이 플레이어 컴퓨터] 서버 장부(사본)가 내 저스트 가드가 그 구간을 막았다고 판단했다. 장부에 넘기기만 한다 (→ GA가 Success 모션)
	 * 클라가 먼저 성공 모션을 띄웠어도 보낸다 (GA가 같은 구간 두 번째 알림을 버린다). 실패는 보내지 않는다 (GA가 시간이 지나면 스스로 실패 모션)
	 */
	UFUNCTION(Client, Reliable)
	void ClientJustGuardResult(int32 WindowId);

	// ---------- 이 플레이어 컴퓨터 → 서버 ----------

	/**
	 * [이 플레이어 컴퓨터 → 서버] 피해자 판정 결과 "맞음"을 보고한다. 검증이 필요 없어 번호만 보낸다.
	 * 서버에서 보낸 플레이어의 캐릭터를 찾아 ULOAVictimJudgeSubsystem에 넘기기만 한다.
	 * @param RequestId  ClientRequestHitJudge로 받은 번호
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportHitJudgeHit(int32 RequestId);

	/**
	 * [이 플레이어 컴퓨터 → 서버] 피해자 판정 결과 "안 맞음"을 판정에 쓴 내 위치와 함께 보고한다. (서버가 위치로 검증)
	 * 서버에서 보낸 플레이어의 캐릭터를 찾아 ULOAVictimJudgeSubsystem에 넘기기만 한다.
	 * @param RequestId         ClientRequestHitJudge로 받은 번호
	 * @param ReportedLocation  판정에 쓴 내 위치 (1cm 단위로 압축)
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportHitJudgeMiss(int32 RequestId, FVector_NetQuantize ReportedLocation);

	/**
	 * [이 플레이어 컴퓨터 → 서버] 피해자 판정 결과 "모양 안이었지만 저스트 가드로 막음"을 보고한다.
	 * 서버 장부(사본)가 같은 규칙으로 다시 판단한다. 실패하면 맞은 걸로
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportHitJudgeGuarded(int32 RequestId);

	/**
	 * [이 플레이어 컴퓨터 → 서버] 저가 구간 WindowId에서 G를 눌렀다 (누른 서버 시각, LOANetTime). 저가 매니저에 넘기기만 한다
	 * "보낸 연결의 캐릭터"에 적는다 → 남의 장부에 쓸 수 없다
	 * @param PressYaw  누른 순간 보고 있던 방향을 1바이트로 줄인 값 (FRotator::CompressAxisToByte, 약 1.4° 단위). 저가 방향 판정용
	 *                  서버 캐릭터의 회전은 ASC(PlayerState 채널)의 커서 위치로 와서 이 보고보다 늦을 수 있다 → 같이 보낸다
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportJustGuardPress(int32 WindowId, double PressServerTime, uint8 PressYaw);

	/** [이 플레이어 컴퓨터 → 서버] G를 뗐다 / 취소했다(회피 등, bCancelled). 저가 매니저에 넘기기만 한다 */
	UFUNCTION(Server, Reliable)
	void ServerReportJustGuardRelease(int32 WindowId, double ServerTime, bool bCancelled);

	// ---------- [디버그 측정] (2026-10-29) 클라 시계 오차 재기 (콘솔 LOA.Debug.ClockCheck). 원인을 찾으면 지운다 ----------
	// UFUNCTION은 #if !UE_BUILD_SHIPPING 안에 둘 수 없어서 늘 들어 있다. 몸통은 Shipping에서 아무것도 안 한다

	/** [이 플레이어 컴퓨터 → 서버] 시계 확인 요청. 보낸 순간의 내 실제 시각을 그대로 돌려받는다 */
	UFUNCTION(Server, Reliable)
	void ServerDebugClockPing(double ClientRealTime);

	/** [서버 → 이 플레이어 컴퓨터] 받은 순간의 서버 시각과 함께 돌려줌 → 왕복 시간·내 시계 오차를 로그로 */
	UFUNCTION(Client, Reliable)
	void ClientDebugClockPong(double ClientRealTime, double ServerTime);

private:
	/** [서버] 전투 알림판: 주인 플레이어가 때린 피해 → 숫자 정보로 옮겨 담아 ClientShowCombatText (종류 Damage) */
	void HandleDamageDealt(const struct FLOADamageAppliedInfo& Info);

	/** [서버] 전투 알림판: 주인 플레이어가 받은 회복 → 들어온 양을 ClientShowCombatText (종류 Heal, 2026-11-16) */
	void HandleHealReceived(const struct FLOAHealAppliedInfo& Info);

	/** [서버] 전투 알림판: 주인 플레이어가 맞은 피해 → 들어온 양을 ClientShowCombatText (종류 DamageTaken, 빨강, 2026-11-16 apply118) */
	void HandleDamageReceived(const struct FLOADamageAppliedInfo& Info);

	/** 구독할 때 쓴 열쇠(주인 PlayerState)와 핸들 셋. 뗄 때 같은 열쇠로 (때린·맞은·회복 열쇠는 같은 PlayerState) */
	TWeakObjectPtr<const AActor> CombatNoticeKey;
	FDelegateHandle DamageDealtHandle;
	FDelegateHandle HealReceivedHandle;
	FDelegateHandle DamageReceivedHandle;

	/** [서버] 보고 두 개(맞음 / 안 맞음)의 공통: 보낸 PC의 캐릭터를 찾아 피해자 판정에 넘긴다. */
	void ForwardHitJudgeReport(int32 RequestId, bool bHit, const FVector& ReportedLocation) const;

	/** 이 PC의 지금 캐릭터에 붙은 저스트 가드 장부 (서버: 보낸 플레이어의 사본 / 클라: 내 원본). 없으면 nullptr */
	class ULOAJustGuardComponent* FindPawnJustGuardComponent() const;
};
