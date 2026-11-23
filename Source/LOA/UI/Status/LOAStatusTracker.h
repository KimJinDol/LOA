// LOAStatusTracker.h
// [배관] 상태(버프·디버프·상태 이상) 지켜보기. ASC 하나의 상태 태그를 보다가 아이콘 줄에 알림을 쏜다. (2026-11-10)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할] 주인이 누구인지 모른다. ASC 하나만 안다 → 플레이어(파티원 컨트롤러)와 보스(보스 위젯 컨트롤러)가 같이 쓴다
//   파티원 컨트롤러 : 이름·직업·체력·사망 (플레이어 일) + 이 Tracker 하나
//   이 Tracker      : 상태 태그 목록과 남은 시간 (상태 일)
//
// [보는 태그] LOAStatusTags의 부모 셋 (Buff / Debuff / CrowdControl.Status). 자식이 붙고 떨어질 때마다 부모 개수도 바뀐다
//   (엔진: 태그 개수는 부모까지 센다). 부모 태그 자체는 이름표가 아니라서 목록에서 뺀다
//
// [알림]
//   OnStatusTagsChanged(태그들) : 셋 중 하나의 개수가 바뀜 → 지금 붙은 자식 태그 전부
//   OnStatusTagAdded(태그)      : 그중 새로 붙은 태그 하나씩 (2026-11-16 apply116, 전투 글자 "버프 이름"용)
//     "새로" = 지난번 목록에 없던 태그. 같은 버프가 다시 걸려 시간만 늘거나 GE가 하나 더 붙어도(태그는 그대로) 알리지 않는다 (사용자: 처음 붙을 때만)
//     Init 때 이미 붙어 있던 태그도 알리지 않는다 (지금 목록이 처음 "지난번")
//     예) [공격력 증가] → [공격력 증가, 주는 피해 증가] : "주는 피해 증가" 하나 / 공격력 증가가 다시 걸림 : 없음
// [위젯이 부를 함수]
//   GetStatusTimeRemaining(태그) : 그 태그를 주는 GE 중 가장 오래 남은 것 (아래 [네트워크])
//
// [네트워크]
//   태그 : GE가 주는 태그는 모든 컴퓨터에 복제 (Mixed·Minimal 둘 다)
//   시간 : Init에서 무엇을 받았느냐로 정해진다 (2026-11-10)
//          시간 복제 부품을 받음 → 거기서. 모든 컴퓨터가 안다 (보스: 보스 위젯 컨트롤러가 넘김)
//          안 받음           → GE에서. GE가 이 컴퓨터에 있어야 안다 (플레이어 Mixed → 자기 것만)

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "LOAStatusTracker.generated.h"

class UAbilitySystemComponent;
class ULOAStatusReplicationComponent;

/** 지금 붙어 있는 상태 태그 전부 (부모 태그 자체는 빠짐) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOAStatusTagsChangedSignature, const FGameplayTagContainer&, StatusTags);

/** 상태 태그 하나가 새로 붙었다 (헤더 [알림]). C++만 듣는다 (2026-11-16) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAStatusTagAddedNative, const FGameplayTag& /*StatusTag*/);

UCLASS(BlueprintType)
class LOA_API ULOAStatusTracker : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 이 ASC의 상태 태그를 보기 시작한다. 다시 부르면 이전 것은 푼다
	 * @param InReplicatedTimes  남은 시간을 읽을 곳. nullptr이면 이 컴퓨터의 GE에서 읽는다
	 */
	void Init(UAbilitySystemComponent* InASC, const ULOAStatusReplicationComponent* InReplicatedTimes = nullptr);

	/** 걸어 둔 알림 해제. 두 번 불려도 안전 */
	void Shutdown();

	/** 지금 붙어 있는 상태 태그 (OnStatusTagsChanged와 같은 내용) */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Status")
	FGameplayTagContainer GetStatusTags() const;

	/**
	 * 그 상태 태그를 주는 GE 중 가장 오래 남은 것의 남은 / 전체 시간.
	 * 이 컴퓨터에 GE가 없거나(남의 것) 시간이 없는(무한) 효과면 false
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Status")
	bool GetStatusTimeRemaining(FGameplayTag StatusTag, float& OutTimeRemaining, float& OutDuration) const;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Status")
	FLOAStatusTagsChangedSignature OnStatusTagsChanged;

	/** 상태 태그 하나가 새로 붙었다 (헤더 [알림], 2026-11-16) */
	FLOAStatusTagAddedNative OnStatusTagAdded;

private:
	void HandleStatusTagChanged(const FGameplayTag Tag, int32 NewCount);

	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	/** 남은 시간을 읽을 곳 (Init에서 받음). 없으면 GE에서 */
	TWeakObjectPtr<const ULOAStatusReplicationComponent> ReplicatedTimes;

	/** 부모 태그별 알림 번호표 */
	TMap<FGameplayTag, FDelegateHandle> TagChangedHandles;

	/** 지난번 알림 때의 상태 태그. 새로 붙은 것을 가려낸다 (헤더 [알림], 2026-11-16) */
	FGameplayTagContainer LastStatusTags;
};
