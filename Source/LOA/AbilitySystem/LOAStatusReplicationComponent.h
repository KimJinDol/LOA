// LOAStatusReplicationComponent.h
// [배관] 상태 아이콘 남은 시간을 모든 컴퓨터에 보낸다. 보스처럼 ASC가 Minimal이라 GE가 클라에 없는 액터에 붙인다. (2026-11-10)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가]
//   Minimal ASC는 태그만 보내고 GE(남은 시간이 든 곳)는 아무에게도 안 보낸다 → 클라는 "기절" 아이콘은 그려도 남은 초를 모른다
//   ASC를 Full로 바꾸면 보스의 모든 GE(스펙, 보정치, 컨텍스트)가 전부 나간다 → 상태 표시에 필요한 3개만 따로 보낸다
//     { 상태 태그, 끝나는 서버 시각, 전체 시간 }
//   엔진이 Full에서 하는 방식과 같다 (GE의 시작 서버 시각을 보내고 클라가 지금 서버 시각과 비교). 필드만 적다
//
// [보내는 방식] 프로퍼티 복제 (RPC 아님). 늦게 들어온 플레이어도 지금 값을 받는다
//   알림(OnRep)은 없다: 아이콘이 매 프레임 남은 시간을 물어본다 (ULOAStatusIcon). 태그 목록은 ASC 태그 복제로 따로 온다
//
// [흐름] [서버]
//   InitializeWithAbilitySystem(ASC) : GE 적용 / 제거 알림 구독
//   상태 태그(LOAStatusTags)를 주는 GE가 적용(새로 붙음, 다시 받아 시간 갱신) 또는 제거됨 → 그 자리에서 목록을 처음부터 다시 만든다
//     왜 처음부터: 한 상태를 GE 여럿이 줄 수 있다. 하나가 빠지면 남은 GE 중 가장 늦게 끝나는 것으로 바뀌어야 한다
//       예) 낙인 A(5초 남음) + 낙인 B(8초) → 8초. B 제거 → 5초
//     제거 알림 순간 그 GE는 아직 목록에 있다 (엔진: IsPendingRemove 표시 후 알림) → IsPendingRemove는 건너뛴다
//   무한 GE는 뺀다 (남은 시간 없음 → 아이콘에 숫자 안 보임)
//
// [읽기] [모든 컴퓨터] 주인의 위젯 컨트롤러가 ULOAStatusTracker::Init에 이 부품을 넘긴다 → 시간은 GE 대신 여기서
//   남은 시간 = 끝나는 서버 시각 - 지금 서버 시각 (LOANetTime)
//
// [알려진 한계]
//   GE 시간을 코드로 바꾸는 엔진 함수(ModifyActiveEffectStartTime 등)는 적용·제거 알림이 없어 반영되지 않는다
//   지금 프로젝트는 그 함수를 쓰지 않는다. 쓰게 되면 그 GE의 OnGameplayEffectTimeChangeDelegate도 구독한다

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "LOAStatusReplicationComponent.generated.h"

class UAbilitySystemComponent;
struct FActiveGameplayEffect;
struct FActiveGameplayEffectHandle;
struct FGameplayEffectSpec;

/** 상태 하나의 시간. 클라는 EndServerTime - 지금 서버 시각으로 남은 시간을 안다 */
USTRUCT()
struct FLOAReplicatedStatusTime
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag StatusTag;

	/** 끝나는 서버 시각 (초, LOANetTime 기준) */
	UPROPERTY()
	double EndServerTime = 0.0;

	/** 전체 시간 (초) */
	UPROPERTY()
	float Duration = 0.f;
};

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOAStatusReplicationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOAStatusReplicationComponent();

	/** [모든 컴퓨터] 주인이 BeginPlay에서 부른다. 서버만 GE 알림을 구독한다 (클라는 받기만) */
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	/** 그 상태의 남은 / 전체 시간. 목록에 없거나 이미 끝났으면 false */
	bool GetStatusTimeRemaining(FGameplayTag StatusTag, float& OutTimeRemaining, float& OutDuration) const;

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent Interface

private:
	/** [서버] GE 적용 (새로 붙음 + 다시 받아 시간 갱신). 즉발 GE(데미지)도 오므로 상태 태그를 주는 것만 */
	void HandleEffectApplied(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);

	/** [서버] GE 제거 */
	void HandleEffectRemoved(const FActiveGameplayEffect& Effect);

	/** 이 GE가 상태 태그를 주는가 */
	static bool GrantsStatusTag(const FGameplayEffectSpec& Spec);

	/** [서버] 지금 붙은 GE로 목록을 처음부터 다시 만든다 */
	void RebuildStatusTimes();

	/** 복제되는 목록. 상태 태그마다 하나 */
	UPROPERTY(Replicated)
	TArray<FLOAReplicatedStatusTime> StatusTimes;

	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	FDelegateHandle EffectAppliedHandle;
	FDelegateHandle EffectRemovedHandle;
};
