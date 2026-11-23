// LOATelegraphSubsystem.h
// [배관] 예고 매니저. 컴퓨터마다 하나. 예고(ALOATelegraph)를 풀에 모아 두고 꺼내 주고 돌려받는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   입구 (서버, static)
//     ShowToAll(데이터, 몇 초 뒤) : 서버 시각·번호를 채워 모든 플레이어 화면으로 보낸다. 번호를 돌려준다.
//     HideToAll(번호)             : 판정 전에 취소된 예고를 모든 플레이어 화면에서 지운다.
//   각 컴퓨터
//   ShowTelegraph(데이터) : 그 Style의 풀에서 쉬고 있는 ALOATelegraph를 꺼내 데이터대로 그리게 한다.
//   HideTelegraph(번호)   : 판정 전에 취소된 예고를 풀로 돌려놓는다. (패턴 취소, 장판이 먼저 지워짐)
//   매 프레임 (Tick)      : ① MPC_Telegraph의 ServerTime에 지금 예고 시계(서버 시각 + 내 편도 + 여유)를 한 번 쓴다.
//                          ② 판정 시각이 지난 예고를 풀로 돌려놓는다. (판정 시각이 빠른 순으로 줄 세워 둠 → 앞에서부터만 본다)
//   예고가 하나도 없으면 Tick 자체를 하지 않는다.
//
//   모르는 것 : 판정 방법, 스킬 모듈, 범위 액터, ULOACombatLibrary (FLOATelegraphData만 받는다)
//   전송 통로로 ULOACombatRPCComponent(PC에 붙은 전투 RPC 통로)만 안다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [풀 - 왜 쓰나]
//   예) 보스가 장판 30개를 동시에 깐다. 매번 만들면 30개 × 2층 = 데칼 60개 + 머티리얼 사본 60개를 한 프레임에 만든다 → 멈칫.
//   풀은 미리 만들어 숨겨 둔 것을 꺼내 자리·크기·칸 값만 바꾼다. → 만드는 비용이 패턴 중에 생기지 않는다.
//   ※ 화면에 데칼이 많이 겹치는 그리기 비용(그래픽 카드)은 풀로 줄지 않는다.
//
//   Style마다 풀이 따로다. (층 머티리얼이 다르면 만들어 둔 머티리얼 사본을 다시 쓸 수 없다)
//   Style의 Pool Size만큼 미리 만들고, 모자라면 하나씩 더 만들며 경고를 한 번 남긴다.
//   미리 만드는 시점: Project Settings의 Prewarm Styles에 있으면 맵 시작 때 / 없으면 그 Style이 처음 쓰일 때.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] 예: 내려찍기 (원 350, 판정 1.2초 뒤)
//   [서버] ALOAAreaActor → ULOATelegraphSubsystem::ShowToAll(원 350, 장판 자리, 1.2초)
//          → 서버 시각 100.0 → 101.2, 번호 7
//          → 플레이어마다 ULOACombatRPCComponent::ClientShowTelegraph (Reliable)
//   [각 컴퓨터] ClientShowTelegraph → 이 서브시스템 ShowTelegraph → 풀에서 꺼내 그림
//               예고 시계로 101.2초가 지나면 (= 서버 시각 101.2 − 내 편도 − 여유) → 풀로 돌려놓음
//
// [예고 앞당김] (2026-10-28)
//   내 이동은 편도만큼 늦게 서버에 닿는다. 서버가 101.2에 판정할 때 아는 내 위치 = 내 화면의 101.2 − 편도 위치.
//   → 예고는 서버 판정보다 편도만큼 일찍 꽉 차야 "화면 = 서버 판정"이 된다.
//   예고 시계 = 서버 시각 + 내 편도 + 여유(0.04, 원격 클라만). 예고의 시작·꽉 참·사라짐을 전부 이 시계로 본다 (예고 길이는 그대로, 통째로 앞당김)
//   여유 (2026-10-29 측정): 편도만 앞당기면 약 0.02초(서버 프레임 하나)가 남는다 → Project Settings → LOA Telegraph → Lead Margin
//   그래서 예고 장판은 항상 서버 판정이다 (Targeting의 Hit Judge 칸과 상관없음. ULOACombatJudgeManagerSubsystem)
//     예) 편도 0.1, 서버 100.0~101.2 예고 → 내 화면 99.9~101.1. 받는 순간(서버 100.1) 이미 0.2초만큼 차 있다
//   리슨 호스트 = 편도 0 → 서버 시각 그대로
//   대가: 핑이 높을수록 내 화면의 예고가 짧다 (받는 데 편도 + 앞당김 편도 = 왕복만큼). 핑 200 → 1.2초 예고가 1.0초로 보임
//
// [보스 모션에 맞춰 그리기] (2026-10-31) FLOATelegraphData::bSyncWithBossMotion
//   빨간 구역만 보여 주고 보스 모션을 보고 피하는 장판 (장판 액터 Hit Timing = Animation, 피해자 판정 가능)
//   이 예고는 앞당기면 안 된다: 보스 모션은 편도만큼 늦게 보이는데 예고는 편도 + 여유만큼 일찍 사라진다 → 도끼보다 먼저 사라짐
//   → 이 컴퓨터에서 그릴 때 시작·꽉 참 시각을 (편도 × 2 + 여유)만큼 늦춘다 (원격 클라만, GetBossMotionDelay)
//       편도 + 여유 : 예고 시계의 앞당김을 되돌림
//       편도        : 보스 모션이 늦게 보이는 만큼
//     예) 편도 0.1, 여유 0.04, 서버 판정 101.2 → 이 예고는 예고 시계 101.44에 꽉 참 = 서버 시각 101.3 = 내 화면에서 도끼가 떨어지는 순간
//   차오름 층이 있는 Style이어도 그 순간에 꽉 찬다 (같은 MPC 시계를 쓰고 시각만 옮겼으므로)
//
// [꽉 참 ≠ 판정] (2026-10-31) 꽉 찬 뒤 일정 시간 후 터지는 장판 (장판 액터 Hit After Fill)
//   시작 ── 꽉 참(FillServerTime) ── 꽉 찬 채로 남음 ── 판정(HitServerTime) → 풀로 돌아감 + OnTelegraphHit
//   앞당김·보스 모션 맞춤은 세 시각을 통째로 옮긴다 → 판정 기준(Telegraph = 꽉 찬 뒤 리듬 / Animation = 보스 모션) 규칙 그대로
//
// [네트워크]
//   복제하지 않는다. 컴퓨터마다 따로 있고 받은 데이터만 그린다. (ULOARaidSubsystem과 같은 방식)
//   데디케이티드 서버는 화면이 없으므로 아무것도 만들지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Telegraph/LOATelegraphTypes.h"   // FLOATelegraphData (진행 중 예고 목록에 값으로 둔다)
#include "LOATelegraphSubsystem.generated.h"

class ALOATelegraph;
class ULOATelegraphStyle;
class UMaterialParameterCollection;

/** Style 하나의 풀 */
USTRUCT()
struct FLOATelegraphPool
{
	GENERATED_BODY()

	/** 쉬고 있는 예고 (숨겨져 있음) */
	UPROPERTY()
	TArray<TObjectPtr<ALOATelegraph>> Free;

	/** 이 Style로 지금까지 만든 수 (쉬는 것 + 그리는 중인 것) */
	int32 NumCreated = 0;

	/** Pool Size를 넘어 더 만들었다는 경고를 이미 남겼나 (한 번만) */
	bool bWarnedGrow = false;
};

/** 그리는 중인 예고 하나 */
USTRUCT()
struct FLOAActiveTelegraph
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ALOATelegraph> Telegraph = nullptr;

	/** 예고 번호 (HideTelegraph용). 0 = 번호 없음 */
	int32 TelegraphId = 0;

	/** 이 서버 시각이 지나면 풀로 돌려놓는다 */
	double HitServerTime = 0.0;

	/** 받은 예고 데이터 (판정 알림 OnTelegraphHit에 넘긴다) */
	FLOATelegraphData Data;
};

/** 예고가 이 컴퓨터 화면에서 판정 시각에 도달 (풀로 돌아가는 순간. 판정 전에 지워진 예고는 울리지 않는다) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOATelegraphHitDelegate, const FLOATelegraphData& /*Data*/);

UCLASS()
class LOA_API ULOATelegraphSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 예고 매니저. 게임 월드가 아니면 nullptr */
	static ULOATelegraphSubsystem* Get(const UObject* WorldContextObject);

	// ---------- 입구 (서버) ----------

	/**
	 * [서버 전용] 예고를 모든 플레이어 화면에 그린다.
	 * Data에는 Style, Shape, Location, Yaw (+ bSyncWithBossMotion, Attacker, AttackTags, CrowdControlEffect)만 채워 넘긴다. 번호와 시각은 여기서 채운다.
	 *   예) SecondsUntilHit 1.2, 지금 서버 시각 100.0 → 100.0에서 차오르기 시작, 101.2에 꽉 참 = 판정
	 *   예) SecondsUntilHit 1.7, SecondsHitAfterFill 0.5 → 101.2에 꽉 참, 101.7까지 꽉 찬 채 → 판정
	 * @param SecondsHitAfterFill  꽉 찬 뒤 판정까지 (초). 0이면 꽉 참 = 판정. SecondsUntilHit보다 크면 처음부터 꽉 참
	 * 플레이어마다 ULOACombatRPCComponent::ClientShowTelegraph (Reliable). 리슨 호스트 본인 화면은 바로 실행.
	 * @return 예고 번호 (판정 전에 취소되면 HideToAll에 넘긴다). 서버가 아니거나, 겉모습이 없거나, 0초 이하면 0
	 */
	static int32 ShowToAll(const UObject* WorldContextObject, FLOATelegraphData Data, float SecondsUntilHit, float SecondsHitAfterFill = 0.f);

	/** [서버 전용] 번호의 예고를 모든 플레이어 화면에서 지운다. 0이면 아무것도 안 함 */
	static void HideToAll(const UObject* WorldContextObject, int32 TelegraphId);

	// ---------- 각 컴퓨터 ----------

	/** 데이터대로 예고를 그린다. (각 컴퓨터. ULOACombatRPCComponent가 부른다) */
	void ShowTelegraph(const FLOATelegraphData& Data);

	/** 번호의 예고를 지금 지운다. (판정 전에 취소됨) 이미 사라졌으면 아무 일 없음 */
	void HideTelegraph(int32 TelegraphId);

	/**
	 * [각 컴퓨터] 예고가 화면에서 판정 시각에 도달한 순간 (꽉 찬 뒤 터지는 장판이면 꽉 참이 아니라 터지는 순간). 예고 쪽은 듣는 쪽이 무엇을 하는지 모른다
	 * 듣는 쪽: ULOAServerJudgeSubsystem::JudgeTelegraphLocal (원격 클라) → 판정 매니저가 저가 먼저 Success / CC 먼저 반응 (2026-11-04. 전에는 ULOAHitJudgeSubsystem)
	 */
	FLOATelegraphHitDelegate OnTelegraphHit;

	/**
	 * 지금 예고 시계 = 서버 시각 + 내 편도 + 여유 (원격 클라만). 헤더 [예고 앞당김]
	 * MPC 시계 쓰기, 판정 시각이 지난 예고 돌려놓기, "늦게 도착한 예고는 안 그림" 판단이 모두 이 값을 쓴다.
	 * 밖에서도 쓴다: 예고와 같은 순간에 무언가 바꿔야 하는 쪽 (ALOATerrainZone: 예고가 꽉 차는 순간 바닥을 없앰, 2026-11-14)
	 */
	double GetTelegraphTimeNow() const;

	//~ Begin FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject Interface

protected:
	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 만든다. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	/** 맵 시작: MPC 찾기 + Prewarm Styles 풀 채우기 */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem Interface

private:
	/** 화면이 있는 컴퓨터인가 (데디케이티드 서버면 false) */
	bool CanDraw() const;

	/** 보스 모션에 맞춰 그리는 예고를 늦출 시간 (초) = 편도 × 2 + 여유. 원격 클라만, 아니면 0. 헤더 [보스 모션에 맞춰 그리기] */
	double GetBossMotionDelay() const;

	/** MPC_Telegraph의 ServerTime 칸에 지금 예고 시계를 쓴다. (칸 이름은 머티리얼과의 약속이라 그대로) */
	void WriteServerTime();

	/** 그 Style의 풀을 Pool Size까지 채운다. */
	void FillPool(ULOATelegraphStyle* Style);

	/** 그 Style의 예고 하나를 새로 만든다 (숨겨진 채로) */
	ALOATelegraph* CreateTelegraph(ULOATelegraphStyle* Style);

	/** 숨기고 자기 Style의 풀로 돌려놓는다. (그리는 중 목록에서 빼는 일은 부르는 쪽이 한다) */
	void ReleaseToPool(ALOATelegraph* Telegraph);

	/** Style마다 풀 */
	UPROPERTY()
	TMap<TObjectPtr<ULOATelegraphStyle>, FLOATelegraphPool> Pools;

	/** 그리는 중인 예고. 판정 시각이 빠른 순서로 줄 서 있다. */
	UPROPERTY()
	TArray<FLOAActiveTelegraph> Active;

	/** MPC_Telegraph (Project Settings에서). 없으면 예고가 차오르지 않는다. */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> TimeCollection;

	/** MPC가 비었다는 경고를 남겼나 (한 번만) */
	bool bWarnedNoTimeCollection = false;
};
