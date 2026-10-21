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
//   매 프레임 (Tick)      : ① MPC_Telegraph의 ServerTime에 지금 서버 시각을 한 번 쓴다.
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
//               101.2초(서버 시각)가 지나면 → 풀로 돌려놓음
//
// [네트워크]
//   복제하지 않는다. 컴퓨터마다 따로 있고 받은 데이터만 그린다. (ULOARaidSubsystem과 같은 방식)
//   데디케이티드 서버는 화면이 없으므로 아무것도 만들지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LOATelegraphSubsystem.generated.h"

class ALOATelegraph;
class ULOATelegraphStyle;
class UMaterialParameterCollection;
struct FLOATelegraphData;

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
};

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
	 * Data에는 Style, Shape, Location, Yaw만 채워 넘긴다. 번호와 시각은 여기서 채운다.
	 *   예) SecondsUntilHit 1.2, 지금 서버 시각 100.0 → 100.0에서 차오르기 시작, 101.2에 꽉 참
	 * 플레이어마다 ULOACombatRPCComponent::ClientShowTelegraph (Reliable). 리슨 호스트 본인 화면은 바로 실행.
	 * @return 예고 번호 (판정 전에 취소되면 HideToAll에 넘긴다). 서버가 아니거나, 겉모습이 없거나, 0초 이하면 0
	 */
	static int32 ShowToAll(const UObject* WorldContextObject, FLOATelegraphData Data, float SecondsUntilHit);

	/** [서버 전용] 번호의 예고를 모든 플레이어 화면에서 지운다. 0이면 아무것도 안 함 */
	static void HideToAll(const UObject* WorldContextObject, int32 TelegraphId);

	// ---------- 각 컴퓨터 ----------

	/** 데이터대로 예고를 그린다. (각 컴퓨터. ULOACombatRPCComponent가 부른다) */
	void ShowTelegraph(const FLOATelegraphData& Data);

	/** 번호의 예고를 지금 지운다. (판정 전에 취소됨) 이미 사라졌으면 아무 일 없음 */
	void HideTelegraph(int32 TelegraphId);

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

	/**
	 * 지금 서버 시각. = LOANetTime::GetServerTimeNow (피해자 판정과 같은 시계)
	 * MPC 시계 쓰기와 "늦게 도착한 예고는 안 그림" 판단이 둘 다 이 값을 쓴다.
	 */
	double GetServerTimeNow() const;

	/** MPC_Telegraph의 ServerTime 칸에 지금 서버 시각을 쓴다. */
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
