// LOAJustGuardComponent.h
// [배관] 저스트 가드 장부. 플레이어 캐릭터에 붙는다. (2026-10-26)
// 저가 구간(ID)마다 "언제 눌렀나 · 언제 뗐나 · 취소했나 · 언제 맞았나"를 적어 두고, 들어온 타격을 막았나를 스스로 판단한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [저가는 맞은 사람의 상태] (2026-11-04 판정 구조 개편)
//   판정은 "맞을 자리였나"만 정한다. 맞을 자리였으면 판정 매니저(ULOACombatJudgeManagerSubsystem)가 이 컴포넌트에 묻는다:
//     TryJustGuard(들어온 타격) → 막았나
//   판정 쪽은 저가 구간 번호를 들고 다니지 않는다. 공격한 보스 + 공격 속성 + 시각만 주고, 구간은 여기서 찾는다.
//   ① 저가 공격인가 (공격 속성 Skill.Attribute.JustGuard)                    아니면 못 막음
//   ② 그 보스의 구간 찾기
//        서버 = 저가 매니저(ULOAJustGuardSubsystem::FindWindowAt)에 "이 보스, 판정 시각에 열려 있던 구간"을 물음
//        클라 = 내가 받은 구간 알림 중 이 보스의 가장 최근 구간 (구간 알림에 그 보스가 같이 온다)
//   ③ 맞은 시각 적기 (AddHitTime)                                             ← 홀딩의 기준 타격은 여기서만 정해진다
//   ④ 클라가 이미 "맞음"이라 했으면 여기서 끝 (맞은 시각만 적고 판단 안 함)
//   ⑤ 막았나 판단 (IsBlocked. 기록은 바꾸지 않는다. 묻기만 했는데 기록이 바뀌는 일 없앰)
//   ⑥ 막음 알리기: 이 컴퓨터의 GA → 성공 모션. 서버 사본이면 그 플레이어 클라에도 (ClientJustGuardResult, 항상)
//        클라가 이미 성공 모션을 띄웠으면 GA가 같은 구간 두 번째 알림을 버린다 (ULOAGA_JustGuard::OnGuardSucceeded)
//
// ─────────────────────────────────────────────────────────────────────────────
// [기록 하나 = 저가 구간 하나]
//   #7  단타  보스 A  열림   누름 9.75  뗌 9.85   맞은 시각 10.00
//   #8  홀딩  보스 A  열림   누름 12.30 (누르는 중)  맞은 시각 12.60, 12.90, 13.20
//   한 구간에 한 번만 누를 수 있다 → "한 번 누름 = 한 공격"이 구조로 지켜진다
//
// [원본과 사본]
//   내 컴퓨터 : 원본. 열린 구간 목록 + 내 누름 (리슨 호스트는 이게 곧 서버 기록)
//   서버      : 사본. 원격 플레이어의 보고를 저가 매니저(ULOAJustGuardSubsystem)가 검사한 뒤 여기 적는다
//   복제되지 않는다. 값은 RPC로만 오간다.
//
// [왜 GA가 아니라 장부인가]
//   GA(몸통)는 모션이 끝나면 사라지는데, 판정은 그 뒤에 올 수 있다 (피해자 판정 보고는 최대 1초 뒤).
//   그래서 기록은 캐릭터에 남긴다.
//
// [판정 규칙] IsBlocked(기록, 본 시각, 공격 기준점, 내 위치)
//   본 시각(그 사람 화면에서 공격이 닿은 시각)은 판정 방식이 정해서 준다 (서버 판정 / 피해자 판정 / 내 화면). 여기서는 계산하지 않는다
//   막기 유효 시간 = 0.5초 (Project Settings → LOA Combat → Just Guard Window)
//
//   단타               : 기준 = 이 타격.  기준 − 0.5 ≤ 누른 시각 ≤ 기준,  그 전에 취소(피격 이상으로 끊김)되지 않음
//   홀딩 (첫 타부터)   : 기준 = 이 사람에게 처음 닿은 타격.  기준 − 0.5 ≤ 누른 시각 ≤ 기준,  이번 타격 때 아직 누르고 있음
//   홀딩 (중간부터)    : 기준 = 누른 뒤 처음 닿은 타격 (누른 시각 + 0.5초 안).  나머지는 홀딩과 같다. 기준보다 앞에 맞은 타격은 맞음 (2026-11-04)
//                        이 방식은 첫 타 뒤에도 눌러야 하므로 누르기 마감을 하지 않는다 (ULOAJustGuardSubsystem [누르기 마감])
//   방향 : 공격 기준점이 캐릭터 정면 ±45° 안 (Project Settings → LOA Combat → Just Guard Facing Half Angle) (2026-10-27)
//          저가 GA는 누를 때 커서 쪽으로 돌고, 저가 중에는 돌지 않는다 → 커서를 공격 쪽에 두고 눌러야 한다
//          정면 = 기록의 PressYaw (누른 순간 방향. 서버 사본은 누름 보고에 같이 온다)
//            → 서버 판정·피해자 판정·서버 재판정·마감 판정이 모두 같은 정면을 본다 (저가가 끝난 뒤 돌아도 상관없음)
//          위치 = 부르는 쪽이 준다 (서버 판정·클라 판정 = 지금 위치 / 서버 재판정·마감 = 판정 순간 서버 위치)
//          홀딩은 타격마다 본다 (돌덩이가 여기저기 떨어지면 정면 밖 돌은 못 막음)
//          기준점이 내 몸 안(캡슐 반지름 안)이면 방향이 없다 → 방향 검사 통과
//
//   예) 단타, 기준 10.00 → 9.50 ~ 10.00에 누르면 막음
//       9.80 누름 + 9.90 띄움에 맞아 끊김 → 못 막음 (취소 뒤 공격)
//   예) 홀딩 (첫 타부터), 첫 타 10.00 → 9.70 누름 → 10.0·10.3·10.6 막음 → 10.7 뗌 → 10.9 맞음
//       첫 타 뒤 10.20에 누름 → 그 뒤 타격도 전부 맞음
//   예) 홀딩 4연타 10.0 / 10.3 / 10.6 / 10.9, 10.2에 누르고 계속 누름
//       첫 타부터 : 기준 10.0 → 10.2는 늦음 → 4타 전부 맞음
//       중간부터  : 10.0 맞음 → 기준 10.3 → 10.3 · 10.6 · 10.9 막음
//
// [클라 먼저 성공 모션] 원격 클라는 서버 결과를 기다리지 않고 내 화면에서 먼저 판단한다
//   피해자 판정 요청 도착 / 예고 장판이 내 화면에서 다 참 → 판정 매니저가 이 컴포넌트에 TryJustGuard (ULOACombatJudgeManagerSubsystem::ReactLocal)
//   서버 사본도 같은 규칙으로 따로 판단한다 (같은 누름·같은 기준이라 거의 일치)
//   틀렸을 때 (드묾): 성공 모션이 나왔는데 데미지는 들어간다. 되돌리지 않는다
//   리슨 호스트는 서버 판정 = 자기 화면이라 이미 바로 나온다

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Library/LOACombatTypes.h"   // ELOAJustGuardMode
#include "LOAJustGuardComponent.generated.h"

/**
 * 들어온 타격 하나 (판정 매니저의 확정 / 클라 먼저 반응이 채운다). 저가 전용 칸은 없다 (2026-11-04)
 */
struct FLOAIncomingHit
{
	/** 때린 보스 (그 보스의 구간을 찾는다) */
	TObjectPtr<AActor> Attacker = nullptr;

	/** 공격 속성 (저가 공격인지는 여기서 본다) */
	FGameplayTagContainer AttackTags;

	/** 공격 기준점 (방향 검사) */
	FVector AttackOrigin = FVector::ZeroVector;

	/** 서버에서 판정한 시각 (서버: 어느 구간인지 찾을 때) */
	double HitServerTime = 0.0;

	/** 그 사람 화면에서 닿은 서버 시각 (제때 눌렀나) */
	double SeenTime = 0.0;

	/** 그 사람 위치 (방향 검사) */
	FVector MyLocation = FVector::ZeroVector;

	/** 클라가 이미 "맞음"이라 했다 → 맞은 시각만 적고 판단하지 않는다 */
	bool bClientSaidHit = false;
};

/** 저가 구간 하나에 대한 이 플레이어의 기록 */
struct FLOAJustGuardRecord
{
	/** 저가 ID (서버가 구간을 열 때 매김) */
	int32 WindowId = 0;

	/** 단타 / 홀딩 (첫 타부터) / 홀딩 (중간부터) */
	ELOAJustGuardMode Mode = ELOAJustGuardMode::None;

	/** [조종하는 사람] 이 구간을 연 보스 (구간 알림에 같이 온다. 클라가 구간을 찾을 때) (2026-11-04) */
	TWeakObjectPtr<AActor> Attacker;

	/** [조종하는 사람] 지금 누를 수 있나 (서버가 알린 상태가 열림. 누르기 마감·닫힘이면 false) */
	bool bOpen = false;

	/** [조종하는 사람] 구간이 끝났나 (서버가 "닫힘"을 알림). 누르기 마감(bOpen만 false)과 구분한다 (2026-11-05) */
	bool bClosed = false;

	/** [조종하는 사람] 구간 길이 (초). "열림" 알림에 같이 온다. 0 = 모름 (2026-11-05) */
	float Duration = 0.f;

	/** [조종하는 사람] "열림"을 받은 이 컴퓨터의 게임 시각 (UWorld::GetTimeSeconds). 음수 = 못 받음. 구간 끝 = 이 값 + Duration → 홀딩 게이지의 100% (2026-11-05) */
	double OpenLocalTime = -1.0;

	/**
	 * [조종하는 사람] 이 구간의 타격이 내 화면에서 처음 닿은 이 컴퓨터의 게임 시각 (UWorld::GetTimeSeconds). 음수 = 아직 안 맞음
	 * 홀딩 게이지의 0% (2026-11-08). 게이지는 내 화면 UI라 서버 시각(HitTimes) 대신 로컬 시계로 따로 적는다 (OpenLocalTime과 같은 시계)
	 */
	double FirstHitLocalTime = -1.0;

	/** 누른 서버 시각. 음수 = 안 누름 */
	double PressTime = -1.0;

	/** 누른 순간 보고 있던 방향 (Yaw, 도). 저가 중에는 못 도니까 이번 저가 내내의 정면 (2026-10-27) */
	float PressYaw = 0.f;

	/** 뗀 서버 시각. 음수 = 아직 누르고 있음 (안 눌렀으면 의미 없음) */
	double ReleaseTime = -1.0;

	/** 취소한 서버 시각 (피격 이상으로 저가가 끊김). 음수 = 취소 안 함. 이 뒤의 공격은 막지 못한다 */
	double CancelTime = -1.0;

	/** 이 구간에서 이 플레이어에게 닿은 타격의 본 시각들 (작은 것부터). 홀딩의 기준 타격을 여기서 고른다 (2026-11-04) */
	TArray<double> HitTimes;

	bool HasPressed() const { return PressTime >= 0.0; }
	bool IsHeldAt(double ServerTime) const { return HasPressed() && (ReleaseTime < 0.0 || ReleaseTime >= ServerTime); }
	bool WasCancelledBefore(double ServerTime) const { return CancelTime >= 0.0 && CancelTime < ServerTime; }
};

/** 저가 ID 하나에 대한 알림 (성공). GA가 듣는다 */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAJustGuardWindowDelegate, int32 /*WindowId*/);

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOAJustGuardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOAJustGuardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 조종하는 사람 컴퓨터 ----------

	/**
	 * [조종하는 사람] 서버가 알린 저가 구간 상태 + 그 구간을 연 보스 (ULOACombatRPCComponent::ClientJustGuardWindow)
	 * 열림일 때만 누를 수 있다. 열림이면 OnWindowOpened, 닫힘이면 OnWindowClosed (홀딩 중인 저가 GA가 끝난다. 2026-11-04)
	 * Duration: 구간 길이 (초). 열림일 때 받은 순간과 같이 적어 둔다 → 홀딩 게이지 (GetHoldProgress, 2026-11-05)
	 */
	void SetWindowState(int32 WindowId, ELOAJustGuardMode Mode, ELOAJustGuardWindowState State, AActor* Attacker, float Duration);

	/** [조종하는 사람] 지금 누를 수 있는 저가 ID: 열려 있고 아직 안 누른 것 중 가장 앞선 것. 없으면 0 (GA 발동 조건) */
	int32 FindPressableWindow() const;

	/**
	 * [조종하는 사람] 홀딩 게이지. 홀딩 구간에서 G를 누르고 있고 첫 타가 닿았으면 true + 진행률 (2026-11-05, 기준 변경 2026-11-08)
	 * 진행률 = (지금 − 첫 타) ÷ (구간 끝 − 첫 타)  (사용자 결정 2026-11-08). 셋 다 이 컴퓨터의 게임 시각
	 *   첫 타 = FirstHitLocalTime / 구간 끝 = OpenLocalTime + Duration
	 *   예) 구간 10.0 ~ 13.0, 첫 타 10.5 → 지금 11.75면 (11.75 − 10.5) ÷ (13.0 − 10.5) = 0.5
	 * 첫 타 전, 손을 뗐거나(취소 포함), 구간이 끝났거나(닫힘), 단타면 false → 게이지를 숨긴다
	 * 화면 위젯(ULOAJustGuardHoldBar)이 매 프레임 부른다
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|JustGuard")
	bool GetHoldProgress(float& OutProgress) const;

	// ---------- 기록 (조종하는 사람 = 원본, 서버 = 저가 매니저가 검사한 사본) ----------

	/** 눌렀다 (+ 그 순간 보고 있던 방향). 이미 누른 구간이면 무시 (한 구간에 한 번) */
	void RecordPress(int32 WindowId, ELOAJustGuardMode Mode, double PressServerTime, float PressYaw);

	/** 뗐다. 안 눌렀거나 이미 뗐으면 무시 */
	void RecordRelease(int32 WindowId, double ReleaseServerTime);

	/** 취소했다 (피격 이상으로 끊김). 안 뗐으면 뗀 것으로도 적는다. 이미 취소했으면 무시 */
	void RecordCancel(int32 WindowId, double CancelServerTime);

	/** 이 구간의 기록. 없으면 nullptr (저가 매니저가 원격 보고를 검사할 때, GA가 누를 때) */
	const FLOAJustGuardRecord* FindRecord(int32 WindowId) const;

	// ---------- 막았나 (서버 = 판정 매니저의 확정 / 클라 = 판정 매니저의 먼저 반응) ----------

	/**
	 * 들어온 타격을 막았나. 헤더 [저가는 맞은 사람의 상태] ① ~ ⑥
	 * 막았으면 알림까지 한다 (이 컴퓨터의 GA + 서버 사본이면 그 클라)
	 * @return 막았으면 true (판정 매니저가 효과를 주지 않는다 / 클라는 "막음" 보고)
	 */
	bool JudgeJustGuard(const FLOAIncomingHit& Hit);

private:
	/** ② 이 타격의 구간 기록 (서버 = 저가 매니저에 물음 / 클라 = 이 보스의 가장 최근 구간). 없으면 nullptr */
	FLOAJustGuardRecord* FindRecordForHit(const FLOAIncomingHit& Hit);

	/** ③ 맞은 시각 적기 (작은 것부터. 같은 시각은 한 번) */
	static void AddHitTime(FLOAJustGuardRecord& Record, double SeenTime);

	/** ⑤ 본 시각에 닿은 타격을 이 기록의 누름이 막았나. 규칙은 헤더 [판정 규칙]. 기록은 바꾸지 않는다 */
	bool IsBlocked(const FLOAJustGuardRecord& Record, double SeenTime, const FVector& AttackOrigin, const FVector& VictimLocation) const;

	/** 기준 타격의 본 시각 (단타 = 이 타격 / 홀딩 = 첫 타 / 홀딩 중간부터 = 누른 뒤 첫 타). 헤더 [판정 규칙] */
	static double GetReferenceTime(const FLOAJustGuardRecord& Record, double SeenTime);

	/**
	 * 공격 기준점이 VictimLocation에서 누른 순간 방향(PressYaw) 정면 ±Just Guard Facing Half Angle 안인가 (헤더 [판정 규칙] 방향)
	 * 기준점이 몸 안(캡슐 반지름 안)이면 방향이 없다 → 통과. OutAngle = 정면에서 벌어진 각도(도)
	 */
	bool IsFacing(const FLOAJustGuardRecord& Record, const FVector& AttackOrigin, const FVector& VictimLocation, float& OutAngle) const;

public:

	// ---------- 결과 ----------

	/** 이 구간을 막았다는 알림 → OnGuardSucceeded (조종하는 사람의 GA → Success 모션). 서버의 "막음" 알림 RPC도 여기로 */
	void NotifyGuardSucceeded(int32 WindowId);

	/** 막았다 (홀딩은 같은 ID로 여러 번 온다). 조종하는 사람의 GA가 듣는다 */
	FLOAJustGuardWindowDelegate OnGuardSucceeded;

	/** [조종하는 사람] 구간이 끝났다 (서버가 "닫힘"을 알림). 조종하는 사람의 GA가 듣는다 (2026-11-04) */
	FLOAJustGuardWindowDelegate OnWindowClosed;

	/** [조종하는 사람] 구간이 열렸다 (서버가 "열림"을 알림). 지금은 플레이어 테스터의 자동 저가가 듣는다 (2026-11-04) */
	FLOAJustGuardWindowDelegate OnWindowOpened;

private:
	/** [서버 사본] 원격 플레이어 클라에 "막음" 알림 (ClientJustGuardResult). 리슨 호스트 본인·클라에서는 아무것도 안 함 */
	void NotifyClientGuardSucceeded(int32 WindowId) const;

	/** [조종하는 사람] 이 보스가 연 구간 중 가장 최근 것 (열림·닫힘 상관없이). 없으면 nullptr */
	FLOAJustGuardRecord* FindLatestRecordOf(const AActor* Attacker);

	FLOAJustGuardRecord* FindRecordMutable(int32 WindowId);
	FLOAJustGuardRecord& FindOrAddRecord(int32 WindowId, ELOAJustGuardMode Mode);

	/** 기록. ID가 작은(오래된) 것이 앞. 최근 MaxRecords개만 남긴다 (피해자 판정 마감 1초 안에 구간이 그만큼 열릴 일은 없다) */
	TArray<FLOAJustGuardRecord> Records;
};
