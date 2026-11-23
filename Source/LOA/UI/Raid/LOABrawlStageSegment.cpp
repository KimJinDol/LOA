// LOABrawlStageSegment.cpp

#include "UI/Raid/LOABrawlStageSegment.h"

#include "Components/ProgressBar.h"
#include "Components/Widget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOABrawlStageSegment)

void ULOABrawlStageSegment::SetState(float Percent, bool bLocked)
{
	const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);
	if (SegmentBar && !FMath::IsNearlyEqual(ClampedPercent, AppliedPercent))
	{
		AppliedPercent = ClampedPercent;
		SegmentBar->SetPercent(ClampedPercent);
	}

	const int8 LockedValue = bLocked ? 1 : 0;
	if (LockIcon && LockedValue != AppliedLocked)
	{
		AppliedLocked = LockedValue;
		LockIcon->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
