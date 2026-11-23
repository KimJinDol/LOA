// LOAPartyBeaconHostObject.cpp

#include "Party/LOAPartyBeaconHostObject.h"
#include "Party/LOAPartyBeaconClient.h"
#include "Data/LOACharacterSelection.h"
#include "Core/LOALog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyBeaconHostObject)

ALOAPartyBeaconHostObject::ALOAPartyBeaconHostObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 엔진 규칙: 붙은 사람마다 만들 짝 클래스 + 종류 이름(= 짝 클래스 이름). 지원자 쪽 GetBeaconType()과 같아야 이어진다
	ClientBeaconActorClass = ALOAPartyBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}

void ALOAPartyBeaconHostObject::InitParty(const FLOAPartyMember& Leader, const FPrimaryAssetId& RaidId, const FString& Title, int32 MaxMembers)
{
	PartyInfo = FLOAPartyInfo();
	PartyInfo.RaidId = RaidId;
	PartyInfo.Title = Title;
	PartyInfo.MaxMembers = MaxMembers;

	FLOAPartyMember LeaderEntry = Leader;
	LeaderEntry.MemberId = 0;
	LeaderEntry.bLeader = true;
	PartyInfo.Members.Add(LeaderEntry);

	NextMemberId = 1;
	ClientsByMemberId.Reset();

	LOA_LOG(this, Log, TEXT("[Party] Opened party '%s' (%s) by [%s], max %d."), *Title, *RaidId.ToString(), *LeaderEntry.PlayerName, MaxMembers);
}

// ─── 통로에서 ───

void ALOAPartyBeaconHostObject::HandleApply(ALOAPartyBeaconClient* Client, const FLOAPartyMember& Request)
{
	if (!Client)
	{
		return;
	}

	// 같은 연결이 두 번 신청 → 무시 (이미 신청자이거나 파티원)
	if (FindMemberIdByClient(Client) != INDEX_NONE)
	{
		LOA_LOG(this, Warning, TEXT("[Party] Duplicate apply from %s ignored."), *GetNameSafe(Client));
		return;
	}

	// 이름 검사·정원 검사 실패 → 거절하고 닫는다
	if (!IsValidApplyRequest(Request) || PartyInfo.IsFull())
	{
		LOA_LOG(this, Log, TEXT("[Party] Apply from [%s] refused (invalid name or party full)."), *Request.PlayerName);
		Client->ClientApplyResult(false);
		DisconnectClient(Client);
		return;
	}

	FLOAPartyMember Applicant = Request;
	Applicant.MemberId = NextMemberId++;
	Applicant.bLeader = false;
	PartyInfo.Applicants.Add(Applicant);
	ClientsByMemberId.Add(Applicant.MemberId, Client);

	LOA_LOG(this, Log, TEXT("[Party] Applicant #%d [%s] (%s)."), Applicant.MemberId, *Applicant.PlayerName, *Applicant.ClassId.ToString());

	// 신청 알림은 파티장 화면에만 (신청자 탭은 파티장만, 11-19 사용자)
	FLOAPartyNotice Notice;
	Notice.Type = ELOAPartyNoticeType::Applied;
	Notice.PlayerName = Applicant.PlayerName;
	OnNotice.ExecuteIfBound(Notice);
	OnPartyChanged.ExecuteIfBound();
}

void ALOAPartyBeaconHostObject::HandleLeave(ALOAPartyBeaconClient* Client)
{
	const int32 MemberId = FindMemberIdByClient(Client);
	if (MemberId == INDEX_NONE)
	{
		return;
	}

	FString RemovedMemberName;
	const bool bWasMember = RemoveEntry(MemberId, RemovedMemberName);

	// 목록에서 먼저 뺐으므로, 닫힘 알림(NotifyClientDisconnected)이 와도 다시 처리하지 않는다
	DisconnectClient(Client);

	if (bWasMember)
	{
		BroadcastPartyInfo();
		BroadcastNotice(ELOAPartyNoticeType::Left, RemovedMemberName);
	}
	else
	{
		// 지원 취소: 파티장 화면의 신청자 탭만 바뀜
		OnPartyChanged.ExecuteIfBound();
	}
}

void ALOAPartyBeaconHostObject::NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor)
{
	const int32 MemberId = FindMemberIdByClient(LeavingClientActor);

	// 엔진: 목록 정리
	Super::NotifyClientDisconnected(LeavingClientActor);

	if (MemberId == INDEX_NONE)
	{
		// 이미 뺀 사람 (나가기·거절·강퇴·입장에서 우리가 닫음)
		return;
	}

	// 말없이 끊김 (창 닫기·시간 초과) → 나가기와 같게
	LOA_LOG(this, Log, TEXT("[Party] #%d disconnected without leaving."), MemberId);

	FString RemovedMemberName;
	if (RemoveEntry(MemberId, RemovedMemberName))
	{
		BroadcastPartyInfo();
		BroadcastNotice(ELOAPartyNoticeType::Left, RemovedMemberName);
	}
	else
	{
		OnPartyChanged.ExecuteIfBound();
	}
}

// ─── 파티장의 지시 ───

bool ALOAPartyBeaconHostObject::Accept(int32 MemberId)
{
	const FLOAPartyMember* Found = PartyInfo.FindApplicant(MemberId);
	ALOAPartyBeaconClient* Client = FindClient(MemberId);
	if (!Found || !Client)
	{
		LOA_LOG(this, Warning, TEXT("[Party] Accept: no applicant #%d."), MemberId);
		return false;
	}

	if (PartyInfo.IsFull())
	{
		LOA_LOG(this, Warning, TEXT("[Party] Accept: party is full (%d/%d)."), PartyInfo.Members.Num(), PartyInfo.MaxMembers);
		return false;
	}

	// 신청자 → 파티원 (번호·연결은 그대로)
	const FLOAPartyMember NewMember = *Found;
	for (int32 Index = 0; Index < PartyInfo.Applicants.Num(); ++Index)
	{
		if (PartyInfo.Applicants[Index].MemberId == MemberId)
		{
			PartyInfo.Applicants.RemoveAt(Index);
			break;
		}
	}
	PartyInfo.Members.Add(NewMember);

	LOA_LOG(this, Log, TEXT("[Party] Accepted #%d [%s] (%d/%d)."), MemberId, *NewMember.PlayerName, PartyInfo.Members.Num(), PartyInfo.MaxMembers);

	// 본인에게 결과 먼저 → 그다음 파티 정보 (Reliable은 같은 연결 안에서 보낸 순서대로 도착)
	Client->ClientApplyResult(true);
	BroadcastPartyInfo();
	BroadcastNotice(ELOAPartyNoticeType::Joined, NewMember.PlayerName);
	return true;
}

bool ALOAPartyBeaconHostObject::Reject(int32 MemberId)
{
	ALOAPartyBeaconClient* Client = FindClient(MemberId);
	if (!PartyInfo.FindApplicant(MemberId) || !Client)
	{
		LOA_LOG(this, Warning, TEXT("[Party] Reject: no applicant #%d."), MemberId);
		return false;
	}

	FString UnusedName;
	RemoveEntry(MemberId, UnusedName);

	Client->ClientApplyResult(false);
	DisconnectClient(Client);

	LOA_LOG(this, Log, TEXT("[Party] Rejected #%d."), MemberId);
	OnPartyChanged.ExecuteIfBound();
	return true;
}

bool ALOAPartyBeaconHostObject::Kick(int32 MemberId)
{
	ALOAPartyBeaconClient* Client = FindClient(MemberId);
	if (MemberId == 0 || !PartyInfo.FindMember(MemberId) || !Client)
	{
		LOA_LOG(this, Warning, TEXT("[Party] Kick: no member #%d (leader cannot be kicked)."), MemberId);
		return false;
	}

	FString KickedName;
	RemoveEntry(MemberId, KickedName);

	Client->ClientRemoved(ELOAPartyNoticeType::Kicked);
	DisconnectClient(Client);

	LOA_LOG(this, Log, TEXT("[Party] Kicked #%d [%s]."), MemberId, *KickedName);
	BroadcastPartyInfo();
	BroadcastNotice(ELOAPartyNoticeType::Kicked, KickedName);
	return true;
}

void ALOAPartyBeaconHostObject::SendEnterAndClose(const FString& Address)
{
	TArray<int32> MemberIds;
	ClientsByMemberId.GetKeys(MemberIds);

	TArray<ALOAPartyBeaconClient*> ToClose;
	for (const int32 MemberId : MemberIds)
	{
		ALOAPartyBeaconClient* Client = FindClient(MemberId);
		if (!Client)
		{
			continue;
		}

		if (PartyInfo.FindMember(MemberId))
		{
			Client->ClientEnterRaid(Address);
		}
		else
		{
			// 수락 안 된 신청자는 거절로 정리
			Client->ClientApplyResult(false);
		}
		ToClose.Add(Client);
	}

	LOA_LOG(this, Log, TEXT("[Party] Enter raid at %s. Sent to %d member(s), closing %d beacon connection(s)."),
		*Address, PartyInfo.Members.Num() - 1, ToClose.Num());

	// 표를 비운 뒤 닫는다 → 닫힘 알림(NotifyClientDisconnected)이 와도 "이미 뺀 사람"
	// DisconnectClient는 닫기 전에 보낼 것을 먼저 보낸다 (엔진 AOnlineBeaconHost::DisconnectClient → FlushNet)
	ClientsByMemberId.Reset();
	PartyInfo.Applicants.Reset();
	for (ALOAPartyBeaconClient* Client : ToClose)
	{
		DisconnectClient(Client);
	}
}

void ALOAPartyBeaconHostObject::Disband()
{
	TArray<int32> MemberIds;
	ClientsByMemberId.GetKeys(MemberIds);

	TArray<ALOAPartyBeaconClient*> ToClose;
	for (const int32 MemberId : MemberIds)
	{
		if (ALOAPartyBeaconClient* Client = FindClient(MemberId))
		{
			Client->ClientRemoved(ELOAPartyNoticeType::Disbanded);
			ToClose.Add(Client);
		}
	}

	ClientsByMemberId.Reset();
	PartyInfo = FLOAPartyInfo();

	for (ALOAPartyBeaconClient* Client : ToClose)
	{
		DisconnectClient(Client);
	}

	LOA_LOG(this, Log, TEXT("[Party] Disbanded. Closed %d connection(s)."), ToClose.Num());
}

// ─── 도우미 ───

bool ALOAPartyBeaconHostObject::RemoveEntry(int32 MemberId, FString& OutRemovedMemberName)
{
	ClientsByMemberId.Remove(MemberId);

	for (int32 Index = 0; Index < PartyInfo.Members.Num(); ++Index)
	{
		if (PartyInfo.Members[Index].MemberId == MemberId)
		{
			OutRemovedMemberName = PartyInfo.Members[Index].PlayerName;
			PartyInfo.Members.RemoveAt(Index);
			return true;
		}
	}

	for (int32 Index = 0; Index < PartyInfo.Applicants.Num(); ++Index)
	{
		if (PartyInfo.Applicants[Index].MemberId == MemberId)
		{
			PartyInfo.Applicants.RemoveAt(Index);
			return false;
		}
	}
	return false;
}

int32 ALOAPartyBeaconHostObject::FindMemberIdByClient(const AOnlineBeaconClient* Client) const
{
	for (const TPair<int32, TObjectPtr<ALOAPartyBeaconClient>>& Pair : ClientsByMemberId)
	{
		if (Pair.Value == Client)
		{
			return Pair.Key;
		}
	}
	return INDEX_NONE;
}

ALOAPartyBeaconClient* ALOAPartyBeaconHostObject::FindClient(int32 MemberId) const
{
	const TObjectPtr<ALOAPartyBeaconClient>* Found = ClientsByMemberId.Find(MemberId);
	return Found ? Found->Get() : nullptr;
}

void ALOAPartyBeaconHostObject::BroadcastPartyInfo()
{
	// 파티원에게는 신청자를 뺀 사본 (필요한 만큼만)
	FLOAPartyInfo MemberView = PartyInfo;
	MemberView.Applicants.Reset();

	for (const FLOAPartyMember& Member : PartyInfo.Members)
	{
		if (ALOAPartyBeaconClient* Client = FindClient(Member.MemberId))
		{
			Client->ClientPartyInfo(MemberView);
		}
	}

	OnPartyChanged.ExecuteIfBound();
}

void ALOAPartyBeaconHostObject::BroadcastNotice(ELOAPartyNoticeType Type, const FString& PlayerName)
{
	FLOAPartyNotice Notice;
	Notice.Type = Type;
	Notice.PlayerName = PlayerName;

	for (const FLOAPartyMember& Member : PartyInfo.Members)
	{
		if (ALOAPartyBeaconClient* Client = FindClient(Member.MemberId))
		{
			Client->ClientNotice(Notice);
		}
	}

	OnNotice.ExecuteIfBound(Notice);
}

bool ALOAPartyBeaconHostObject::IsValidApplyRequest(const FLOAPartyMember& Request) const
{
	// 이름 규칙은 한 곳 (로비 입력 칸과 같은 함수). 고쳐 보니 달라지면 규칙에 안 맞는 이름
	return !Request.PlayerName.IsEmpty()
		&& FLOACharacterSelection::MakeValidPlayerName(Request.PlayerName) == Request.PlayerName;
}
