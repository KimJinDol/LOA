// LOARaidResultTypes.cpp

#include "Game/LOARaidResultTypes.h"
#include "Data/LOACharacterClassData.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARaidResultTypes)

TArray<ELOARaidStat> FLOARaidPlayerResult::GetTopStats(int32 MaxCount) const
{
	TArray<ELOARaidStat> Stats;
	for (int32 Index = 0; Index < static_cast<int32>(ELOARaidStat::Count); ++Index)
	{
		if (GetShare(static_cast<ELOARaidStat>(Index)) > 0.f)
		{
			Stats.Add(static_cast<ELOARaidStat>(Index));
		}
	}

	// 몫이 큰 것부터. 같으면 enum 순서 (StableSort)
	Stats.StableSort([this](ELOARaidStat A, ELOARaidStat B) { return GetShare(A) > GetShare(B); });

	if (Stats.Num() > MaxCount)
	{
		Stats.SetNum(FMath::Max(MaxCount, 0));
	}
	return Stats;
}
