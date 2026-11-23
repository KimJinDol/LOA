// LOAGA_JustGuard.h
// [배관] 저스트 가드 (G). 몸통만 맡는다: 가드 모션, 이동·스킬 잠금, Success / Fail 모션. 모든 직업 공용. (2026-10-26)
// "막았나"는 정하지 않는다. 누른 시각을 장부(ULOAJustGuardComponent)에 적을 뿐이다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙]
//   보스가 저가 구간을 열었을 때(노란 빛)만, 한 구간에 한 번 쓸 수 있다
//   쿨타임 없음 / 쓰는 동안 이동·스킬·회피 불가 (Success·Fail 모션 포함, 끝날 때까지)
//   누를 때 커서 쪽으로 돈다 → 공격 기준점이 정면 ±45° 안이어야 막는다 (장부 [판정 규칙] 방향, 2026-10-27)
//   직업마다 BP 자식 하나 (몽타주만 다름). 예) GA_JustGuard_Berserker
//
// [흐름] (조종하는 사람 컴퓨터)
//   ① G → 열린 구간 #7에 누름 기록 → 원격이면 서버에도 보고
//   ② 몽타주 Guard 구간 (반복 = 누르고 있는 자세)
//   ③ 성공 알림 → 이미 뗐으면 Success / 누르고 있으면(홀딩) Guard 유지, 뗄 때 Success
//   ③-1 [홀딩] 누르고 있는 중에 구간이 끝남(서버 "닫힘") → 막은 적 있으면 Success, 없으면 Fail (2026-11-04)
//        단타는 ④가 이미 끝내므로 보지 않는다
//   ④ 누른 뒤 0.5초(막기 유효 시간) + 0.3초(Result Wait Time) 안에 성공이 없으면 → Fail
//   ⑤ Success / Fail 구간이 끝나면 종료. 실패 모션 동안은 못 움직인다 (Unlock 노티파이 없음)
//   피격 이상(띄움·경직 등)으로 끊기면 → "취소" 기록 → 그 뒤 공격은 못 막는다
//
//   Result Wait Time(0.3초)이 있는 이유: 서버 판정 공격은 성공 알림이 편도만큼 늦게 온다.
//     예) 9.50에 누름, 10.00 공격 → 알림 10.05 도착. 10.00에 바로 Fail을 틀면 성공인데 실패 모션이 먼저 나온다
//
// [언제 쓸 수 있나 - CanActivateAbility]
//   내 장부에 "열려 있고 아직 안 누른 구간"이 있을 때만. 조종하는 사람 컴퓨터만 검사한다.
//   서버가 검사하면: 구간 끝 무렵에 누른 G가 서버에 도착할 땐 이미 닫혀 있어 거부된다 (화면은 노란데 G가 씹힘)
//   서버는 대신 누름 보고를 저가 담당이 검사한다 (ULOAJustGuardSubsystem::ReceivePress)
//
// [네트워크]
//   Local Predicted. Success / Fail 구간은 조종하는 사람 GA만 정한다.
//   엔진이 그 점프를 서버 몽타주에 전달하고, 서버가 다른 사람 화면에 복제한다.
//   서버 GA는 몽타주만 튼다. (서버 GA까지 정하면 클라 Fail / 서버 Success처럼 두 번 점프할 수 있다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_JustGuard_<직업> (부모: LOAGA_JustGuard)
//   Montage     : 구간 Guard / Success / Fail. Montage Sections에서 Guard → Guard 반복, Success·Fail → 끝
//   Asset Tags  : Ability.Common.JustGuard
//   회피 막기는 생성자 기본값 (Block Abilities with Tag = Ability.Common.Dodge). BP에서 지우지 말 것
//   직업 꾸러미 : { GA_JustGuard_<직업>, 1, InputTag.JustGuard } + 입력 설정 G → InputTag.JustGuard

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "GameplayPrediction.h"
#include "Library/LOACombatTypes.h"   // ELOAJustGuardMode
#include "LOAGA_JustGuard.generated.h"

class ULOAJustGuardComponent;

UCLASS(Abstract)
class LOA_API ULOAGA_JustGuard : public ULOAPlayerAbility
{
	GENERATED_BODY()

public:
	ULOAGA_JustGuard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UGameplayAbility Interface
	/** 부모 조건 + [조종하는 사람] 열려 있고 아직 안 누른 저가 구간이 있나. 헤더 [언제 쓸 수 있나] */
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	//~ End UGameplayAbility Interface

#if WITH_EDITOR
	/** [에디터 전용] 몽타주 없음 / 구간 이름이 몽타주에 없음 → 오류 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	//~ Begin ULOAPlayerAbility Interface
	/** Guard 구간 재생. 조종하는 사람이면 누름 기록 + 뗌·성공·실패 시각 대기 */
	virtual void StartSkill() override;

	/** 아직 안 뗐으면 "뗌" 기록 (피격 이상으로 취소 등), 구독 해제 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End ULOAPlayerAbility Interface

	// ---------- 데이터 (BP Class Defaults) ----------

	/** 누르고 있는 자세 구간. 몽타주에서 자기 자신으로 반복되게 둔다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|JustGuard")
	FName GuardSection = TEXT("Guard");

	/** 막았을 때 구간 (끝나면 종료) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|JustGuard")
	FName SuccessSection = TEXT("Success");

	/** 못 막았을 때 구간 (끝나면 종료. 그동안 못 움직임) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|JustGuard")
	FName FailSection = TEXT("Fail");

	/**
	 * 막기 유효 시간이 끝난 뒤 서버의 성공 알림을 더 기다리는 시간 (초). 그 뒤에도 성공이 없으면 실패 모션.
	 * 서버 판정 알림이 오는 편도 지연을 덮는다. 길수록 실패 모션이 늦게 나온다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|JustGuard", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ResultWaitTime = 0.3f;

	/** 막은 뒤 키를 계속 누르고 있어도 이 시간이 지나면 Success로 끝낸다 (초). 홀딩 패턴보다 길게 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|JustGuard", meta = (ClampMin = "0.5", Units = "Seconds"))
	float MaxHoldTime = 8.f;

private:
	/** G를 뗐다 (엔진 입력 이벤트). GA가 정상으로 끝날 때 안 뗐으면 여기로도 온다 */
	void OnInputReleased();

	/** 장부 원본 + (원격이면) 서버에 뗌 / 취소 기록 */
	void ReportRelease(bool bCancelled);

	/** 장부: 이 저가 구간을 막았다 */
	void OnGuardSucceeded(int32 InWindowId);

	/** 장부: 저가 구간이 끝났다 → [홀딩] 아직 누르고 있으면 끝낸다 (막은 적 있으면 Success, 없으면 Fail) */
	void OnWindowClosed(int32 InWindowId);

	/** 막기 유효 시간 + Result Wait Time이 지남 → 성공이 없으면 Fail */
	UFUNCTION()
	void OnDecisionTimeReached();

	/** 막은 뒤 Max Hold Time이 지남 → Success */
	UFUNCTION()
	void OnMaxHoldTimeReached();

	/** 구간으로 넘어간다 (한 번만: Success나 Fail로 넘어간 뒤에는 무시) */
	void FinishWithSection(FName Section);

	/** 내 캐릭터의 장부 */
	ULOAJustGuardComponent* GetJustGuardComponent() const;

	/** 이번에 누른 저가 ID (0 = 서버 GA라 기록하지 않음) */
	int32 WindowId = 0;

	/** 이번에 누른 구간의 방식 (단타 / 홀딩. 구간이 끝났을 때 홀딩만 끝낸다) */
	ELOAJustGuardMode WindowMode = ELOAJustGuardMode::None;

	/** 막았나 / 뗐나 / Success·Fail로 넘어갔나 (발동마다 초기화) */
	bool bSucceeded = false;
	bool bReleased = false;
	bool bFinishing = false;

	/** 구독 번호표 */
	FDelegateHandle GuardSucceededHandle;
	FDelegateHandle WindowClosedHandle;
	FDelegateHandle InputReleasedHandle;

	/** 뗌 입력 구독에 쓴 예측키 (해제할 때 같은 칸) */
	FPredictionKey InputReleasedPredictionKey;

	/** 구독한 장부 */
	TWeakObjectPtr<ULOAJustGuardComponent> ListenedJustGuardComp;
};
