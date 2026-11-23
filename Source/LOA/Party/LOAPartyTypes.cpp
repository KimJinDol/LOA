// LOAPartyTypes.cpp

#include "Party/LOAPartyTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyTypes)

namespace LOAPartyTypes
{
	/** 목록에서 번호가 같은 멤버. 없으면 nullptr */
	static const FLOAPartyMember* FindById(const TArray<FLOAPartyMember>& List, int32 MemberId)
	{
		for (const FLOAPartyMember& Member : List)
		{
			if (Member.MemberId == MemberId)
			{
				return &Member;
			}
		}
		return nullptr;
	}
}

const FLOAPartyMember* FLOAPartyInfo::FindMember(int32 MemberId) const
{
	return LOAPartyTypes::FindById(Members, MemberId);
}

const FLOAPartyMember* FLOAPartyInfo::FindApplicant(int32 MemberId) const
{
	return LOAPartyTypes::FindById(Applicants, MemberId);
}
