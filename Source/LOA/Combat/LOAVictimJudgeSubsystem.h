// LOAVictimJudgeSubsystem.h
// [배관] 피해자 판정. "맞는 플레이어의 화면으로 맞았나를 정하고, 서버가 검사한다." (2026-11-04, 판정 구조 개편으로 판정 담당에서 떼어 냄)
// 판정만 한다. 적용도, 저가도, 판정 매니저도 모른다. 결과는 알림(OnTargetJudged)으로 올린다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 있나]
// 서버가 아는 플레이어 위치는 지연만큼 늦다. → 내 화면에서 피했는데 서버 판정으로 맞는다.
// 보스 모션을 보고 피하는 공격(Hit Judge = Victim + Animation)은 맞는 사람 화면으로 판정한다.
//
// [누가 부르나] 판정 매니저(ULOACombatJudgeManagerSubsystem)
//   HoldRemoteVictims : 서버가 찾은 대상에서 원격 플레이어를 빼서 서버 결과·위치를 적어 둔다 (보내지는 않는다)
//   SendVictimRequests   : 판정 매니저가 "판정 시작"을 알린 뒤 요청을 보낸다 (저가 누르기 마감 "닫힘"이 요청보다 먼저 가게)
//   결과는 대상마다 꼭 한 번 OnTargetJudged로 올린다 (보고 / 위치 기다림 끝 / 마감 중 하나)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] 예: 판정 101.2초, 모션 공격, Shape(Hit Judge = Victim), 원격 플레이어 A·B, 리슨 호스트 H, 허수아비 D
//
//   [서버] 101.2  판정 매니저: 서버 위치로 대상 찾기 → H, A, D 맞음
//                  HoldRemoteVictims: 원격 플레이어 A, B를 빼서 서버 결과와 서버 위치를 적는다: A=맞음 (300,0), B=안 맞음 (900,0)  → 공격 번호 7
//                  (H, D는 판정 매니저가 서버 판정으로)
//                  SendVictimRequests: A, B에게 ClientRequestHitJudge(7, 모양, 자리, 방향, 101.2)
//   [A]    101.3  요청 도착 = 내 화면에서 보스 공격이 닿은 순간 → JudgeLocal: 지금 위치 (560,0) → 모양 밖 → ServerReportHitJudgeMiss(7, (560,0))
//   [서버] 101.4  ReceiveReport(A, 7, 안 맞음, (560,0))
//                   서버 결과(맞음)와 다르다 → 검증
//                   ① 시간: 판정 뒤 0.2초 → A의 핑 0.2 + 여유 0.3 안
//                   ② 각도·거리: IsBodyInShape(모양, 자리, 방향, (560,0)) → 정말 모양 밖
//                   ③ 위치: 서버가 지금 아는 A 위치 (550,0)과 10cm → 150cm 안 (보고와 A의 이동이 같이 도착했으므로 거의 같다)
//                   → 클라 판정(안 맞음) 인정 → 결과 "피함"
//   [서버] 102.2  마감: B 보고 없음 → 적어 둔 서버 결과(안 맞음) → 결과 "피함"
//
//   [클라 판정 + 서버 검증] 판정은 클라가 한다. 서버는 받아들일지만 정한다.
//     클라 "맞음"                     → 그대로 (거짓 "맞음"은 본인만 손해). 계산 없음
//     클라 "안 맞음" + 서버 "안 맞음" → 그대로. 계산 없음
//     클라 "안 맞음" + 서버 "맞음"    → 검증 (2026-10-31)
//         ① 시간: 판정 뒤 (그 플레이어 핑 + ReportTimeMargin 0.3초) 안에 왔나       → 실패: 서버 결과(맞음) + 경고
//         ② 모양: 보고 위치가 정말 모양 밖 (IsBodyInShape: 원 = 거리, 부채꼴 = 각도 + 거리, 사각형 = 앞뒤·좌우) → 실패: 서버 결과 + 경고
//         ③ 위치: 보고 위치 ↔ 보고가 도착한 순간 서버가 아는 그 플레이어 위치, ReportLocationTolerance(150cm, 바닥 거리) 안
//              안   → 클라 판정
//              밖   → 바로 정하지 않고 ReportCatchUpTime(0.1초)까지 매 틱 다시 본다 (ResolveCatchUps)
//                     서버 위치가 따라옴 → 클라 판정 / 끝까지 안 옴 → 서버 결과(맞음) + 경고
//       왜 "보고 도착 순간" 위치: 보고와 그 순간까지의 이동은 같이 클라를 떠나 같이 도착한다 → 정상이면 거의 같다.
//         차이는 서버가 이동을 한두 프레임 늦게 받은 만큼. 핑·이동 속도·순간이동과 상관없다.
//       왜 기다리나: 이동 패킷은 Unreliable이라 사라지면 서버 위치가 다음 이동이 올 때까지 멈춰 있다.
//       왜 시간 제한: 없으면 보고를 늦추고 그사이 진짜로 걸어 나간 뒤 보고해도 ③을 통과한다.
//   그 전에: 기다리는 중인 (판정 번호, 그 플레이어)가 있는가 → 없으면 버림 (중복, 틀린 번호, 마감 지남, 대상 아닌 플레이어)
//   막을 수 없는 것: 시간 여유 안에서 보고를 늦추고 그만큼 실제로 움직이기 (0.3초 ≈ 걷기 180cm), 150cm 안의 거짓 위치.
//     랙으로 늦은 보고와 일부러 늦춘 보고는 서버가 구분할 수 없다 → 정상 플레이어도 보고가 재전송될 만큼 늦으면 서버 결과.
//     즉사기·기믹은 Hit Judge = Server.
//
// [판정 시각 = 플레이어가 본 순간]
//   요청을 받은 순간의 내 위치 → 내 화면에서 보스 공격이 닿은 순간
//     보스 몽타주 시작과 판정 요청은 같은 서버 → 클라 길로 온다. 그래서 내 화면의 보스 동작은 서버보다 편도만큼 늦고,
//     서버에서 도끼가 닿은 순간 보낸 요청도 편도만큼 늦게 도착한다 → "요청 도착 = 내 화면에서 도끼가 닿은 순간".
//
// [올리는 결과] 대상 하나마다 FLOAJudgeResult 하나 (판정 매니저가 확정: 저가 확인 → 적용)
//   보고 "맞음"           → 맞을 자리 + 클라가 맞음이라 함 (저가 컴포넌트는 맞은 시각만 적는다)
//   보고 "막음"           → 맞을 자리 (서버 저가 컴포넌트가 다시 판단)
//   보고 "안 맞음" 인정   → 피함 / 거절 → 맞을 자리 (서버 결과)
//   마감                  → 서버 결과 그대로 (맞을 자리면 서버 저가 컴포넌트가 판단)
//   기준 시각 = 판정 시각 + 그 사람 편도 (최대 0.2초, 보스 모션을 편도만큼 늦게 본다), 위치 = 판정 순간 서버 위치
//
// [클라: 요청이 왔을 때 내 화면으로 판정] JudgeLocal
//   모양 안이면 "내 화면에서 맞을 자리" 알림(OnLocalHit) → 판정 매니저가 저가 먼저 → 못 막았으면 CC 먼저 반응
//   돌려받은 "막았나"로 보고를 고른다: 막음 → ServerReportHitJudgeGuarded / 맞음 → ServerReportHitJudgeHit / 모양 밖 → ServerReportHitJudgeMiss(위치)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만  : 원격 플레이어 나누기, 서버 결과·위치 적어 두기, 보고 검증(시간·모양·위치, 위치가 안 맞으면 잠깐 기다림), 마감
//   2. 전달    : 판정 요청 (서버 → 그 플레이어, ULOACombatRPCComponent::ClientRequestHitJudge, Reliable)
//                보고 = 맞음: 번호 (ServerReportHitJudgeHit) / 안 맞음: 번호 + 판정에 쓴 위치 (ServerReportHitJudgeMiss)
//                       막음: 번호 (ServerReportHitJudgeGuarded). 그 플레이어 → 서버, Reliable
//   3. 각자    : 내 판정 (각 클라, 요청을 받은 순간 위치로)
//   이 서브시스템은 복제되지 않는다. 컴퓨터마다 따로 있다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Library/LOACombatTypes.h"      // FLOAHitShape
#include "Combat/LOAHitJudgeTypes.h"     // FLOAHitJudgeRequestData, FLOAAttackInfo, FLOAJudgeResult, FLOAOnLocalHit
#include "LOAVictimJudgeSubsystem.generated.h"

class ULOASkillTargeting;
class APawn;

/** [서버] 피해자 판정 → 판정 매니저: 대상 하나의 결과가 정해졌다 (대상마다 꼭 한 번) */
DECLARE_DELEGATE_TwoParams(FLOAOnVictimJudged, int32 /*AttackId*/, const FLOAJudgeResult& /*Result*/);

/** [서버] 보고를 기다리는 플레이어 한 명 */
struct FLOAHitJudgeVictim
{
	TWeakObjectPtr<APawn> Victim;

	/** 서버 판정 결과 (보고가 안 오거나, 보고가 검증에 실패하면 이걸 쓴다) */
	bool bServerHit = false;

	/** 판정 순간 서버가 아는 위치. 저가 방향 판정에 쓴다. (보고 검증은 보고가 도착한 순간의 위치로 한다) */
	FVector ServerLocation = FVector::ZeroVector;
};

/** [서버] 피해자 판정 요청 하나 (보고를 기다리는 사람이 있는 동안만 남는다). 번호 = 공격 번호 */
struct FLOAHitJudgeRequest
{
	int32 AttackId = 0;

	/** 판정 모양. 클라 판정을 검증할 때 쓴다. */
	FLOAHitShape Shape;

	/** 모양 기준점. 검증 + 맞은 지점(데미지 숫자 위치) 계산용 */
	FVector ShapeCenter = FVector::ZeroVector;

	/** 모양 방향 (도) */
	float Yaw = 0.f;

	/** 판정 서버 시각 */
	double HitServerTime = 0.0;

	/** 이 서버 시각이 지나면 보고가 안 온 사람은 서버 결과대로 */
	double DeadlineServerTime = 0.0;

	TArray<FLOAHitJudgeVictim> Victims;
};

/** [서버] 위치가 안 맞아서, 서버 위치가 따라오길 잠깐 기다리는 "안 맞음" 보고 하나. 헤더 [클라 판정 + 서버 검증] ③ */
struct FLOAHitJudgeCatchUp
{
	/** 그 요청의 복사본 (결과를 만들 때. Victims는 비움) */
	FLOAHitJudgeRequest Request;

	/** 보고한 플레이어 */
	FLOAHitJudgeVictim Entry;

	/** 보고 위치 */
	FVector ReportedLocation = FVector::ZeroVector;

	/** 이 서버 시각까지 안 따라오면 서버 결과(맞음) */
	double GiveUpServerTime = 0.0;
};

/** [서버] 보고 검사 결과 (ValidateReport) */
enum class ELOAReportCheck : uint8
{
	Trust,	// 클라 결과를 쓴다
	Reject,	// 서버 결과를 쓴다
	Wait,	// 위치만 안 맞음 → 서버 위치가 따라오길 기다린다
};

/** 위치 기록 한 칸 ([디버그 측정] 서버 위치 기록이 쓴다) */
struct FLOAPositionSample
{
	double ServerTime = 0.0;
	FVector Location = FVector::ZeroVector;
};

UCLASS()
class LOA_API ULOAVictimJudgeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 피해자 판정. 게임 월드가 아니면 nullptr */
	static ULOAVictimJudgeSubsystem* Get(const UObject* WorldContextObject);

	// ---------- 서버 (판정 매니저가 부른다) ----------

	/**
	 * [서버] 이 공격의 대상인 원격 플레이어를 Hits에서 빼서 서버 결과(맞음/안 맞음)와 서버 위치를 적어 둔다. 아직 보내지 않는다.
	 * 리슨 호스트 본인은 빼지 않는다. (서버 화면 = 자기 화면이라 서버 판정이 곧 자기 판정)
	 * @param Targeting  Shape여야 한다 (모양·편)
	 * @return 적어 둔 사람 수 (= 나중에 올릴 결과 수). 0이면 아무것도 남기지 않는다
	 */
	int32 HoldRemoteVictims(const ULOASkillTargeting* Targeting, const FLOAAttackInfo& Attack, TArray<FLOATargetHit>& Hits);

	/**
	 * [서버] HoldRemoteVictims로 적어 둔 사람들에게 판정 요청을 보낸다 (판정 매니저가 "판정 시작"을 알린 뒤)
	 * @param Attack  HoldRemoteVictims에 넘긴 것과 같은 공격 (공격한 보스·속성·CC GE를 요청에 싣는다)
	 */
	void SendVictimRequests(const FLOAAttackInfo& Attack);

	/**
	 * [서버] 보고를 받는다. (ULOACombatRPCComponent::ServerReportHitJudgeHit / ServerReportHitJudgeMiss가 부른다)
	 * 받은 보고를 ValidateReport로 검사해서 통과하면 클라 결과, 실패하면 서버 결과를 올린다.
	 * 위치만 안 맞으면 CatchUps에 넣고 잠깐 기다린다 (ResolveCatchUps).
	 * @param ReportedLocation  "안 맞음"일 때만 의미 있음 (맞음 보고는 위치를 보내지 않는다)
	 */
	void ReceiveReport(APawn* Victim, int32 RequestId, bool bReportedHit, const FVector& ReportedLocation);

	/**
	 * [서버] 보고 "모양 안이었지만 저스트 가드로 막음"을 받는다. (ULOACombatRPCComponent::ServerReportHitJudgeGuarded가 부른다)
	 * "맞을 자리"로 올린다. 막았는지는 판정 매니저의 확정에서 서버 저가 컴포넌트가 다시 판단한다 (실패하면 맞음)
	 */
	void ReceiveGuardReport(APawn* Victim, int32 RequestId);

	/** [서버] 대상 하나의 결과 (판정 매니저가 듣는다) */
	FLOAOnVictimJudged OnTargetJudged;

	// ---------- 각 클라 ----------

	/**
	 * [클라] 판정 요청: 받은 순간의 내 위치로 판정해 보고한다. (ULOACombatRPCComponent::ClientRequestHitJudge가 부른다)
	 * 모양 안이면 OnLocalHit → 돌려받은 "막았나"로 "막음" / "맞음" 보고를 고른다. 헤더 [클라: 요청이 왔을 때 내 화면으로 판정]
	 */
	void JudgeLocal(const FLOAHitJudgeRequestData& Request);

	/** [클라] 요청이 왔고 내 화면에서 맞을 자리였다 (판정 매니저가 듣는다) */
	FLOAOnLocalHit OnLocalHit;

	//~ Begin FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject Interface

protected:
	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 만든다. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem Interface

private:
	/**
	 * [서버] 클라 보고를 믿을 수 있나 검사한다. ReceiveReport가 부른다. 헤더 [클라 판정 + 서버 검증]
	 *   "맞음" 보고 / 서버도 "안 맞음"           → Trust (다툴 게 없다)
	 *   "안 맞음" 보고 + 서버 "맞음" (피했다는 주장) → ① 시간 ② 모양 실패면 Reject / ③ 위치가 안 맞으면 Wait / 다 통과면 Trust
	 */
	ELOAReportCheck ValidateReport(const FLOAHitJudgeRequest& Request, const FLOAHitJudgeVictim& Entry,
		bool bReportedHit, const FVector& ReportedLocation) const;

	/**
	 * [서버] 한 사람의 결과를 만들어 올린다 (OnTargetJudged). 헤더 [올리는 결과]
	 * @param Request  그 요청의 복사본 (올리는 중에 Requests가 바뀌어도 안전하게)
	 */
	void FinishReport(const FLOAHitJudgeRequest& Request, const FLOAHitJudgeVictim& Entry, bool bInRange, bool bClientSaidHit) const;

	/** [서버] 기다리는 중인 (번호, 그 플레이어)를 목록에서 꺼낸다. 없으면 false (중복, 틀린 번호, 마감 지남, 대상 아닌 플레이어) */
	bool TakeVictim(int32 RequestId, APawn* Victim, FLOAHitJudgeRequest& OutSnapshot, FLOAHitJudgeVictim& OutEntry);

	/** [서버] 본 시각 − 판정 시각 (초) = 그 플레이어의 편도 (최대 MaxSeenDelay). 보스 모션을 편도만큼 늦게 본다 */
	static double GetSeenDelay(const APawn* Victim);

	/** [서버] 마감이 지난 요청 처리: 보고가 안 온 사람은 적어 둔 서버 결과대로 */
	void ResolveExpired(double Now);

	/** [서버] 위치를 기다리는 보고 처리: 서버 위치가 보고 위치에 왔으면 클라 결과, 기다림이 끝났으면 서버 결과 */
	void ResolveCatchUps(double Now);

	/** [서버] 보고를 기다리는 요청들 (공격 번호 → 요청) */
	TMap<int32, FLOAHitJudgeRequest> Requests;

	/** [서버] 위치가 따라오길 기다리는 "안 맞음" 보고들 (보통 비어 있다) */
	TArray<FLOAHitJudgeCatchUp> CatchUps;

#if !UE_BUILD_SHIPPING
	// ---------- [디버그 측정] (2026-10-28) 예고 앞당김 뒤 클라·서버 위치 어긋남 원인 찾기. 찾으면 지운다 ----------

	/** [서버] 원격 플레이어 위치 기록 (기다리는 요청이 있을 때만) */
	void DebugRecordServerPositions(double Now);

	/** [서버] "안 맞음" 보고 위치에 서버 기록이 판정 뒤 몇 초에 도달했나 로그 */
	void DebugLogServerCatchUp(const FLOAHitJudgeRequest& Request, const FLOAHitJudgeVictim& Entry, const FVector& ReportedLocation) const;

	TMap<TWeakObjectPtr<APawn>, TArray<FLOAPositionSample>> DebugServerHistory;
#endif
};
