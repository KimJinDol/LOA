// LOAServerJudgeSubsystem.h
// [배관] 서버 판정. "서버가 아는 위치로 맞을 자리였나를 정한다." (2026-11-04, 판정 구조 개편으로 판정 담당에서 떼어 냄)
// 판정만 한다. 적용도, 저가도, 판정 매니저도 모른다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [누가 부르나]
//   판정 매니저(ULOACombatJudgeManagerSubsystem)가 대상을 찾은 뒤 Judge(공격, 대상들)를 부른다 → 대상마다 결과를 돌려준다
//   결과 = { 맞을 자리였나(서버 판정은 찾은 대상 전부 true), 그 사람 기준 시각, 판정에 쓴 위치 }
//
// [그 사람 기준 시각] 판정 시각 + GetSeenOffset = 그 사람 화면에서 공격이 닿은 서버 시각 (저가 "제때 눌렀나")
//   보스 모션 (Animation)  : 판정 시각 그대로 (보정 없음)
//     한 사람의 핑에 맞추면 다른 사람 결과(협동 저가 집계)가 늦어진다 → 핑이 높은 사람은 그만큼 일찍 눌러야 한다
//   예고 장판 (Telegraph)  : 판정 시각 − (그 사람 편도 + 예고 여유 Lead Margin)
//     예고를 그만큼 앞당겨 그리므로 그 사람 화면의 "다 참" 순간 (ULOATelegraphSubsystem [예고 앞당김])
//     앞으로 옮기기만 하므로 그 순간 이미 와 있는 누름으로 판정 → 집계가 늦어지지 않는다
//   리슨 호스트 본인, 플레이어가 아닌 대상 : 판정 시각 그대로 (서버 화면 = 자기 화면)
//   예) 편도 0.05, 여유 0.04, 판정 101.20 → 모션 101.20 / 예고 장판 101.11
//
// ─────────────────────────────────────────────────────────────────────────────
// [클라: 예고 장판이 내 화면에서 다 참] JudgeTelegraphLocal (2026-11-01, 옮김 2026-11-04)
//   예고 장판은 늘 서버 판정이라 저가 성공·CC 반응이 서버 결과를 받고 나왔다 (화면의 판정 순간 뒤 왕복 + 여유)
//   → 원격 클라는 예고가 내 화면에서 판정 시각에 도달한 순간(ULOATelegraphSubsystem::OnTelegraphHit) 스스로 본다
//     서버가 판정 순간 아는 내 위치 = 지금 내 위치 (예고 앞당김) → 내 판정이 서버와 같다
//   내 몸이 모양 안이면 "내 화면에서 맞을 자리" 알림(OnLocalHit)만 낸다. 보고는 안 보낸다 (서버가 스스로 판정한다)
//   그 뒤(저가 먼저 → 못 막았으면 CC 먼저 반응)는 판정 매니저가 정한다 (ULOACombatJudgeManagerSubsystem::ReactLocal)
//   보스 모션에 맞춰 그리는 예고(빨간 구역만, bSyncWithBossMotion)는 안 본다: 그 장판은 피해자 판정 요청이 따로 온다
//   이 일 때문에 함수가 아니라 서브시스템이다 (예고 알림 구독을 들고 있어야 한다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만 : Judge (결과를 돌려주기만. 적용은 판정 매니저)
//   2. 전달   : 없음 (예고는 ULOATelegraphSubsystem이 받는다)
//   3. 각자   : 원격 클라의 "예고 다 참" 판정 (보고 없음)
//   이 서브시스템은 복제되지 않는다. 컴퓨터마다 따로 있다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/LOAHitJudgeTypes.h"   // FLOAAttackInfo, FLOAJudgeResult, FLOALocalHit
#include "LOAServerJudgeSubsystem.generated.h"

struct FLOATelegraphData;
class APawn;

UCLASS()
class LOA_API ULOAServerJudgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 서버 판정. 게임 월드가 아니면 nullptr */
	static ULOAServerJudgeSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * [서버] 서버가 찾은 대상마다 판정 결과 하나 (맞을 자리 = true, 기준 시각, 지금 위치). 헤더 [그 사람 기준 시각]
	 * @param Hits  판정 매니저가 서버 위치로 찾은 대상 (피해자 판정으로 넘긴 원격 플레이어는 이미 빠졌다)
	 */
	void Judge(const FLOAAttackInfo& Attack, const TArray<FLOATargetHit>& Hits, TArray<FLOAJudgeResult>& OutResults) const;

	/** [클라] 예고 장판이 내 화면에서 다 찼고 내 몸이 모양 안. 판정 매니저가 듣는다 (돌려받는 "막았나"는 여기서 쓰지 않는다) */
	FLOAOnLocalHit OnLocalHit;

protected:
	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 만든다 */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	/** 예고 판정 알림 구독 (원격 클라인지는 알림 때 본다) */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem Interface

private:
	/** [서버] 그 사람 화면에서 공격이 닿은 시각 − 판정 시각 (초). 헤더 [그 사람 기준 시각] */
	static double GetSeenOffset(const APawn* Target, ELOAHitJudgeTiming Timing);

	/** [원격 클라] 예고가 내 화면에서 판정 시각에 도달 (ULOATelegraphSubsystem::OnTelegraphHit) */
	void JudgeTelegraphLocal(const FLOATelegraphData& Data);

	/** 예고 판정 알림 구독 번호 (Initialize ~ Deinitialize) */
	FDelegateHandle TelegraphHitHandle;
};
