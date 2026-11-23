// LOAAreaActor.h
// [배관] 범위 소환 액터. 소환된 자리에서, 정해진 시간에 자기 주변 범위를 판정해 Payloads를 준다.
//   한 방  : 낫을 떨어뜨림 (길로틴 스윙)   → Tick Count 1, First Tick Delay = 낫이 떨어지는 시간
//   장판   : 바닥에 남아 계속 데미지         → Tick Count 10, Tick Interval 0.5 …
//   보스 장판 : 내려찍기 (예고가 차오르다 꽉 차는 순간 판정) → 첫 판정 시각 = 몽타주 노티파이 (Spawn 모듈이 계산해 넘김)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP 만드는 법]
//   1) 이 클래스를 부모로 BP를 만든다. (예: BP_Area_GuillotineSwing, BP_Area_Grux_Slam)
//   2) 겉모습: 메시/나이아가라를 Root 아래에 붙인다. (콜리전은 끈다. 판정은 Targeting이 한다)
//   3) Class Defaults의 LOA|Area 칸
//        Targeting        [ Shape ▼ ]  Sphere/Fan/Box … (모양의 기준점 = 이 액터 위치, 방향 = 이 액터 앞 방향)
//        First Tick Delay 0.3          소환 후 첫 판정까지 (초). 0이면 소환 즉시
//                                      ※ Spawn 모듈에 First Hit Event를 고르면 이 칸 대신 몽타주 노티파이 시각을 쓴다
//        Tick Count       1            판정 횟수 (1 = 한 방)
//        Tick Interval    0.5          판정 사이 간격 (초). Tick Count 2 이상일 때만 보임
//        Linger Time      0.5          마지막 판정 뒤 사라지기까지 (초). 연출(낫이 박혀 있는 모습)용
//        Snap To Ground   ☑            소환되면 이 액터를 발밑 바닥으로 옮김 (겉모습이 공중에 뜨지 않게. 판정·예고 기준점은 Shape의 같은 칸)
//   4) 예고가 필요하면: Telegraph Style에 겉모습 에셋 (예: DA_Telegraph_BossDanger. 비우면 예고 없음)
//      예고 모양·크기·자리는 Targeting(Shape)의 판정 모양에서 저절로 정해진다. (판정과 같은 규칙: GetShapeCenter)
//      Hit Timing (예고가 있을 때만 보임, 2026-10-31):
//        Telegraph (기본) : 차오르는 예고가 꽉 차는 순간을 보고 피함 → 항상 서버 판정, 예고 앞당김
//        Animation        : 빨간 구역만 보여 주고 보스 모션을 보고 피함 → Targeting의 Hit Judge대로 (Victim 가능),
//                           예고는 보스 모션에 맞춰 그린다 (내 화면에서 도끼가 떨어지는 순간 사라짐)
//      Hit After Fill (예고가 있을 때만 보임, 2026-10-31): 꽉 찬 뒤 판정까지 (초). 0 = 꽉 참이 곧 판정
//        예) 꽉 차고 0.5초 뒤 터짐 (리듬) → Telegraph + 0.5 / 꽉 차고 보스가 내려찍음 → Animation + 그 시간
//   ※ 예고가 있는 공격은 전부 이 액터로 만든다. (보스가 그 자리에서 휘두르는 부채꼴도 발밑에 이 액터를 소환)
//     예고와 판정이 같은 객체·같은 자리·같은 시각이라 어긋날 수 없다. 예고 없는 즉발 공격은 Apply 모듈.
//   데미지는 여기서 정하지 않는다. → 이 BP를 소환하는 GA의 Spawn 모듈 Payloads (스킬마다 다른 배율)
//   범위 안의 대상은 판정할 때마다 전부 맞는다. (대상 수 제한 없음. 수 제한은 투사체의 MaxHitCount만)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   [서버] Spawn 모듈이 소환 (InitSkillActor로 Context/Payloads 복사. Context.SourceActor = 이 액터)
//            (First Hit Event를 고른 Spawn 모듈이면 SetFirstHitDelay로 "노티파이까지 남은 초"도 넘긴다)
//     → BeginPlay : 첫 판정까지의 초 = 넘겨받은 초, 없으면 First Tick Delay
//                   Telegraph Style이 있으면: 판정 모양 → 예고 모양으로 옮겨 ULOATelegraphSubsystem::ShowToAll(그 초)
//                   첫 판정 예약 (0이면 바로 판정)
//     → DoTick    : ULOACombatJudgeManagerSubsystem::JudgeAndApply(Targeting, Context, Payloads, Timing) → 범위 안 대상 전부에게 Payloads
//                   Timing: Telegraph Style이 있으면 Hit Timing 칸 (기본 Telegraph), 없으면 Animation (GetHitTiming)
//                   (판정 방식은 Targeting의 Hit Judge 칸. 이 액터는 모른다. Victim이면 원격 플레이어는 보고를 받은 뒤 적용됨)
//                   남은 횟수가 있으면 Tick Interval 뒤 다시 DoTick
//                   다 했으면 Linger Time 뒤 사라짐 (0이면 바로)
//     → EndPlay   : 첫 판정 전에 사라지면 (Spawn 모듈이 패턴 취소로 지움) 예고도 지운다.
//
//   예) 내려찍기: 서버 100.0초 소환, 노티파이까지 1.2초 → 예고 (100.0 → 101.2)
//       모두: 101.2초에 예고가 꽉 차고 사라짐 / 서버: 1.2초 타이머로 판정 (같은 서버 시계)
//
//   방향: Spawn 모듈이 소환할 때 "캐릭터가 보는 방향"으로 놓는다. → Fan 모양은 그 방향으로 펼쳐진다.
//   위치: 소환된 자리에 고정. 보스가 그 뒤에 움직여도 판정은 예고가 깔린 자리에서 난다. (로아 장판처럼)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만  : 판정, 데미지, 예고 보내기 (클라에는 Context가 없어서 판정할 수도 없다)
//   2. 전달    : 액터 자체는 복제(처음 위치·방향, 겉모습). 예고는 이 액터와 따로 간다. (ULOACombatRPCComponent Reliable RPC)
//   3. 각자    : 예고 그리기 (각 컴퓨터의 ULOATelegraphSubsystem)
//   움직이지 않으므로 이동 복제는 끈다. 서버가 Destroy하면 클라의 액터도 사라진다.
//   ※ 예고는 "서버 시각 + 내 편도 + 여유"로 차오른다 (ULOATelegraphSubsystem [예고 앞당김]).
//     내 이동이 편도만큼 늦게 서버에 닿으니, 서버 판정 순간에 서버가 아는 내 위치 = 내 화면에서 꽉 찬 순간의 위치.
//     그래서 예고가 꽉 차는 순간을 보고 피하는 장판(Hit Timing = Telegraph)은 항상 서버 판정이다 (2026-10-29).
//     Hit Judge를 Victim으로 두면 저장 시 오류 (IsDataValid).
//   ※ Hit Timing = Animation이면 예고를 앞당기지 않고 보스 모션에 맞춰 그린다 (ULOATelegraphSubsystem [보스 모션에 맞춰 그리기]. 2026-10-31)

#pragma once

#include "CoreMinimal.h"
#include "Actors/Skill/LOASkillActor.h"
#include "Combat/LOAHitJudgeTypes.h"   // ELOAHitJudgeTiming (칸 타입)
#include "LOAAreaActor.generated.h"

class ULOASkillTargeting;
class ULOATelegraphStyle;

UCLASS(Blueprintable)
class LOA_API ALOAAreaActor : public ALOASkillActor
{
	GENERATED_BODY()

public:
	ALOAAreaActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	/**
	 * [에디터 전용] BP 저장 시 검사.
	 *   오류 : Targeting 없음 / Tick Count 2 이상인데 Tick Interval이 0 이하
	 *          Hit Timing이 Telegraph(예고가 있음)인데 Targeting(Shape)의 Hit Judge가 Victim (예고가 꽉 차는 장판은 항상 서버 판정)
	 *   Targeting 칸 검사 : 고른 Targeting 종류의 ValidateTargeting (예: Fan의 Inner Radius)
	 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/**
	 * [서버] 첫 판정까지의 초를 First Tick Delay 대신 이 값으로. SpawnActorDeferred와 FinishSpawning 사이(BeginPlay 전)에 부른다.
	 * Spawn 모듈이 "몽타주의 First Hit Event 노티파이까지 남은 초"를 계산해 넘긴다. (예: 1.2)
	 */
	void SetFirstHitDelay(float Seconds);

	/** 판정을 한 번이라도 했는가. Spawn 모듈이 "패턴이 취소되면 아직 안 터진 장판을 지울지" 판단할 때 쓴다. */
	bool HasStartedHitting() const { return TicksDone > 0; }

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

private:
	/** [서버] 판정 1번 → 남았으면 다음 판정 예약, 다 했으면 사라지기 예약 */
	void DoTick();

	/**
	 * [서버] 판정 모양 → 예고 모양으로 옮겨 모든 플레이어 화면에 띄운다. (Telegraph Style이 비었으면 아무것도 안 함)
	 * @return 예고 번호. 띄우지 않았으면 0
	 */
	int32 ShowTelegraph(float SecondsUntilHit) const;

	/** 플레이어가 무엇을 보고 피하나. 예고가 없으면 보스 모션, 있으면 Hit Timing 칸 */
	ELOAHitJudgeTiming GetHitTiming() const { return TelegraphStyle ? HitTiming : ELOAHitJudgeTiming::Animation; }

	// ---------- 컴포넌트 ----------

	/** 루트. 겉모습(메시/나이아가라)을 BP에서 이 아래에 붙인다. */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Area")
	TObjectPtr<USceneComponent> Root;

	// ---------- 데이터 (BP의 Class Defaults에서) ----------

	/**
	 * 어디를 판정하나. 보통 Shape. 모양의 기준점 = 이 액터 위치, 방향 = 이 액터 앞 방향.
	 * 편 판단은 시전자 기준이다. (Context.Caster)
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "LOA|Area")
	TObjectPtr<ULOASkillTargeting> Targeting;

	/**
	 * 소환 후 첫 판정까지 (초). 0이면 소환 즉시 판정.
	 * 소환한 Spawn 모듈에 First Hit Event가 있으면 이 값 대신 몽타주 노티파이 시각을 쓴다. (보스 패턴)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area", meta = (ClampMin = "0", Units = "Seconds"))
	float FirstTickDelay = 0.f;

	/** 판정 횟수. 1 = 한 방 (기본). 장판처럼 여러 번 때리면 2 이상 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area", meta = (ClampMin = "1"))
	int32 TickCount = 1;

	/** 판정 사이 간격 (초). Tick Count가 2 이상일 때만 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area",
		meta = (ClampMin = "0.01", Units = "Seconds", EditCondition = "TickCount > 1", EditConditionHides))
	float TickInterval = 0.5f;

	/** 마지막 판정 뒤 사라지기까지 (초). 0이면 바로 사라진다. 판정 없이 연출만 남기는 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area", meta = (ClampMin = "0", Units = "Seconds"))
	float LingerTime = 0.5f;

	/**
	 * 소환되면 이 액터를 발밑 바닥으로 옮기나 (2026-11-01). 켜짐(기본) = 메시·나이아가라가 바닥에 붙는다 / 꺼짐 = 소환 위치 그대로.
	 * 서버 BeginPlay에서 한 번 (클라는 옮겨진 처음 위치를 복제로 받는다). 판정·예고 기준점은 Targeting Shape의 Snap To Ground가 따로 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area")
	bool bSnapToGround = true;

	/**
	 * 예고 겉모습. 고르면 첫 판정 전에 판정 모양 그대로 예고가 깔리고, 첫 판정 순간 꽉 찬다. 비우면 예고 없음.
	 * 예) 보스 장판 = DA_Telegraph_BossDanger / 플레이어 스킬 낫 = 비움
	 * 예고 모양은 Targeting이 Shape일 때만 알 수 있다. (다른 종류면 경고 + 예고 없음)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area")
	TObjectPtr<ULOATelegraphStyle> TelegraphStyle;

	/**
	 * 플레이어가 무엇을 보고 피하나 (예고가 있을 때만. 없으면 언제나 보스 모션)
	 *   Telegraph (기본) : 차오르는 예고가 꽉 차는 순간 → 항상 서버 판정, 예고를 앞당겨 그림
	 *   Animation        : 빨간 구역만 보여 주고 보스 모션을 보고 피함 → Hit Judge대로 (Victim 가능), 예고는 보스 모션에 맞춰 그림
	 *                      차오름 층이 없는 Style을 쓴다 (예: 빨강 한 층). 있어도 도끼가 떨어지는 순간 꽉 찬다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area", meta = (EditCondition = "TelegraphStyle != nullptr", EditConditionHides))
	ELOAHitJudgeTiming HitTiming = ELOAHitJudgeTiming::Telegraph;

	/**
	 * 예고가 꽉 찬 뒤 판정까지 (초). 0이면 꽉 참 = 판정 (기본). 예고는 판정까지 꽉 찬 채로 남는다.
	 *   예) 첫 판정 1.7초 뒤, 0.5 → 1.2초에 꽉 참, 0.5초 꽉 찬 채 → 1.7초에 터짐
	 *   Hit Timing과 같이: Telegraph = 꽉 찬 뒤 리듬을 보고 피함 (서버 판정, 예고 통째로 앞당김)
	 *                      Animation = 꽉 찬 뒤 보스 모션을 보고 피함 (Hit Judge대로, 보스 모션에 맞춰 그림)
	 * 첫 판정까지의 시간보다 크면 처음부터 꽉 찬 채로 깔린다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Area", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "TelegraphStyle != nullptr", EditConditionHides))
	float HitAfterFill = 0.f;

	// ---------- 런타임 ----------

	/** [서버] SetFirstHitDelay로 받은 초. 음수 = 받지 않음 → First Tick Delay */
	float FirstHitDelayOverride = -1.f;

	/** [서버] 지금까지 판정한 횟수 */
	int32 TicksDone = 0;

	FTimerHandle TickTimerHandle;

	/** [서버] 띄운 예고 번호. 첫 판정이 나면 0. 0이 아닌 채로 사라지면 = 판정 전에 지워짐 → 예고도 지움 */
	int32 TelegraphId = 0;
};
