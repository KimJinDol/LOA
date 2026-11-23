// LOABrawlArena.h
// [배관] 대난투 경기장 = 다른 차원. 레벨에 하나 놓는다. (2026-11-18 apply122, 대난투 ② 연출 + 차원 이동)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   보스와 플레이어 전원을 이 경기장으로 데려왔다가 원래 자리로 돌려보낸다. 그 사이의 화면 연출(슬로우·줌인·페이드) 시각을 정한다.
//   언제 시작할지는 모른다. 부르는 쪽(보스 그로기 GA ULOAGA_BossBrawl ← 무력화 0)이 StartBrawl을 부른다.
//   누가 듣는지 모른다: 다 돌아오면 OnBrawlEnded를 울리기만 한다.
//   (2026-11-18 apply125, ③) 진행도 여기서 센다: 단계·단계 게이지·누적 피해·남은 시간 (헤더 [진행])
//
// [모양] 사용자 그림 11-18 (실제 로아 대난투 화면): 좁고 긴 직선 통로, 배경에 파란 속도선
//   (2026-11-18 apply124 사용자) 보스는 통로 화면 위쪽 끝에서 아래(플레이어 쪽)를 본다
//   액터 자리 = 통로 위쪽 끝 바닥 윗면 = 보스가 서는 곳. 액터 앞(+X) = 보스가 보는 쪽 = 화면 아래 = 통로가 이어지는 쪽
//     카메라가 월드 +X를 보므로(붐 Yaw 0) 액터 Yaw 180. 바닥·벽은 BP에서 액터 앞쪽으로 깔려 있다 (바닥 가운데 = 액터 앞 2700)
//   플레이어 자리 = Player Spots (액터 기준 상대 위치, 레벨에서 점을 끌어 옮김). 사람이 자리 수보다 많으면 마지막 자리 옆으로 150씩
//   바닥·벽·속도선은 BP·레벨에서 이 밑에 붙인다 (바닥 메시는 Floor 프리셋, 둘레는 RaidWall, 보스 뒤는 PlayerWall = 플레이어만 막음. apply130)
//
// [흐름] 예: 슬로우 1.0 / 어두워짐 0.5 / 검은 화면 0.5 / 밝아짐 0.5, 서버 시각 100.0에 무력화 0
//   [서버] 100.0 StartBrawl : 상태 = 들어가는 중, 시작 시각 100.0 (복제) / 원래 자리 기억 / 플레이어 입력 막기 / 남은 장판·투사체 지우기
//   [각자] 100.0~101.0 슬로우(애니 재생 속도) + 내 카메라 줌인 → 101.0~101.5 어두워짐 → 검은 화면
//   [서버] 101.5 순간이동 : 보스 = 경기장 가운데, 플레이어 = Player Spots (죽은 사람도 시체 그대로. 사용자 결정 11-18)
//   [서버] 102.0 상태 = 진행 (시작 시각 102.0) / 입력 풀기 / 보스에 약점 공격(apply124)·대난투 태그(apply125) / 보스 남은 체력 적어 둠 / 진행 세기 시작   [각자] 102.0~102.5 밝아짐
//   [서버] 남은 시간 0 또는 누적 ≥ 적어 둔 체력 → EndBrawl : 약점 공격·대난투 태그 뗌 / 상태 = 나오는 중 → 같은 연출 → 원래 자리로 → 상태 = 전 → OnBrawlEnded
//          → 누적 피해를 보스 체력에 한 번에 (헤더 [진행])
//   각 컴퓨터는 "지금 서버 시각 − 상태 시작 시각"으로 연출을 계산한다 (정비소 화면 전환 ALOAGameState::GetTransferFadeAlpha와 같은 방식)
//   핑 200이면 0.1초 늦게 받아 슬로우 10% 지점부터. 순간이동 결과는 검은 화면 0.5초 안에 도착한다
//
// [진행] (2026-11-18 apply125, 사용자 결정. 숫자는 대신 정함: 30초, 1단계 3,000, 2단계 6,000)
//   진행 시작: 1단계, 남은 시간 Brawl Seconds, 보스 남은 체력을 적어 둔다 (Entry Health)
//     보스에 State.Brawl.Active + 약점 공격 → 데미지 계산이 ×2 하고 체력 대신 보스 스탯창의 대난투 입구로 보낸다 (체력은 그대로)
//   보스가 맞을 때마다 (전투 알림판 "받은 사람별" 구독, 대난투 피해만) 들어온 피해를 누적 피해와 지금 단계 게이지에 더한다
//     누적 피해는 남은 시간이 멈춘 협동 저가 패턴 중에도 센다 (사용자 11-18)
//     단계 게이지는 멈춘 동안 안 오른다 (2026-11-18 apply127 사용자: 협동 저가 결과가 나온 뒤 집계가 다시 시작될 때부터)
//   단계 게이지가 목표(Stage Goals[단계-1])를 넘으면 → 바로 다음 단계 (게이지 0부터, 넘친 양은 버림) + 남은 시간 멈춤 + OnStageAdvanced
//     → 보스 그로기 GA가 협동 저가 패턴을 쓰고, 끝나면 FinishCoopJustGuard(성공 인원) (2026-11-18 apply128)
//     → 등급(ULOAJustGuardSubsystem::GetCoopJustGuardGrade) → 남은 시간 + 등급별 추가 시간 (Coop Just Guard Bonus Seconds) → 다시 흐름
//        결과(등급·추가 시간)를 복제 → 협동 저가 결과 위젯이 보스 몸에 "PERFECT +5초" (입체 폰트) (ULOACoopJustGuardResultWidget)
//     패턴 중 피해는 누적에만, 게이지는 다시 흐를 때부터
//     예) 1단계 3,000 채움 → 2단계, 12.3초에서 멈춤 → 패턴 중 1,500 → 누적 +1,500, 2단계 게이지 0 그대로
//         → 2명 성공 = Great +3초 → 15.3초부터 다시 흐름, 이후 피해부터 게이지
//   마지막 단계(목표 수 + 1)는 다음 단계가 없다. 게이지 목표 = 보스 남은 체력 (apply127 사용자 A)
//     목표 = 입장 때 체력 − 마지막 단계에 들어가는 순간의 누적 피해. 다 차면 누적 ≥ 입장 때 체력 → 대난투 끝 → 돌아와서 보스 사망
//     예) 입장 20,000, 1·2단계 누적 9,000 → 마지막 목표 11,000. 5,500 → 50%
//         협동 저가 중 넣은 피해는 누적에만 들어가므로, 그만큼 게이지가 덜 찬 채로 보스가 죽을 수 있다 (사용자 A: 그대로)
//   끝: 남은 시간 0 (타이머) 또는 누적 ≥ 적어 둔 체력 (맞을 때 확인) → EndBrawl
//   예) 30초, 목표 [3,000, 6,000] → 1단계 3,000 채움(12초 남음, 멈춤) → 패턴 3초 → Perfect +5초 → 2단계 6,000 채움(10초 남음, 멈춤) → 패턴 → Bad +0초 → 3단계 10초 → 끝
//       누적 9,000 + 3단계 딜 = 화면 위 숫자. 원래 차원에 돌아온 뒤 보스 HP 1,000,000 → 990,000 근처
//   돌아온 뒤 체력에 적용 (사용자 11-18: 사망 연출·몽타주는 대난투를 벗어난 뒤): 체력 바꾸기 GE (−min(누적, 적어 둔 체력), 막타 = 마지막으로 친 사람)
//     → Common 스탯창의 원래 길로 체력 0이면 사망 이벤트 → 사망 GA·몽타주가 원래 차원에서
//   예) 적어 둔 체력 2,000, 누적 1,800에서 1,500 한 방 → 누적 3,300 ≥ 2,000 → 끝 → 돌아와서 체력 −2,000 → 사망
//       숫자·레이드 기록은 그 한 방 1,500 그대로 (스탯창은 대난투 누적을 모름)
//
// [슬로우 = 화면 연출만]
//   보스·플레이어 메시의 애니 재생 속도(GlobalAnimRateScale)만 늦춘다. 모든 컴퓨터가 같은 시각 규칙으로 같게
//   전체 시간 배율(Global Time Dilation)을 쓰지 않는 이유: 서버 계산(타이머·이동)까지 느려지고, 캐릭터 이동이 서버와 어긋나 위치 보정이 생긴다
//   그동안 새 입력은 서버가 막는다 (State.Block.Move · State.Block.Ability 태그 GE). 하던 스킬은 느리게 끝까지 간다
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크] 세 규칙
//   1. 서버만   : 시작·끝, 원래 자리 기억, 입력 막기·풀기, 장판 지우기, 순간이동, 약점 공격·대난투 태그 붙이기·떼기
//                 (태그는 ASC 복제로 모든 컴퓨터에 간다 → 보스 상태 아이콘 줄. 백·헤드·배율·입구 판단은 서버만)
//                 진행 세기 (맞을 때마다, 단계 넘기기, 시간 멈춤·다시), 돌아온 뒤 체력 적용
//   2. 복제     : RepState (상태 + 그 상태가 시작된 서버 시각). 레벨에 놓인 액터라 항상 관련(bAlwaysRelevant)
//                 Progress (단계, 단계 게이지, 누적 피해, 끝나는 서버 시각 / 멈춘 남은 시간). 대난투 UI가 읽는다
//                 CoopJustGuardResult (협동 저가 결과 번호 + 등급 + 추가 시간, apply128). 결과 위젯(보스 몸)이 번호가 바뀌면 띄운다
//                 순간이동 결과는 캐릭터 이동 복제 + ALOACharacter::RelocateBody(주인 클라 바로 옮김)
//   3. 각자     : 슬로우(애니 속도), 줌인(내 카메라, ALOAPlayerController), 페이드(ALOAPlayerController)
//                 남은 시간 = 끝나는 서버 시각 − 지금 서버 시각 (멈춤이면 멈춘 값), 대난투 UI 보이기 (ALOAHUD, OnPhaseChanged)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP 설정] BP_BrawlArena (부모 ALOABrawlArena)
//   액터 자리   : 통로 가운데 바닥 윗면. 앞(빨간 화살표) = 보스가 볼 쪽. 지금 경기장에서 멀리 (보이지 않게)
//   바닥·벽     : 바닥 스태틱 메시 (Floor 프리셋) + 둘레 Box (RaidWall, 숨김) + 보스 뒤 Box (PlayerWall: 보스 중앙선부터 뒤로 폭 전체, 보스와 겹쳐도 됨)
//   LOA|Brawl   : Player Spots, 연출 시간, Slow Anim Rate, Zoom Scale, Brawl Seconds, Stage Goals, Coop Just Guard Bonus Seconds

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "Library/LOACombatTypes.h"   // ELOAJustGuardGrade
#include "LOABrawlArena.generated.h"

class UArrowComponent;
class ALOABossCharacter;
class ALOACharacter;
class ALOABrawlArena;
class UAbilitySystemComponent;
struct FLOADamageAppliedInfo;

/** 대난투 상태 */
UENUM(BlueprintType)
enum class ELOABrawlPhase : uint8
{
	None		UMETA(DisplayName = "None"),		// 대난투 아님 (원래 차원)
	Entering	UMETA(DisplayName = "Entering"),	// 들어가는 중 (슬로우 → 어두워짐 → 순간이동)
	Active		UMETA(DisplayName = "Active"),		// 진행 중 (다른 차원)
	Exiting		UMETA(DisplayName = "Exiting"),		// 나오는 중 (슬로우 → 어두워짐 → 원래 자리)
};

/** 복제하는 상태 (상태와 시각이 같이 도착하게 하나로 묶음) */
USTRUCT()
struct FLOABrawlRepState
{
	GENERATED_BODY()

	UPROPERTY()
	ELOABrawlPhase Phase = ELOABrawlPhase::None;

	/** 이 상태가 시작된 서버 시각 (초). 연출 계산 기준 */
	UPROPERTY()
	double PhaseStartServerTime = 0.0;
};

/** 복제하는 진행 (2026-11-18 apply125, 헤더 [진행]). 진행 상태가 시작될 때 처음부터 */
USTRUCT()
struct FLOABrawlProgress
{
	GENERATED_BODY()

	/** 지금 단계 (1부터). 0 = 아직 진행 전 */
	UPROPERTY()
	uint8 Stage = 0;

	/** 지금 단계 게이지에 쌓인 피해 (단계가 넘어가면 0) */
	UPROPERTY()
	float StageDamage = 0.f;

	/** 이번 대난투 누적 피해 (화면 위 큰 숫자) */
	UPROPERTY()
	float TotalDamage = 0.f;

	/** 남은 시간이 0이 되는 서버 시각 (초). 멈춰 있으면 읽지 않는다 */
	UPROPERTY()
	double TimerEndServerTime = 0.0;

	/** 멈췄을 때의 남은 시간 (초). 협동 저가 패턴 동안 */
	UPROPERTY()
	float PausedRemainingSeconds = 0.f;

	UPROPERTY()
	bool bTimerPaused = false;

	/** 마지막 단계 게이지 목표 = 입장 때 체력 − 마지막 단계에 들어가는 순간의 누적 피해 (apply127). 마지막 단계 전에는 0 */
	UPROPERTY()
	float LastStageGoal = 0.f;
};

/** 복제하는 협동 저가 결과 (2026-11-18 apply128). 결과 위젯(ULOACoopJustGuardResultWidget)이 번호가 바뀌면 등급·추가 시간을 띄운다 */
USTRUCT()
struct FLOACoopJustGuardResult
{
	GENERATED_BODY()

	/** 결과마다 1씩 (255 다음 0). 같은 등급이 두 번 나와도 UI가 새 결과로 안다 */
	UPROPERTY()
	uint8 ResultId = 0;

	UPROPERTY()
	ELOAJustGuardGrade Grade = ELOAJustGuardGrade::None;

	/** 이번 결과로 남은 시간에 더한 초 */
	UPROPERTY()
	float BonusSeconds = 0.f;
};

/** [서버] 대난투가 끝나고 원래 자리로 다 돌아왔다 */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOABrawlEndedNative, ALOABrawlArena* /*Arena*/);

/** [서버] 다음 단계로 넘어감 (남은 시간 멈춤). 듣는 쪽: 보스 그로기 GA (협동 저가 패턴 → FinishCoopJustGuard) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FLOABrawlStageAdvancedNative, ALOABrawlArena* /*Arena*/, int32 /*NewStage*/);

/** [모든 컴퓨터] 상태가 바뀜 (서버·호스트는 바꿀 때, 클라는 복제 도착 때). 듣는 쪽: ALOAHUD (대난투 UI ↔ 보스 체력바) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOABrawlPhaseChangedNative, ALOABrawlArena* /*Arena*/);

UCLASS()
class LOA_API ALOABrawlArena : public AActor
{
	GENERATED_BODY()

public:
	ALOABrawlArena(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 서버 ----------

	/** [서버] 대난투를 시작한다 (들어가는 중). 이미 대난투 중이면 false */
	bool StartBrawl(ALOABossCharacter* Boss);

	/** [서버] 대난투를 끝낸다 (나오는 중 → 원래 자리). 진행 중이 아니면 아무것도 안 한다 */
	void EndBrawl();

	/** [서버] 원래 자리로 다 돌아온 순간 한 번 */
	FLOABrawlEndedNative OnBrawlEnded;

	/** [서버] 다음 단계로 넘어감. 남은 시간은 멈춰 있다 → 듣는 쪽이 FinishCoopJustGuard(또는 ResumeBrawlTimer)를 불러야 다시 흐른다 */
	FLOABrawlStageAdvancedNative OnStageAdvanced;

	/**
	 * [서버] 협동 저가 패턴이 끝남 (apply128). 성공 인원 → 등급 → 추가 시간을 더해 다시 흐르게 + 결과 복제 (UI)
	 * 진행 중이 아니거나 멈춰 있지 않으면 아무것도 안 함. 예) 성공 3명 → Perfect → +5초
	 */
	void FinishCoopJustGuard(int32 SuccessCount);

	/** [서버] 추가 시간 없이 다시 흐르게 (협동 저가 패턴을 못 썼을 때). 멈춰 있지 않으면 아무것도 안 함 */
	void ResumeBrawlTimer();

	// ---------- 모든 컴퓨터 ----------

	ELOABrawlPhase GetPhase() const { return RepState.Phase; }

	/** 상태가 바뀜 (헤더 [네트워크] 3) */
	FLOABrawlPhaseChangedNative OnPhaseChanged;

	/** 대난투 UI를 보일 때인가 = 진행 중 · 나오는 중 (보스 체력바 대신) */
	bool ShouldShowBrawlUI() const { return RepState.Phase == ELOABrawlPhase::Active || RepState.Phase == ELOABrawlPhase::Exiting; }

	/** 지금 단계 (1부터, 0 = 진행 전) / 단계 수 (목표 수 + 1 = 마지막은 게이지 없음) */
	int32 GetStage() const { return Progress.Stage; }
	int32 GetStageCount() const { return StageGoals.Num() + 1; }
	bool IsLastStage() const { return Progress.Stage >= GetStageCount(); }

	/** 지금 단계 게이지 (0 ~ 1). 마지막 단계는 보스 남은 체력 기준 (헤더 [진행]) */
	float GetStageGaugePercent() const;

	/** 이번 대난투 누적 피해 */
	float GetTotalDamage() const { return Progress.TotalDamage; }

	/** 남은 시간 (초). 진행 중이 아니면 0, 멈춰 있으면 멈춘 값 */
	float GetRemainingSeconds() const;

	/** 남은 시간이 멈춰 있나 (협동 저가 패턴) */
	bool IsTimerPaused() const { return Progress.bTimerPaused; }

	/** 마지막 협동 저가 결과 (번호가 바뀌면 새 결과. 협동 저가 결과 위젯) */
	const FLOACoopJustGuardResult& GetCoopJustGuardResult() const { return CoopJustGuardResult; }

	/** 화면을 얼마나 검게 (0 ~ 1). 헤더 [흐름]. ALOAPlayerController가 매 프레임 읽는다 */
	float GetFadeAlpha() const;

	/** 내 카메라 거리 배율 (1 = 그대로, Zoom Scale = 가장 가까움). ALOAPlayerController가 매 프레임 읽는다 */
	float GetCameraZoomScale() const;

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

#if WITH_EDITORONLY_DATA
	/** 에디터에서만 보이는 화살표 = 보스가 보는 쪽 */
	UPROPERTY()
	TObjectPtr<UArrowComponent> EditorArrow;
#endif

	// ---------- 레벨에서 고르는 칸 ----------

	/** 플레이어 서는 자리 (액터 기준 상대 위치, 바닥 높이). 레벨에서 점을 끌어 옮긴다. 기본 = 보스 앞 500, 좌우 150 간격 (대신 정함) */
	UPROPERTY(EditAnywhere, Category = "LOA|Brawl", meta = (MakeEditWidget = "true"))
	TArray<FVector> PlayerSpots;

	// ---------- BP 기본값 (헤더 [흐름]. 전부 대신 정함 11-18) ----------

	/** 슬로우 + 줌인 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "0.0", Units = "Seconds"))
	float SlowSeconds = 1.f;

	/** 어두워지는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "0.05", Units = "Seconds"))
	float FadeOutSeconds = 0.5f;

	/** 검은 화면 시간 (초). 순간이동 결과가 도착할 여유 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "0.05", Units = "Seconds"))
	float BlackSeconds = 0.5f;

	/** 밝아지는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "0.05", Units = "Seconds"))
	float FadeInSeconds = 0.5f;

	/** 슬로우 동안 애니 재생 속도 (1 = 그대로) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SlowAnimRate = 0.3f;

	/** 줌인이 다 됐을 때 카메라 거리 배율 (0.6 = 60% 거리) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ZoomScale = 0.6f;

	/** 진행 시간 (초). 협동 저가 패턴 동안은 멈춘다 (헤더 [진행]. 대신 정함 11-18, 사용자 OK) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "1.0", Units = "Seconds"))
	float BrawlSeconds = 30.f;

	/**
	 * 단계마다 넘어가는 데 필요한 피해 (2배 된 값 기준). 개수 + 1 = 단계 수 (마지막은 목표 없음)
	 * 기본 [3,000, 6,000] = 3단계 (대신 정함 11-18, 사용자 OK: 개발하기 쉽게 작게. 밸런스는 나중)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "1.0"))
	TArray<float> StageGoals;

	/** 협동 저가 등급별 추가 시간 (초, apply128). 없는 등급 = 0. 기본 Perfect 5 / Great 3 / Bad 0 (대신 정함 11-18) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl")
	TMap<ELOAJustGuardGrade, float> CoopJustGuardBonusSeconds;

	/** 자리 수보다 사람이 많을 때 마지막 자리 옆으로 벌리는 간격 (cm) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Brawl", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float ExtraSpotSpacing = 150.f;

private:
	UFUNCTION()
	void OnRep_RepState();

	/** [서버] 상태 바꾸기 + 시작 시각 = 지금 */
	void SetPhase(ELOABrawlPhase NewPhase);

	/** "지금 서버 시각 − 상태 시작 시각" (초) */
	double GetSecondsInPhase() const;

	/** 들어가는 중 / 나오는 중에서 순간이동까지의 초 = 슬로우 + 어두워짐 */
	float GetSecondsUntilTeleport() const { return SlowSeconds + FadeOutSeconds; }

	/** [서버] 들어가는 중 → 순간이동 → (검은 화면) → 진행 */
	void TeleportIn();
	void FinishEntering();

	/** [서버] 나오는 중 → 원래 자리 → (검은 화면) → 전 */
	void TeleportBack();
	void FinishExiting();

	/** [서버] 플레이어 전원 입력 막기 / 풀기 (State.Block.Move + State.Block.Ability 태그 GE) */
	void BlockPlayerInput();
	void UnblockPlayerInput();

	/**
	 * [서버] 보스에 진행 태그 붙이기 / 떼기 (태그 GE 하나, 진행 상태 동안만)
	 *   State.Debuff.WeakPoint (약점 공격, apply124): 백·헤드 어택 스킬이 방향과 상관없이 보너스 (ULOACombatLibrary::DecideAttackJudgeRecord)
	 *   State.Brawl.Active (apply125): 받는 피해 × 대난투 배율 (ULOADamageExecution)
	 */
	void ApplyActiveBossTags();
	void RemoveActiveBossTags();

	/**
	 * [서버] 누적 피해를 보스 체력에 한 번에 (원래 차원 도착 뒤, apply125). 체력 바꾸기 GE (ULOAGE_HealthChange)
	 *   양 = −min(누적, 적어 둔 체력). 막타 = 마지막으로 친 사람. 체력 0이면 Common 스탯창이 사망 이벤트를 보낸다
	 */
	void ApplyTotalDamageToBoss(ALOABossCharacter* Boss);

	/** [서버] 보스가 맞음 (전투 알림판 "받은 사람별", 진행 중). 대난투 피해면 세고, 누적 ≥ 적어 둔 체력이면 EndBrawl */
	void HandleBossDamaged(const FLOADamageAppliedInfo& Info);
	void SubscribeBossDamage(ALOABossCharacter* Boss);
	void UnsubscribeBossDamage();

	/** [서버] 다음 단계 + 남은 시간 멈춤 + OnStageAdvanced */
	void AdvanceStage();

	/** [서버] 멈춘 남은 시간 + BonusSeconds로 다시 흐르게 (ResumeBrawlTimer · FinishCoopJustGuard) */
	void ResumeBrawlTimerWithBonus(float BonusSeconds);

	/** 이 단계의 목표 피해. 마지막 단계면 Progress.LastStageGoal, 범위 밖이면 0 */
	float GetStageGoal(int32 Stage) const;

	/** [서버] 게임에 들어와 있는 플레이어 몸 (살아 있든 죽었든) */
	TArray<ALOACharacter*> GetPlayerCharacters() const;

	/** [서버] Index번 플레이어가 설 자리 (월드, 몸 중심 높이). 헤더 [모양] */
	FVector GetPlayerSpotLocation(int32 Index, const ALOACharacter* Character) const;

	/** [모든 컴퓨터] 보스·플레이어 메시 애니 재생 속도 (슬로우). 바뀔 때만 */
	void ApplyAnimRate(float Rate);

	UPROPERTY(ReplicatedUsing = OnRep_RepState)
	FLOABrawlRepState RepState;

	/** 진행 (헤더 [진행]). UI가 매 프레임 읽기만 해서 도착 알림이 필요 없다 */
	UPROPERTY(Replicated)
	FLOABrawlProgress Progress;

	/** 마지막 협동 저가 결과 (apply128). 번호는 대난투가 바뀌어도 이어진다 (UI가 옛 결과를 새 것으로 보지 않게) */
	UPROPERTY(Replicated)
	FLOACoopJustGuardResult CoopJustGuardResult;

	/** [서버] 지금 대난투 중인 보스 */
	TWeakObjectPtr<ALOABossCharacter> BrawlBoss;

	/** [서버] 원래 자리 (들어갈 때 기억) */
	FTransform BossReturnTransform;
	TMap<TWeakObjectPtr<ALOACharacter>, FVector> PlayerReturnLocations;

	/** [서버] 입력 막기 GE 번호표 (플레이어 ASC → 번호표) */
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> InputBlockHandles;

	/** [서버] 진행 태그 GE 번호표 (보스 ASC). 붙인 ASC를 같이 들고 있다 (뗄 때 그 ASC에서) */
	TWeakObjectPtr<UAbilitySystemComponent> BossTagsASC;
	FActiveGameplayEffectHandle ActiveTagsHandle;

	/** [서버] 전투 알림판 구독 (열쇠 = 보스 ASC의 주인) */
	TWeakObjectPtr<const AActor> BossDamageKey;
	FDelegateHandle BossDamagedHandle;

	/** [서버] 진행 시작 때 보스 남은 체력 (사용자 11-18: 누적이 이만큼 되면 끝) */
	float EntryHealth = 0.f;

	/** [서버] 마지막으로 보스를 친 사람 (돌아온 뒤 체력 적용의 막타) */
	TWeakObjectPtr<AActor> LastHitter;

	FTimerHandle PhaseTimerHandle;

	/** [모든 컴퓨터] 마지막으로 넣은 애니 속도 (같으면 다시 안 넣음) */
	float AppliedAnimRate = 1.f;
};
