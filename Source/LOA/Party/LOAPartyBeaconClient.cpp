// LOAPartyBeaconClient.cpp

#include "Party/LOAPartyBeaconClient.h"
#include "Party/LOAPartyBeaconHostObject.h"
#include "Core/LOALog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyBeaconClient)

// ─── 서버 RPC (파티장 컴퓨터에서 실행) → 판단은 HostObject ───

void ALOAPartyBeaconClient::ServerApply_Implementation(const FLOAPartyMember& Request)
{
	if (ALOAPartyBeaconHostObject* HostObject = Cast<ALOAPartyBeaconHostObject>(GetBeaconOwner()))
	{
		HostObject->HandleApply(this, Request);
	}
}

void ALOAPartyBeaconClient::ServerLeave_Implementation()
{
	if (ALOAPartyBeaconHostObject* HostObject = Cast<ALOAPartyBeaconHostObject>(GetBeaconOwner()))
	{
		HostObject->HandleLeave(this);
	}
}

// ─── 클라 RPC (지원자 컴퓨터에서 실행) → 알림만 ───

void ALOAPartyBeaconClient::ClientApplyResult_Implementation(bool bAccepted)
{
	OnApplyResult.ExecuteIfBound(bAccepted);
}

void ALOAPartyBeaconClient::ClientPartyInfo_Implementation(const FLOAPartyInfo& Info)
{
	OnPartyInfo.ExecuteIfBound(Info);
}

void ALOAPartyBeaconClient::ClientNotice_Implementation(const FLOAPartyNotice& Notice)
{
	OnNotice.ExecuteIfBound(Notice);
}

void ALOAPartyBeaconClient::ClientRemoved_Implementation(ELOAPartyNoticeType Reason)
{
	OnRemoved.ExecuteIfBound(Reason);
}

void ALOAPartyBeaconClient::ClientEnterRaid_Implementation(const FString& Address)
{
	OnEnterRaid.ExecuteIfBound(Address);
}

// ─── 엔진 알림 ───

void ALOAPartyBeaconClient::OnConnected()
{
	Super::OnConnected();

	LOA_LOG(this, Log, TEXT("[Party] Beacon connected to host."));
	OnConnectedToHost.ExecuteIfBound();
}

void ALOAPartyBeaconClient::OnFailure()
{
	LOA_LOG(this, Log, TEXT("[Party] Beacon connection failed or closed."));
	NotifyConnectionClosed();

	// 엔진: 넷 드라이버 정리
	Super::OnFailure();
}

void ALOAPartyBeaconClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	NotifyConnectionClosed();
	Super::EndPlay(EndPlayReason);
}

void ALOAPartyBeaconClient::NotifyConnectionClosed()
{
	if (bClosedNotified)
	{
		return;
	}
	bClosedNotified = true;
	OnConnectionClosed.ExecuteIfBound();
}
