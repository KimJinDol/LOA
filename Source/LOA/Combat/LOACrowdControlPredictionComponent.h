// LOACrowdControlPredictionComponent.h
// [배관] 예측 피격 반응. 플레이어 캐릭터에 붙는다. (2026-10-31, CC 2단계의 2-1: 이동 멈춤 + 몽타주)
// 피해자 판정에서 "맞음"이면 서버 CC가 오기 전에 내 화면에서 먼저 멈추고 CC 몽타주를 튼다. 진짜 CC GA가 오면 이어받는다.
// 부르는 곳: 판정 매니저 ULOACombatJudgeManagerSubsystem::ReactLocal (2026-11-04. 저가로 못 막았을 때만)
//   ← 피해자 판정 요청이 왔을 때 내 화면에서 "맞음" (ULOAVictimJudgeSubsystem::JudgeLocal)
//   ← 예고 장판이 내 화면에서 다 찼을 때 모양 안 (ULOAServerJudgeSubsystem::JudgeTelegraphLocal) (예전 기록: 둘 다 ULOAHitJudgeSubsystem의 JudgeLocal / JudgeTelegraphLocal 안. 2026-11-01)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 있나]
//   피해자 판정은 클라가 "맞음"을 서버보다 왕복만큼 먼저 안다. 그런데 경직·띄움은 서버가 보고를 받고 GE를 줘야 시작해서
//   내 화면에서 맞은 뒤 왕복(핑)만큼 늦게 반응했다 (편도 50만 돼도 느껴짐).
//
// [왜 CC GA를 예측(LocalPredicted)으로 바꾸지 않나]
//   CC의 숫자(거리·시간·방향)는 서버 GE의 Spec에 있다. GA를 클라가 발동하면 서버에서 발동 요청(ASC 채널)과
//   판정 보고(ULOACombatRPCComponent 채널)의 순서가 보장되지 않아, GE 없이 GA가 시작할 수 있다.
//   → 발동은 그대로 서버(ServerInitiated). 예측은 "보이는 것"(멈춤·몽타주)만 한다.
//
// [왜 ASC가 아니라 캐릭터 컴포넌트인가] (사용자 결정)
//   ULOAAbilitySystemComponent는 "입력 태그로 스킬 발동"만 한다. 멈춤·몽타주는 캐릭터 몸의 일이다.
//   캐릭터가 다시 태어나면 새로 생겨 예측 상태가 남지 않는다. (저스트 가드 장부 ULOAJustGuardComponent와 같은 자리)
//
// [흐름] (편도 0.05)
//   0.00 [서버] 판정 → 요청 (이 공격의 CC GE도 같이. ULOAVictimJudgeSubsystem::SendVictimRequests)
//   0.05 [B]    내 판정 "맞음" → 보고 + PredictCrowdControl
//                 이동·스킬 금지 (로컬 태그, 복제 안 됨) + 판정 기준점 바라보기 + 그 CC GA의 몽타주를 먼저 튼다
//                 바라보기 (2026-11-01): 진짜 CC는 "밀리는 방향의 반대"를 본다 (StartCrowdControlMove)
//                   Away From Source(장판·내려찍기) = 기준점 쪽 → 예측과 같다
//                   Source Forward(장풍) = 장풍 방향의 반대 → 진짜 CC가 올 때 다시 돈다 (방향 규칙까지 맞추려면 GE 쪽을 고쳐야 해서 보류)
//   0.10 [서버] 보고 도착 → GE → 경직·띄움 GA
//   0.15 [B]    진짜 GA 발동 → ConfirmPrediction: 같은 몽타주가 재생 중이면 그 위치부터 이어서 (처음으로 안 돌아감)
//                 스킬 금지 로컬 태그는 여기서 푼다 (이제 진짜 CC 태그가 막는다. 띄움 중 기상기는 되어야 하므로)
//          진짜 GA 끝 → EndPrediction: 이동 금지 로컬 태그를 푼다
//                 (확정 때 풀지 않는 이유: GA 발동 RPC와 이동 금지 GE 복제는 따로 와서, 그 사이 한 프레임 걸을 수 있다)
//
// [어느 GA인가] GE가 붙이는 태그(Grant Tags)로 발동하는 GA (GA의 AbilityTriggers, OwnedTagAdded). CC 종류를 하드코딩하지 않는다
//   예) 경직 GE → State.CrowdControl.Paralysis → GA_Paralysis / 띄움 GE → State.CrowdControl.Push.Airborne → GA_Airborne
//
// [예측하지 않는 경우]
//   GE의 적용 조건(Target Tag Requirements)에 내 태그가 안 맞음 (슈퍼아머·회피 무적·누움 등) → 서버도 안 건다
//   이미 그 CC 중 (진짜 GA가 "또 맞음"을 처리) / 이미 예측 중 / 이 GE로 발동하는 GA가 없음
//
// [틀렸을 때] (드묾: 클라 "맞음"은 서버가 그대로 믿는다. 면역 태그가 서버와 다를 때 정도)
//   Timeout(왕복 + 여유) 안에 진짜 GA가 안 오면 몽타주를 멈추고 로컬 태그를 푼다
//
// [네트워크] 1 서버만: 없음 / 2 복제: 없음 (로컬 태그·몽타주 모두 이 컴퓨터만) / 3 각자: 맞은 사람 컴퓨터만
//   궤적(밀림·띄움 이동)은 아직 예측하지 않는다 (2-2). 진짜 CC가 오면 1단계(내 자리 → 서버 도착 지점)로 그린다

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "LOACrowdControlPredictionComponent.generated.h"

struct FGameplayTagContainer;
class UAnimMontage;
class UGameplayEffect;
class UAbilitySystemComponent;
class ULOAGameplayAbility;

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOACrowdControlPredictionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOACrowdControlPredictionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * [맞은 사람 컴퓨터] 피해자 판정 "맞음" → 서버 CC가 오기 전에 먼저 반응: 이동·스킬 금지(로컬 태그) + 때린 쪽 바라보기 + 그 CC GA의 몽타주
	 * @param CrowdControlEffect  이 공격의 CC GE. 이 GE가 붙이는 태그로 발동하는 GA가 없으면 아무것도 안 함
	 * @param SourcePoint         판정 기준점 (모양 기준점: 장판 중심, 부채꼴 꼭짓점). 이쪽을 바라본다
	 * @param Timeout             이 시간(초) 안에 진짜 GA가 안 오면 되돌린다
	 * @return 예측을 시작했으면 true
	 */
	bool PredictCrowdControl(TSubclassOf<UGameplayEffect> CrowdControlEffect, const FVector& SourcePoint, float Timeout);

	/**
	 * [맞은 사람 컴퓨터] 진짜 CC GA가 발동됨 → 예측을 이어받는다 (스킬 금지 로컬 태그를 풀고 되돌리기 취소)
	 * @return 예측 몽타주가 이 몽타주로 재생 중이면 그 위치(초) → GA가 거기서부터 튼다. 아니면 −1
	 */
	float ConfirmPrediction(const UAnimMontage* Montage);

	/** [맞은 사람 컴퓨터] 진짜 CC GA가 끝남 → 예측으로 붙인 이동 금지 로컬 태그를 푼다. 예측이 없었으면 아무것도 안 함 */
	void EndPrediction();

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 주인 캐릭터의 ASC (PlayerState에 있음). 없으면 nullptr */
	UAbilitySystemComponent* GetOwnerAbilitySystem() const;

	/** 부여된 Ability 중 이 태그가 붙으면 발동하는 것 (AbilityTriggers의 OwnedTagAdded). 없으면 nullptr */
	static const ULOAGameplayAbility* FindAbilityTriggeredByTags(const UAbilitySystemComponent& ASC, const FGameplayTagContainer& Tags);

	/** Timeout 안에 진짜 GA가 안 옴 → 몽타주를 멈추고 로컬 태그를 푼다 */
	void OnPredictionTimeout();

	/** 예측 중 (진짜 GA가 끝날 때까지) */
	bool bPredicted = false;

	/** 진짜 GA가 이어받음 (스킬 금지 로컬 태그는 이미 풂) */
	bool bConfirmed = false;

	/** 먼저 튼 몽타주 (이어받기·되돌리기용) */
	TWeakObjectPtr<UAnimMontage> PredictedMontage;

	/** 되돌리기 타이머 */
	FTimerHandle PredictionTimer;
};
