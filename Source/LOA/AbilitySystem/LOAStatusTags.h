// LOAStatusTags.h
// "상태(아이콘에 보이는 버프·디버프·상태 이상) 태그"의 기준 한 곳. (2026-11-10)
//   쓰는 곳: ULOAStatusTracker (아이콘 줄), ULOAStatusReplicationComponent (보스 남은 시간 복제)
//
// [상태 태그] 아래 부모 셋의 "자식". 부모 태그 자체는 이름표가 아니라서 빠진다
//   State.Buff.*                 예) State.Buff.AttackPower
//   State.Debuff.*               예) State.Debuff.Mark
//   State.CrowdControl.Status.*  예) State.CrowdControl.Status.Stunned

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace LOAStatusTags
{
	/** 부모 태그 셋 */
	LOA_API const TArray<FGameplayTag>& GetParentTags();

	/** 부모 셋 중 하나의 자식인가 (부모 자체는 false) */
	LOA_API bool IsStatusTag(const FGameplayTag& Tag);
}
