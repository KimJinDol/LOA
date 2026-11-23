// LOASkillModule_Move.h
// [배관] Move 모듈. "[언제] 신호가 오면, 보는 방향으로 [거리]만큼 [시간] 동안 옮긴다 (높이를 주면 포물선 점프)."
// 이동만 한다. 맞히는 일은 하지 않는다. (맞히기는 같은 GA의 Apply / Spawn 모듈과 조합)
//
// ─────────────────────────────────────────────────────────────────────────────
// [디테일 패널]
//
//   ▼ [3]  [ Move ▼ ]
//        Trigger            [ On Event ▼ ]  Event.Montage.JumpStart   ← 언제 움직이기 시작하나 (부모 칸)
//        Destination        [ Forward ▼ ]  Forward = 보는 방향으로 Distance / Location = 위치 표시까지
//        Distance           800      보는 방향으로 몇 cm (Forward일 때. 회전하지 않는다. 지금 보는 방향 그대로)
//        Location Tag       [ Location.Arena.Center ]   ← Location일 때. 레벨의 ALOALocationMarker
//        Height             300      0 = 땅 위 이동(돌진, 대시) / 0보다 크면 포물선 점프의 최고 높이
//        Arrive Event       [ Event.Montage.Hit.1 ]   ← (선택) 이 노티파이 순간에 도착하도록 시간을 자동 계산
//        Duration           0.5      Arrive Event를 비웠거나 몽타주에 그 노티파이가 없을 때: 몇 초 만에 도착
//        Pass Through Body Blockers ☐  이동하는 동안 보스·몬스터의 몸 막이를 뚫고 지나간다. 끝나면 원래대로 (관통 스킬)
//
//   보스 점프 착지 : On Event JumpStart, Distance 800, Height 300, Arrive Event = Hit.1 (착지 판정과 같은 순간 도착)
//   보스 돌진      : Distance 1200, Height 0, Arrive Event = 돌진 끝 노티파이
//   패턴 D (사라졌다 맵 중앙에 쿵) : Destination Location, Location Tag = Location.Arena.Center, Height 0,
//                                  Arrive Event = 내려찍기 예고 노티파이. (보스는 State.Body.Hidden 구간이라 이동이 안 보인다)
//   플레이어 관통 대시(베스티지) : Distance 600, Height 0, Duration 0.25, Pass Through Body Blockers ☑
//                                  + 같은 GA에 Apply(Box 600 × 200)로 지나갈 길 판정
//
//   충돌 규칙 (LOACollisionChannels.h): 보스는 원래 플레이어를 통과하고, 플레이어끼리도 통과한다.
//   플레이어를 막는 건 보스·몬스터의 몸 막이(ULOABodyBlockerComponent)뿐 → 관통 스킬만 이 칸을 켠다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//   SkillModuleTriggered : 걸린 시간 정하기 (Arrive Event까지 남은 초, 없으면 Duration)
//                          → Pass Through Body Blockers면 내 ASC에 State.Body.PassThroughBodyBlock (로컬 태그)
//                            (플레이어 이동 컴포넌트가 이 태그를 보고 그동안 몸 막이를 무시. 이 모듈은 그 컴포넌트를 모른다)
//                          → Height 0: ApplyRootMotionMoveToForce(보는 방향 Distance 지점, 시간)
//                            Height > 0: ApplyRootMotionJumpForce(보는 방향, Distance, Height, 시간)
//   이동 끝 (Task 알림) / Ability 끝·취소 (SkillModuleEnd) : 태그 떼기
//     관통 대시가 보스 몸 안에서 끝나도 이동 컴포넌트가 슥 밀어낸다.
//
//   GAS의 루트 모션 Task를 쓴다. 이름은 Root Motion이지만 애니가 아니라 "코드가 정한 이동"이다.
//   ※ 몽타주 애니에 루트 모션이 들어 있으면 애니 시퀀스의 Enable Root Motion을 끈다. (안 끄면 두 번 움직인다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만  : 없음. 이동은 판정이 아니라 Apply·Spawn처럼 서버로 막지 않는다.
//   2. 복제    : 캐릭터 이동 컴포넌트가 루트 모션 이동을 복제한다. (다른 컴퓨터의 보스·플레이어 모습)
//   3. 각자    : GA가 도는 곳에서 각자 이동한다.
//                보스 GA(서버에서만 돔) → 서버가 움직이고 클라는 복제로 봄
//                플레이어 GA(내 컴퓨터 예측 + 서버) → 둘 다 같은 이동을 해서 내 화면은 바로 움직이고 서버와 맞춰진다
//                (GAS 루트 모션 Task는 이 예측을 지원하도록 만들어진 기능)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "LOASkillModule_Move.generated.h"

/** 어디로 가나 */
UENUM(BlueprintType)
enum class ELOAMoveDestination : uint8
{
	/** 보는 방향으로 Distance만큼 */
	Forward		UMETA(DisplayName = "Forward"),

	/** 레벨의 위치 표시(ALOALocationMarker)까지. 거리는 자동. 못 찾으면 Forward + 경고 */
	Location	UMETA(DisplayName = "Location"),
};

UCLASS(meta = (DisplayName = "Move"))
class LOA_API ULOASkillModule_Move : public ULOASkillModule
{
	GENERATED_BODY()

protected:
	//~ Begin ULOASkillModule Interface
	virtual void SkillModuleInit() override;
	virtual void SkillModuleTriggered(const FGameplayEventData* EventData) override;
	virtual void SkillModuleEnd(bool bWasCancelled) override;
	//~ End ULOASkillModule Interface

public:
#if WITH_EDITOR
	/**
	 * [에디터 전용] 저장 시 검사. (부모의 Trigger 검사 + 아래)
	 *   경고 : Arrive Event를 골랐는데 몽타주에 그 노티파이가 없음 (Duration으로 대신함)
	 */
	virtual EDataValidationResult ValidateSkillModule(
		class FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const override;
#endif

protected:
	// ---------- 데이터 (Ability BP의 디테일 패널에서) ----------

	/** 어디로. Forward = 보는 방향으로 Distance / Location = 위치 표시까지 (거리는 자동) */
	UPROPERTY(EditDefaultsOnly, Category = "Move")
	ELOAMoveDestination Destination = ELOAMoveDestination::Forward;

	/** 보는 방향으로 몇 cm. 회전하지 않는다. (예: 보스 점프 800) */
	UPROPERTY(EditDefaultsOnly, Category = "Move",
		meta = (ClampMin = "0", Units = "cm", EditCondition = "Destination == ELOAMoveDestination::Forward", EditConditionHides))
	float Distance = 600.f;

	/** 갈 곳의 이름. 레벨에 같은 태그의 ALOALocationMarker를 놓는다. (예: Location.Arena.Center) */
	UPROPERTY(EditDefaultsOnly, Category = "Move",
		meta = (Categories = "Location", EditCondition = "Destination == ELOAMoveDestination::Location", EditConditionHides))
	FGameplayTag LocationTag;

	/** 0 = 땅 위 이동 (돌진, 대시) / 0보다 크면 포물선 점프의 최고 높이 (예: 300) */
	UPROPERTY(EditDefaultsOnly, Category = "Move", meta = (ClampMin = "0", Units = "cm"))
	float Height = 0.f;

	/**
	 * (선택) 이 Ability 몽타주의 이 노티파이 순간에 도착하도록 걸린 시간을 자동으로 정한다.
	 * 보스 점프: 착지 판정 노티파이(Event.Montage.Hit.1)를 고르면 착지와 판정이 항상 같은 순간.
	 * 비우거나 몽타주에 없으면 Duration.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Move", meta = (Categories = "Event.Montage"))
	FGameplayTag ArriveEvent;

	/** 몇 초 만에 도착하나. Arrive Event를 비웠거나 찾지 못했을 때만 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Move", meta = (ClampMin = "0.05", Units = "Seconds"))
	float Duration = 0.5f;

	/**
	 * 이동하는 동안 보스·몬스터의 몸 막이(ULOABodyBlockerComponent)를 뚫고 지나간다. 끝나면 원래대로.
	 * 관통 스킬(예: 베스티지)만 켠다. 보스 패턴은 필요 없다. (보스 캡슐은 원래 플레이어를 통과)
	 * 이동하는 동안 State.Body.PassThroughBodyBlock 태그를 붙인다. 태그를 보는 건 플레이어 이동 컴포넌트뿐이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Move")
	bool bPassThroughBodyBlockers = false;

private:
	/** 루트 모션 Task가 끝났다 (도착). 관통 끝 */
	UFUNCTION()
	void OnMoveFinished();

	/** Pass Through Body Blockers: State.Body.PassThroughBodyBlock 붙이기 / 떼기 */
	void SetPassThrough(bool bEnable);

	/** 이번 발동에서 태그를 붙였나 (한 번만 떼려고. 같은 태그를 다른 곳에서도 붙였으면 그 개수는 남는다) */
	bool bPassThroughActive = false;
};
