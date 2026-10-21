// LOAHitJudgeSubsystem.h
// [배관] 판정 담당. "이 Targeting으로 대상을 판정하고, 맞은 대상에게 Payloads를 준다."
// 부르는 쪽(Apply 모듈, 장판 액터, 나중에 투사체)이 무엇인지 모른다. 판정 방식(서버 / 피해자)은 Targeting의 칸을 보고 여기서 정한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 있나]
// 서버가 아는 플레이어 위치는 지연만큼 늦다. → 내 화면에서 피했는데 서버 판정으로 맞는다.
// 피해자 판정(맞는 플레이어의 컴퓨터가 자기 위치로 판정)을 공격 종류마다 따로 만들지 않으려고 한곳에 둔다.
//
// [부르는 쪽]
//   ULOAHitJudgeSubsystem::JudgeAndApply(Targeting, Context, Payloads, Timing)   ← FindTargets + ApplyPayloads 대신 이 한 줄
//   → 결과가 바로 나는 대상은 바로, 보고를 기다리는 대상은 나중에 Payloads가 적용된다. 부르는 쪽은 차이를 모른다.
//   → Context와 Payloads는 여기서 복사해 들고 있으므로, 부른 GA가 먼저 끝나거나 장판 액터가 먼저 사라져도 된다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] 예: 판정 101.2초, Shape(Hit Judge = Victim), 원격 플레이어 A·B, 리슨 호스트 H, 허수아비 D
//
//   [서버] 101.2  JudgeAndApply
//                   Targeting->FindTargets → H, A, D 맞음 (서버가 아는 위치)
//                   원격 플레이어 A, B는 빼서 서버 결과와 서버 위치를 적어 둔다: A=맞음 (300,0), B=안 맞음 (900,0)  → 판정 번호 7
//                   H, D는 바로 Payloads
//                   A, B에게 ClientRequestHitJudge(7, 모양, 자리, 방향, 101.2)
//   [A]    101.3  JudgeLocal: 기록에서 "101.2의 내 위치" (560,0)을 꺼냄 → 모양 밖 → ServerReportHitJudgeMiss(7, (560,0))
//   [서버] 101.4  ReceiveReport(A, 7, 안 맞음, (560,0))
//                   서버 결과(맞음)와 다르다 → 검증
//                   ① 거리: 서버 위치 (300,0)과 260cm → 300cm 안
//                   ② 각도·거리: IsBodyInShape(모양, 자리, 방향, (560,0)) → 정말 모양 밖
//                   → 클라 판정(안 맞음) 인정, 데미지 없음
//   [서버] 102.2  마감: B 보고 없음 → 적어 둔 서버 결과(안 맞음) 사용
//
//   [클라 판정 + 서버 검증] 판정은 클라가 한다. 서버는 받아들일지만 정한다.
//     클라 "맞음"                     → 그대로 (거짓 "맞음"은 본인만 손해). 계산 없음
//     클라 "안 맞음" + 서버 "안 맞음" → 그대로. 계산 없음
//     클라 "안 맞음" + 서버 "맞음"    → 검증: ① 보고 위치가 서버 위치에서 MaxReportDistance(300cm, 바닥 거리) 안
//                                             ② 보고 위치가 정말 모양 밖 (IsBodyInShape: 원 = 거리, 부채꼴 = 각도 + 거리, 사각형 = 앞뒤·좌우)
//                                       둘 다 통과 → 클라 판정 / 하나라도 실패 → 서버 결과(맞음) + 경고
//   그 전에: 기다리는 중인 (판정 번호, 그 플레이어)가 있는가 → 없으면 버림 (중복, 틀린 번호, 마감 지남, 대상 아닌 플레이어)
//   막을 수 없는 것: 서버 위치에서 300cm 안의 거짓 위치. (한가운데서 "멀리 있었다"는 못 함) 즉사기·기믹은 Hit Judge = Server.
//
// [위치 기록] (각 클라)
//   클라는 판정 요청을 판정 시각보다 늦게 받는다. (편도 지연만큼) 그래서 내 캐릭터 위치를 최근 1.5초치 매 프레임 기록해 두고,
//   요청받은 시각의 위치를 꺼내 판정한다. 기록의 시각은 LOANetTime(= 예고가 차오르는 시계)이다.
//   예) 기록 [101.18 → (0,0), 101.21 → (30,0)], 요청 101.2 → 둘 사이 → (20,0)
//
// [판정 시각 = 플레이어가 본 순간] ELOAHitJudgeTiming
//   예고 공격 (Telegraph) : 기록에서 판정 서버 시각의 내 위치 → 내 화면에서 예고가 꽉 찬 순간
//   모션 공격 (Animation) : 요청을 받은 순간의 내 위치 → 내 화면에서 보스 공격이 닿은 순간
//     보스 몽타주 시작과 판정 요청은 같은 서버 → 클라 길로 온다. 그래서 내 화면의 보스 동작은 서버보다 편도만큼 늦고,
//     서버에서 도끼가 닿은 순간 보낸 요청도 편도만큼 늦게 도착한다 → "요청 도착 = 내 화면에서 도끼가 닿은 순간".
//     (처음에는 서버가 "판정 시각 + 핑 ÷ 2"로 짐작했다. 핑에는 처리 대기 시간도 들어 있어 실제보다 커서,
//      판정 시각이 클라에게 아직 안 온 미래가 됐다. 예: 짐작 5.97, 실제 도착 5.92)
//   어느 쪽인지는 부르는 쪽이 넘긴다. (장판 액터: 예고를 띄우면 Telegraph / Apply 모듈: Animation)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만  : 판정 요청 만들기, 서버 결과·위치 적어 두기, 보고 검증(의견이 다를 때 거리·각도), 마감 처리, Payloads 적용
//   2. 전달    : 판정 요청 (서버 → 그 플레이어, ULOACombatRPCComponent::ClientRequestHitJudge, Reliable)
//                보고 = 맞음: 번호 (ServerReportHitJudgeHit) / 안 맞음: 번호 + 판정에 쓴 위치 (ServerReportHitJudgeMiss). 그 플레이어 → 서버, Reliable
//   3. 각자    : 내 위치 기록, 내 판정 (각 클라)
//   이 서브시스템은 복제되지 않는다. 컴퓨터마다 따로 있다. (ULOATelegraphSubsystem과 같은 방식)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // Context / Payload를 값으로 가지므로 include
#include "Library/LOACombatTypes.h"                                // FLOAHitShape
#include "LOAHitJudgeSubsystem.generated.h"

class ULOASkillTargeting;
class APawn;

/**
 * 플레이어가 무엇을 보고 피하나. 피해자 판정에서 "클라의 어느 시각 위치로 판정할지"가 달라진다.
 * 클라 화면에는 시계가 두 개다: 예고는 서버 시각에 맞춰져 있고(LOANetTime), 보스 몸(몽타주)은 편도 지연만큼 늦게 보인다.
 *   예) 핑 200ms, 서버 판정 101.2 → 예고는 내 화면 101.2에 꽉 참 / 도끼는 내 화면 101.3에 떨어짐
 */
UENUM()
enum class ELOAHitJudgeTiming : uint8
{
	/** 예고를 보고 피한다 → 판정 서버 시각의 내 위치 (예: 101.2) */
	Telegraph,
	/** 보스 모션을 보고 피한다 → 요청을 받은 순간의 내 위치 (= 내 화면에서 도끼가 떨어진 순간. 예: 101.3) */
	Animation,
};

/** [서버] 보고를 기다리는 플레이어 한 명 */
USTRUCT()
struct FLOAHitJudgeVictim
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<APawn> Victim;

	/** 서버 판정 결과 (보고가 안 오거나, 보고 위치가 서버 위치에서 너무 멀면 이걸 쓴다) */
	bool bServerHit = false;

	/** 판정 순간 서버가 아는 위치. 보고 위치가 이것과 너무 다르면 보고를 믿지 않는다. */
	FVector ServerLocation = FVector::ZeroVector;
};

/** [서버] 판정 하나 (보고를 기다리는 사람이 있는 동안만 남는다) */
USTRUCT()
struct FLOAHitJudgeRequest
{
	GENERATED_BODY()

	/** 결과가 나면 Payloads를 줄 때 쓴다. 복사본. (UPROPERTY: 시전자가 사라지면 GC가 비운다) */
	UPROPERTY()
	FLOASkillModuleContext Context;

	UPROPERTY()
	TArray<FLOAEffectPayload> Payloads;

	/** 판정 모양. 클라 판정을 검증할 때 쓴다. */
	FLOAHitShape Shape;

	/** 모양 기준점. 검증 + 맞은 지점(데미지 숫자 위치) 계산용 */
	FVector ShapeCenter = FVector::ZeroVector;

	/** 모양 방향 (도) */
	float Yaw = 0.f;

	/** 이 서버 시각이 지나면 보고가 안 온 사람은 서버 결과대로 */
	double DeadlineServerTime = 0.0;

	UPROPERTY()
	TArray<FLOAHitJudgeVictim> Victims;
};

/** [클라] 내 위치 기록 한 칸 */
struct FLOAPositionSample
{
	double ServerTime = 0.0;
	FVector Location = FVector::ZeroVector;
};

UCLASS()
class LOA_API ULOAHitJudgeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 판정 담당. 게임 월드가 아니면 nullptr */
	static ULOAHitJudgeSubsystem* Get(const UObject* WorldContextObject);

	// ---------- 입구 (서버) ----------

	/**
	 * [서버 전용] Targeting으로 대상을 판정하고, 맞은 대상에게 Payloads를 준다.
	 * Targeting이 Shape이고 Hit Judge = Victim이면 원격 플레이어는 보고를 받은 뒤(또는 마감에) 준다.
	 * 그 밖에는 FindTargets + ApplyPayloads와 같다.
	 * @param Context   누가, 몇 레벨로. SourceActor = 모양을 놓는 액터 (캐릭터 또는 장판 액터)
	 * @param Timing    플레이어가 무엇을 보고 피하나. 예고를 띄운 공격 = Telegraph, 모션만 보이는 공격 = Animation (Victim일 때만 쓴다)
	 * @return 지금 바로 Payloads를 준 대상 수 (로그용). 서버가 아니면 0
	 */
	static int32 JudgeAndApply(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
		const TArray<FLOAEffectPayload>& Payloads, ELOAHitJudgeTiming Timing);

	/**
	 * [서버] 보고를 받는다. (ULOACombatRPCComponent::ServerReportHitJudgeHit / ServerReportHitJudgeMiss가 부른다)
	 * 클라 판정을 쓴다. 단 "안 맞음"인데 서버는 맞았다고 본 경우만 위치로 검증하고, 실패하면 서버 결과.
	 * @param ReportedLocation  "안 맞음"일 때만 의미 있음 (맞음 보고는 위치를 보내지 않는다)
	 */
	void ReceiveReport(APawn* Victim, int32 RequestId, bool bReportedHit, const FVector& ReportedLocation);

	// ---------- 각 클라 ----------

	/** [클라] 판정 요청: 그 시각의 내 위치로 판정해 결과와 위치를 보고한다. (ULOACombatRPCComponent::ClientRequestHitJudge가 부른다) */
	void JudgeLocal(int32 RequestId, const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw,
		double HitServerTime, bool bJudgeOnArrival);

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
	 * [서버] 이 판정의 대상인 원격 플레이어를 찾아 서버 결과(맞음/안 맞음)와 서버 위치를 OutRequest에 적고, 맞았으면 Hits에서 뺀다.
	 * 리슨 호스트 본인은 빼지 않는다. (서버 화면 = 자기 화면이라 서버 판정이 곧 자기 판정)
	 * @return 적어 둔 수
	 */
	int32 HoldRemoteVictims(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
		TArray<FLOATargetHit>& Hits, FLOAHitJudgeRequest& OutRequest) const;

	/** [서버] 플레이어 한 명에게 그 판정의 Payloads (기다리는 사이 죽었거나 시전자가 사라졌으면 안 줌) */
	void ApplyToVictim(const FLOAHitJudgeRequest& Request, APawn* Victim) const;

	/** [서버] 마감이 지난 판정 처리: 보고가 안 온 사람은 적어 둔 서버 결과대로 */
	void ResolveExpired(double Now);

	/** [클라] 내 캐릭터 위치를 기록에 추가하고 오래된 칸을 버린다. */
	void RecordLocalPosition(double Now);

	/** [클라] 기록에서 그 시각의 내 위치. 기록이 없으면 false */
	bool FindLocalPositionAt(double ServerTime, FVector& OutLocation) const;

	/** [서버] 보고를 기다리는 판정들 (판정 번호 → 판정) */
	UPROPERTY()
	TMap<int32, FLOAHitJudgeRequest> Requests;

	/** [서버] 마지막으로 쓴 판정 번호 (1부터. 0 = 번호 없음) */
	int32 LastRequestId = 0;

	/** [클라] 내 위치 기록. 오래된 것이 앞 */
	TArray<FLOAPositionSample> PositionHistory;
};
