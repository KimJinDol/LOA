// LOAPartySubsystem.cpp

#include "Party/LOAPartySubsystem.h"
#include "Party/LOAPartyBeaconHostObject.h"
#include "Party/LOAPartyBeaconClient.h"
#include "Party/LOALobbySettings.h"
#include "Data/LOARaidData.h"
#include "Player/LOALocalPlayer.h"
#include "Core/LOALog.h"

#include "OnlineBeaconHost.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartySubsystem)

namespace LOAPartySession
{
	/** 파티 광고 세션 이름. 엔진 기본 NAME_GameSession과 다르게 → 레이드 맵의 GameSession이 건드리지 않는다 */
	static const FName SessionName(TEXT("LOAParty"));

	/** 광고에 싣는 칸 (방 목록 한 줄) */
	static const FName TitleKey(TEXT("LOA_TITLE"));
	static const FName LeaderKey(TEXT("LOA_LEADER"));
	static const FName MembersKey(TEXT("LOA_MEMBERS"));
	/** 어느 레이드 모집인가 = 레이드 데이터 ID 글자 (예: "Raid:DA_Raid_ReviveBeastHeart") */
	static const FName RaidKey(TEXT("LOA_RAID"));
	/** 파티마다 하나인 글자 (내 파티 찾기) */
	static const FName PartyCodeKey(TEXT("LOA_PARTY_CODE"));

	/** 검색용 세션 기능 이름 꼬리. 이 창의 기본 이름 뒤에 붙인다 (Context_1 → Context_1_PartySearch) */
	static const TCHAR* SearchInstanceSuffix = TEXT("_PartySearch");

	/** 목록 정렬: 내 파티가 앞 (나머지 순서는 StableSort가 지킨다) */
	static bool IsMyPartyFirst(const FLOAPartyListEntry& A, const FLOAPartyListEntry& B)
	{
		return A.bMyParty && !B.bMyParty;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 시작·끝
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 입장 다시 시도: 접속 실패를 엔진 알림으로 안다 (UnrealEngine.cpp TickWorldTravel: PendingConnectionFailure → CancelPending)
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ULOAPartySubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ULOAPartySubsystem::HandleTravelFailure);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULOAPartySubsystem::HandlePostLoadMap);
}

void ULOAPartySubsystem::Deinitialize()
{
	ResetParty();
	DestroySearchSubsystem();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// 파티장
// ─────────────────────────────────────────────────────────────────────────────

bool ULOAPartySubsystem::CreateParty(const FString& Title)
{
	if (Role != ELOAPartyRole::None)
	{
		LOA_LOG(this, Warning, TEXT("[Party] CreateParty: already in a party."));
		return false;
	}

	FLOAPartyMember Me;
	if (!MakeMyMemberInfo(Me))
	{
		LOA_LOG(this, Warning, TEXT("[Party] CreateParty: set your name first."));
		return false;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!World || !Sessions.IsValid())
	{
		LOA_LOG(this, Error, TEXT("[Party] CreateParty: no world or online session interface."));
		return false;
	}

	const ULOALobbySettings* LobbySettings = GetDefault<ULOALobbySettings>();
	const FString ClampedTitle = Title.Left(LobbySettings->MaxPartyTitleLength);

	// 어느 레이드 모집인가. 작은 데이터 에셋이라 바로 불러온다 (방 만들기 버튼을 누른 순간 한 번)
	const ULOARaidData* RaidData = LobbySettings->RaidData.LoadSynchronous();
	if (!RaidData)
	{
		LOA_LOG(this, Error, TEXT("[Party] CreateParty: Raid Data is not set (Project Settings > LOA Lobby)."));
		return false;
	}
	const FPrimaryAssetId RaidId = RaidData->GetPrimaryAssetId();

	// ① Beacon 호스트: 포트를 듣는 엔진 액터 + 우리 판단 객체
	AOnlineBeaconHost* NewBeaconHost = World->SpawnActor<AOnlineBeaconHost>();
	if (!NewBeaconHost || !NewBeaconHost->InitHost())
	{
		LOA_LOG(this, Error, TEXT("[Party] CreateParty: beacon host failed to listen (port in use?)."));
		if (NewBeaconHost)
		{
			NewBeaconHost->DestroyBeacon();
		}
		return false;
	}

	ALOAPartyBeaconHostObject* NewHostObject = World->SpawnActor<ALOAPartyBeaconHostObject>();
	NewHostObject->InitParty(Me, RaidId, ClampedTitle, RaidData->MaxPartyMembers);
	NewHostObject->OnPartyChanged.BindUObject(this, &ULOAPartySubsystem::HandleHostPartyChanged);
	NewHostObject->OnNotice.BindUObject(this, &ULOAPartySubsystem::HandleHostNotice);

	NewBeaconHost->RegisterHost(NewHostObject);
	// 엔진 Beacon은 처음에 요청을 막아 둔다 (AOnlineBeacon 생성자 DenyRequests) → 연다
	NewBeaconHost->PauseBeaconRequests(false);

	BeaconHost = NewBeaconHost;
	HostObject = NewHostObject;
	MyPartyCode = FGuid::NewGuid().ToString();

	// ② LAN 세션 광고 (목록에 보이게). Beacon 포트를 실어 보낸다
	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = true;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bUsesPresence = false;
	// 세션 접속을 쓰지 않으므로 빈자리 수는 줄지 않는다 → 다 차도 목록에 보임 (실제 인원은 MembersKey)
	SessionSettings.NumPublicConnections = RaidData->MaxPartyMembers;
	SessionSettings.Set(SETTING_BEACONPORT, NewBeaconHost->GetListenPort(), EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(LOAPartySession::TitleKey, ClampedTitle, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(LOAPartySession::LeaderKey, Me.PlayerName, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(LOAPartySession::MembersKey, 1, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(LOAPartySession::RaidKey, RaidId.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(LOAPartySession::PartyCodeKey, MyPartyCode, EOnlineDataAdvertisementType::ViaOnlineService);

	// 지난 광고가 남아 있으면 지운다 (같은 이름으로는 못 만듦)
	if (Sessions->GetNamedSession(LOAPartySession::SessionName))
	{
		Sessions->DestroySession(LOAPartySession::SessionName);
	}

	CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ULOAPartySubsystem::HandleCreateSessionComplete));

	PartyInfo = NewHostObject->GetPartyInfo();
	SetRole(ELOAPartyRole::Leader);

	if (!Sessions->CreateSession(0, LOAPartySession::SessionName, SessionSettings))
	{
		// 실패해도 알림(HandleCreateSessionComplete)이 바로 온다 → 거기서 정리
		LOA_LOG(this, Error, TEXT("[Party] CreateSession call failed."));
	}
	return true;
}

void ULOAPartySubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != LOAPartySession::SessionName)
	{
		return;
	}

	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
	}

	if (!bWasSuccessful)
	{
		LOA_LOG(this, Error, TEXT("[Party] Failed to advertise the party (LAN session)."));
		ResetParty();
		return;
	}

	LOA_LOG(this, Log, TEXT("[Party] Party advertised on LAN (beacon port %d)."),
		BeaconHost.IsValid() ? BeaconHost->GetListenPort() : 0);
}

void ULOAPartySubsystem::HandleHostPartyChanged()
{
	if (!HostObject.IsValid())
	{
		return;
	}

	PartyInfo = HostObject->GetPartyInfo();
	UpdateAdvertisedMembers();
	OnPartyChanged.Broadcast();
}

void ULOAPartySubsystem::HandleHostNotice(const FLOAPartyNotice& Notice)
{
	LOA_LOG(this, Log, TEXT("[Party] Notice %s [%s]."), *UEnum::GetValueAsString(Notice.Type), *Notice.PlayerName);
	OnPartyNotice.Broadcast(Notice);
}

void ULOAPartySubsystem::UpdateAdvertisedMembers()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	FOnlineSessionSettings* Current = Sessions.IsValid() ? Sessions->GetSessionSettings(LOAPartySession::SessionName) : nullptr;
	if (!Current)
	{
		return;
	}

	int32 AdvertisedMembers = 0;
	Current->Get(LOAPartySession::MembersKey, AdvertisedMembers);
	if (AdvertisedMembers == PartyInfo.Members.Num())
	{
		return;
	}

	FOnlineSessionSettings Updated = *Current;
	Updated.Set(LOAPartySession::MembersKey, PartyInfo.Members.Num(), EOnlineDataAdvertisementType::ViaOnlineService);
	Sessions->UpdateSession(LOAPartySession::SessionName, Updated, true);
}

bool ULOAPartySubsystem::AcceptApplicant(int32 MemberId)
{
	return Role == ELOAPartyRole::Leader && HostObject.IsValid() && HostObject->Accept(MemberId);
}

bool ULOAPartySubsystem::RejectApplicant(int32 MemberId)
{
	return Role == ELOAPartyRole::Leader && HostObject.IsValid() && HostObject->Reject(MemberId);
}

bool ULOAPartySubsystem::KickMember(int32 MemberId)
{
	return Role == ELOAPartyRole::Leader && HostObject.IsValid() && HostObject->Kick(MemberId);
}

bool ULOAPartySubsystem::EnterRaid()
{
	if (Role != ELOAPartyRole::Leader || !HostObject.IsValid())
	{
		LOA_LOG(this, Warning, TEXT("[Party] EnterRaid: only the leader can enter."));
		return false;
	}

	// 이 파티의 레이드 맵 (아래에서 파티 정보를 비우기 전에 꺼내 둔다)
	const ULOARaidData* RaidData = ULOARaidData::LoadById(PartyInfo.RaidId);
	if (!RaidData || RaidData->RaidMap.IsNull())
	{
		LOA_LOG(this, Error, TEXT("[Party] EnterRaid: no raid data or raid map for [%s]."), *PartyInfo.RaidId.ToString());
		return false;
	}
	const TSoftObjectPtr<UWorld> RaidMap = RaidData->RaidMap;

	FString RaidAddress;
	if (!MakeRaidAddress(RaidAddress))
	{
		LOA_LOG(this, Error, TEXT("[Party] EnterRaid: could not resolve this computer's address."));
		return false;
	}

	// ① 파티원에게 주소 → 연결 닫기 (보낼 것을 먼저 보낸 뒤 닫음)
	HostObject->SendEnterAndClose(RaidAddress);

	// ② 목록에서 내리고 Beacon 닫기. 파티는 여기서 끝 (레이드 뒤 파티 유지는 나중)
	CloseHost();
	PartyInfo = FLOAPartyInfo();
	SetRole(ELOAPartyRole::None);

	// ③ 레이드 맵을 호스트로 연다. 고른 직업·이름은 엔진이 접속 주소에 붙인다
	LOA_LOG(this, Log, TEXT("[Party] Leader opening raid map as listen server (%s)."), *RaidAddress);
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, RaidMap, true, TEXT("listen"));
	return true;
}

bool ULOAPartySubsystem::MakeRaidAddress(FString& OutAddress) const
{
	// 방 광고의 주소 = 지원자가 찾아와 Beacon으로 붙은 주소 (OnlineSessionInterfaceNull: GetLocalHostAddr)
	// 거기서 IP만 쓰고 포트는 게임 포트로 (로비에는 게임 넷 드라이버가 없어 광고의 게임 포트는 0이다: GetPortFromNetDriver)
	IOnlineSessionPtr Sessions = GetSessionInterface();
	FString BeaconConnectString;
	if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(LOAPartySession::SessionName, BeaconConnectString, NAME_BeaconPort))
	{
		return false;
	}

	FString HostIp;
	FString UnusedPort;
	if (!BeaconConnectString.Split(TEXT(":"), &HostIp, &UnusedPort, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		return false;
	}

	// ?listen은 엔진 기본 포트로 연다 (FURL 기본값, BaseEngine.ini [URL] Port)
	OutAddress = FString::Printf(TEXT("%s:%d"), *HostIp, FURL::UrlConfig.DefaultPort);
	return true;
}

void ULOAPartySubsystem::CloseHost()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		if (Sessions->GetNamedSession(LOAPartySession::SessionName))
		{
			Sessions->DestroySession(LOAPartySession::SessionName);
		}
	}

	if (HostObject.IsValid())
	{
		HostObject->OnPartyChanged.Unbind();
		HostObject->OnNotice.Unbind();
	}

	if (BeaconHost.IsValid())
	{
		if (HostObject.IsValid())
		{
			BeaconHost->UnregisterHost(HostObject->GetBeaconType());
		}
		// 엔진: 넷 드라이버를 닫고 액터를 없앤다
		BeaconHost->DestroyBeacon();
	}

	if (HostObject.IsValid())
	{
		HostObject->Destroy();
	}

	BeaconHost.Reset();
	HostObject.Reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// 검색
// ─────────────────────────────────────────────────────────────────────────────

bool ULOAPartySubsystem::FindParties()
{
	// 역할은 안 본다: 파티 중에도 검색 (11-21 사용자). 검색용 세션 기능이라 내 광고는 안 멈춘다 (헤더 주석)
	if (IsSearching())
	{
		LOA_LOG(this, Warning, TEXT("[Party] FindParties: already searching."));
		return false;
	}

	IOnlineSessionPtr Sessions = GetSearchSessionInterface();
	if (!Sessions.IsValid())
	{
		return false;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = GetDefault<ULOALobbySettings>()->MaxSearchResults;

	FindSessionsHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ULOAPartySubsystem::HandleFindSessionsComplete));

	// 반환값 = 검색을 "시작"했나 (결과는 1~2초 뒤 HandleFindSessionsComplete로 온다)
	// Null은 시작이 실패해도 완료 알림(false)을 먼저 부른다 (OnlineSessionInterfaceNull.cpp FindLANSession)
	// → 여기 정리는 알림 없이 false만 돌려주는 온라인 기능에 대한 대비
	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		SessionSearch.Reset();
		LOA_LOG(this, Warning, TEXT("[Party] FindSessions call failed."));
		return false;
	}

	LOA_LOG(this, Log, TEXT("[Party] Searching LAN parties..."));
	return true;
}

void ULOAPartySubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSearchSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
	}

	Listings.Reset();
	if (bWasSuccessful && Sessions.IsValid() && SessionSearch.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
		{
			FLOAPartyListEntry Entry;
			const FOnlineSessionSettings& Advertised = Result.Session.SessionSettings;
			FString RaidIdString;
			Advertised.Get(LOAPartySession::RaidKey, RaidIdString);
			Entry.RaidId = FPrimaryAssetId::ParseTypeAndName(RaidIdString);	// ToString()의 반대 ("Raid:DA_…" → ID)
			Advertised.Get(LOAPartySession::TitleKey, Entry.Title);
			Advertised.Get(LOAPartySession::LeaderKey, Entry.LeaderName);
			Advertised.Get(LOAPartySession::MembersKey, Entry.NumMembers);
			Advertised.Get(LOAPartySession::PartyCodeKey, Entry.PartyCode);
			Entry.bMyParty = Role != ELOAPartyRole::None && !MyPartyCode.IsEmpty() && Entry.PartyCode == MyPartyCode;
			Entry.MaxMembers = Advertised.NumPublicConnections;

			// 다른 게임의 광고는 엔진이 이미 거른다 (DefaultEngine.ini [LANSession] LanGameUniqueId: LANBeacon.cpp가 패킷의 게임 번호를 비교)
			// 여기는 우리 게임인데 파티 광고가 아닌 세션 대비 (파티 광고에는 파티장 이름·레이드가 꼭 있다)
			if (Entry.LeaderName.IsEmpty() || !Entry.RaidId.IsValid())
			{
				continue;
			}

			// Beacon이 붙을 주소 = 광고의 IP + 광고에 실은 Beacon 포트
			if (!Sessions->GetResolvedConnectString(Result, NAME_BeaconPort, Entry.BeaconAddress))
			{
				continue;
			}
			Listings.Add(Entry);
		}
	}

	// 내 파티를 맨 위로 (11-21 사용자). 나머지 순서는 그대로. 목록 번호(ApplyToParty)는 정렬 뒤 번호
	Listings.StableSort(&LOAPartySession::IsMyPartyFirst);

	SessionSearch.Reset();
	LOA_LOG(this, Log, TEXT("[Party] Found %d party(s)."), Listings.Num());
	OnPartiesFound.Broadcast(Listings);
}

// ─────────────────────────────────────────────────────────────────────────────
// 지원자·파티원
// ─────────────────────────────────────────────────────────────────────────────

bool ULOAPartySubsystem::ApplyToParty(int32 ListingIndex)
{
	if (Role != ELOAPartyRole::None || !Listings.IsValidIndex(ListingIndex))
	{
		LOA_LOG(this, Warning, TEXT("[Party] ApplyToParty: already in a party or bad index %d."), ListingIndex);
		return false;
	}

	FLOAPartyMember Me;
	if (!MakeMyMemberInfo(Me))
	{
		LOA_LOG(this, Warning, TEXT("[Party] ApplyToParty: set your name first."));
		return false;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	ALOAPartyBeaconClient* NewClient = World ? World->SpawnActor<ALOAPartyBeaconClient>() : nullptr;
	if (!NewClient)
	{
		return false;
	}

	NewClient->OnConnectedToHost.BindUObject(this, &ULOAPartySubsystem::HandleBeaconConnected);
	NewClient->OnConnectionClosed.BindUObject(this, &ULOAPartySubsystem::HandleBeaconClosed);
	NewClient->OnApplyResult.BindUObject(this, &ULOAPartySubsystem::HandleApplyResult);
	NewClient->OnPartyInfo.BindUObject(this, &ULOAPartySubsystem::HandlePartyInfoReceived);
	NewClient->OnNotice.BindUObject(this, &ULOAPartySubsystem::HandleNoticeReceived);
	NewClient->OnRemoved.BindUObject(this, &ULOAPartySubsystem::HandleRemoved);
	NewClient->OnEnterRaid.BindUObject(this, &ULOAPartySubsystem::HandleEnterRaid);
	ClientBeacon = NewClient;

	const FLOAPartyListEntry& Entry = Listings[ListingIndex];
	FURL BeaconURL(nullptr, *Entry.BeaconAddress, TRAVEL_Absolute);
	if (!NewClient->InitClient(BeaconURL))
	{
		LOA_LOG(this, Warning, TEXT("[Party] Could not start beacon connection to %s."), *Entry.BeaconAddress);
		DetachClientBeacon();
		return false;
	}

	// 연결되면(HandleBeaconConnected) 신청을 보낸다. 그 사이도 "기다리는 중"
	PartyInfo = FLOAPartyInfo();
	PartyInfo.RaidId = Entry.RaidId;
	PartyInfo.Title = Entry.Title;
	PartyInfo.MaxMembers = Entry.MaxMembers;
	MyPartyCode = Entry.PartyCode;
	SetRole(ELOAPartyRole::Applicant);

	LOA_LOG(this, Log, TEXT("[Party] Applying to '%s' (%s) at %s."), *Entry.Title, *Entry.LeaderName, *Entry.BeaconAddress);
	return true;
}

void ULOAPartySubsystem::HandleBeaconConnected()
{
	FLOAPartyMember Me;
	if (ClientBeacon.IsValid() && MakeMyMemberInfo(Me))
	{
		ClientBeacon->ServerApply(Me);
	}
}

void ULOAPartySubsystem::HandleApplyResult(bool bAccepted)
{
	if (bAccepted)
	{
		LOA_LOG(this, Log, TEXT("[Party] Accepted."));
		// 파티 정보·참여 알림은 바로 뒤에 온다 (같은 연결, 보낸 순서대로)
		SetRole(ELOAPartyRole::Member);
		return;
	}

	LOA_LOG(this, Log, TEXT("[Party] Rejected."));
	ResetParty();
	NotifySelf(ELOAPartyNoticeType::Rejected);
}

void ULOAPartySubsystem::HandlePartyInfoReceived(const FLOAPartyInfo& Info)
{
	PartyInfo = Info;
	OnPartyChanged.Broadcast();
}

void ULOAPartySubsystem::HandleNoticeReceived(const FLOAPartyNotice& Notice)
{
	LOA_LOG(this, Log, TEXT("[Party] Notice %s [%s]."), *UEnum::GetValueAsString(Notice.Type), *Notice.PlayerName);
	OnPartyNotice.Broadcast(Notice);
}

void ULOAPartySubsystem::HandleRemoved(ELOAPartyNoticeType Reason)
{
	LOA_LOG(this, Log, TEXT("[Party] Removed from party (%s)."), *UEnum::GetValueAsString(Reason));
	ResetParty();
	NotifySelf(Reason);
}

void ULOAPartySubsystem::HandleEnterRaid(const FString& Address)
{
	LOA_LOG(this, Log, TEXT("[Party] Enter raid signal: %s."), *Address);

	// 곧 파티장이 연결을 닫는다 → 그 닫힘은 "끊김"이 아니다 (HandleBeaconClosed가 PendingRaidAddress를 본다)
	PendingRaidAddress = Address;
	RaidConnectAttempts = 0;
	DetachClientBeacon();

	// 파티장이 레이드 맵을 여는 동안 기다렸다가 접속
	GetGameInstance()->GetTimerManager().SetTimer(RaidConnectTimer, this, &ULOAPartySubsystem::ConnectToRaid,
		FMath::Max(GetDefault<ULOALobbySettings>()->EnterConnectDelay, KINDA_SMALL_NUMBER), false);
}

void ULOAPartySubsystem::HandleBeaconClosed()
{
	if (IsEnteringRaid() || Role == ELOAPartyRole::None)
	{
		return;
	}

	// 신호 없이 끊김 (파티장 창 닫힘, 시간 초과, 신청이 연결 전에 실패)
	LOA_LOG(this, Log, TEXT("[Party] Connection to the leader closed."));
	ResetParty();
	NotifySelf(ELOAPartyNoticeType::ConnectionLost);
}

void ULOAPartySubsystem::LeaveParty()
{
	switch (Role)
	{
	case ELOAPartyRole::Leader:
		if (HostObject.IsValid())
		{
			HostObject->Disband();
		}
		break;

	case ELOAPartyRole::Applicant:
	case ELOAPartyRole::Member:
		if (ClientBeacon.IsValid())
		{
			// 파티장이 받으면 연결을 닫는다. 안 받아도 아래에서 우리가 없앤다
			ClientBeacon->ServerLeave();
		}
		break;

	default:
		return;
	}

	LOA_LOG(this, Log, TEXT("[Party] Left the party."));
	ResetParty();
}

void ULOAPartySubsystem::DetachClientBeacon()
{
	ALOAPartyBeaconClient* Client = ClientBeacon.Get();
	ClientBeacon.Reset();
	if (!Client)
	{
		return;
	}

	Client->OnConnectedToHost.Unbind();
	Client->OnConnectionClosed.Unbind();
	Client->OnApplyResult.Unbind();
	Client->OnPartyInfo.Unbind();
	Client->OnNotice.Unbind();
	Client->OnRemoved.Unbind();
	Client->OnEnterRaid.Unbind();

	// 지금 이 연결의 RPC 안일 수 있다 → 다음 프레임에 없앤다
	DetachedClientBeacon = Client;
	GetGameInstance()->GetTimerManager().SetTimerForNextTick(this, &ULOAPartySubsystem::DestroyDetachedClientBeacon);
}

void ULOAPartySubsystem::DestroyDetachedClientBeacon()
{
	if (DetachedClientBeacon.IsValid())
	{
		// 엔진: 넷 드라이버를 닫고 액터를 없앤다
		DetachedClientBeacon->DestroyBeacon();
	}
	DetachedClientBeacon.Reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// 입장 (파티원)
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartySubsystem::ConnectToRaid()
{
	if (!IsEnteringRaid())
	{
		return;
	}

	// 접속 실패 뒤 로비가 다시 열리는 중이면 컨트롤러가 없을 수 있다 → 다음 시도로
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PC)
	{
		ScheduleRaidRetry();
		return;
	}

	++RaidConnectAttempts;
	LOA_LOG(this, Log, TEXT("[Party] Connecting to raid %s (attempt %d)."), *PendingRaidAddress, RaidConnectAttempts);
	PC->ClientTravel(PendingRaidAddress, TRAVEL_Absolute);
}

void ULOAPartySubsystem::ScheduleRaidRetry()
{
	const ULOALobbySettings* LobbySettings = GetDefault<ULOALobbySettings>();

	// 처음 한 번 + 다시 시도 횟수
	if (RaidConnectAttempts > LobbySettings->EnterConnectRetryCount)
	{
		LOA_LOG(this, Warning, TEXT("[Party] Gave up entering raid after %d attempts."), RaidConnectAttempts);
		PendingRaidAddress.Reset();
		RaidConnectAttempts = 0;
		ResetParty();
		NotifySelf(ELOAPartyNoticeType::EnterFailed);
		return;
	}

	GetGameInstance()->GetTimerManager().SetTimer(RaidConnectTimer, this, &ULOAPartySubsystem::ConnectToRaid,
		LobbySettings->EnterConnectRetryInterval, false);
}

void ULOAPartySubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!IsEnteringRaid() || !NetDriver || NetDriver->NetDriverName != NAME_PendingNetDriver)
	{
		return;
	}

	// PIE는 한 프로그램 안에 컴퓨터가 여럿 → 이 알림이 내 접속 시도의 것인지 본다
	// 실패한 접속의 넷 드라이버로 월드 컨텍스트를 찾고, 그 주인 GameInstance가 나인지 (Engine.h GetWorldContextFromPendingNetGameNetDriver)
	const FWorldContext* Context = GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver);
	if (!Context || Context->OwningGameInstance != GetGameInstance())
	{
		return;
	}

	LOA_LOG(this, Log, TEXT("[Party] Raid connection failed (network failure %d): %s"), static_cast<int32>(FailureType), *ErrorString);
	ScheduleRaidRetry();
}

void ULOAPartySubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!IsEnteringRaid() || !World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	LOA_LOG(this, Log, TEXT("[Party] Raid travel failed (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	ScheduleRaidRetry();
}

void ULOAPartySubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// 파티원: 레이드 서버에 접속해 맵을 받음 = 입장 성공. 파티는 여기서 끝 (흩어짐, 11-19)
	if (IsEnteringRaid() && LoadedWorld->GetNetMode() == NM_Client)
	{
		LOA_LOG(this, Log, TEXT("[Party] Entered raid at %s."), *PendingRaidAddress);
		GetGameInstance()->GetTimerManager().ClearTimer(RaidConnectTimer);
		PendingRaidAddress.Reset();
		RaidConnectAttempts = 0;
		PartyInfo = FLOAPartyInfo();
		SetRole(ELOAPartyRole::None);
		return;
	}

	// 다른 길로 맵을 옮겨 Beacon 액터가 사라졌으면 (예: 콘솔 open) 파티를 정리한다
	const bool bLostHost = Role == ELOAPartyRole::Leader && !BeaconHost.IsValid();
	const bool bLostClient = (Role == ELOAPartyRole::Applicant || Role == ELOAPartyRole::Member) && !ClientBeacon.IsValid() && !IsEnteringRaid();
	if (bLostHost || bLostClient)
	{
		LOA_LOG(this, Log, TEXT("[Party] Map changed while in a party. Closing the party."));
		ResetParty();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 공통
// ─────────────────────────────────────────────────────────────────────────────

void ULOAPartySubsystem::ResetParty()
{
	CloseHost();
	DetachClientBeacon();

	if (!IsEnteringRaid() && GetGameInstance())
	{
		GetGameInstance()->GetTimerManager().ClearTimer(RaidConnectTimer);
	}

	PartyInfo = FLOAPartyInfo();
	SetRole(ELOAPartyRole::None);
}

void ULOAPartySubsystem::SetRole(ELOAPartyRole NewRole)
{
	if (Role == NewRole)
	{
		return;
	}
	Role = NewRole;
	// 파티가 끝나면(나감·거절·강퇴·해산·입장) 내 파티 표시도 끝
	if (NewRole == ELOAPartyRole::None)
	{
		MyPartyCode.Reset();
	}
	OnPartyChanged.Broadcast();
}

void ULOAPartySubsystem::NotifySelf(ELOAPartyNoticeType Type)
{
	LOA_LOG(this, Log, TEXT("[Party] Notice %s (self)."), *UEnum::GetValueAsString(Type));
	FLOAPartyNotice Notice;
	Notice.Type = Type;
	OnPartyNotice.Broadcast(Notice);
}

bool ULOAPartySubsystem::MakeMyMemberInfo(FLOAPartyMember& OutMember) const
{
	const ULOALocalPlayer* LocalPlayer = Cast<ULOALocalPlayer>(GetGameInstance()->GetFirstGamePlayer());
	if (!LocalPlayer || LocalPlayer->GetSelection().PlayerName.IsEmpty())
	{
		return false;
	}

	OutMember = FLOAPartyMember();
	OutMember.PlayerName = LocalPlayer->GetSelection().PlayerName;
	OutMember.ClassId = LocalPlayer->GetSelection().ClassId;
	return true;
}

IOnlineSessionPtr ULOAPartySubsystem::GetSessionInterface() const
{
	// 온라인 기능(OnlineSubsystem)은 보통 프로그램에 하나지만, PIE는 창 여러 개가 한 프로그램 안에서 돈다.
	// 그래서 엔진이 PIE 창(월드 컨텍스트)마다 따로 만든다 (로그 "Created online subsystem instance for: :Context_1" … Context_3).
	// → 이 컴퓨터(GameInstance)의 월드를 넘겨 그 창의 것을 고른다. 패키징 게임에서는 하나뿐이라 같은 것이 나온다
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

IOnlineSessionPtr ULOAPartySubsystem::GetSearchSessionInterface()
{
	if (SearchSubsystemName.IsNone())
	{
		// 이 창의 기본 세션 기능 이름 뒤에 꼬리를 붙인다. 예) NULL:Context_1 → NULL:Context_1_PartySearch
		// PIE 창마다 기본 이름이 다르니 검색용도 창마다 따로 생긴다
		const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
		const IOnlineSubsystem* MainSubsystem = Online::GetSubsystem(World);
		if (!MainSubsystem)
		{
			return nullptr;
		}
		SearchSubsystemName = FName(*FString::Printf(TEXT("%s:%s%s"),
			*MainSubsystem->GetSubsystemName().ToString(),
			*MainSubsystem->GetInstanceName().ToString(),
			LOAPartySession::SearchInstanceSuffix));
	}

	// 없으면 엔진이 만든다 (OnlineSubsystemModule.cpp GetOnlineSubsystem → Null 공장 CreateSubsystem). 매 프레임은 엔진이 돌린다
	IOnlineSubsystem* SearchSubsystem = IOnlineSubsystem::Get(SearchSubsystemName);
	return SearchSubsystem ? SearchSubsystem->GetSessionInterface() : nullptr;
}

void ULOAPartySubsystem::DestroySearchSubsystem()
{
	if (SearchSubsystemName.IsNone())
	{
		return;
	}

	// 검색 중이면 알림부터 끊는다. 세션 기능 포인터는 이 블록에서만 들고 있다
	// → 지울 때 엔진이 "아무도 안 들고 있나"를 검사한다 (OnlineSubsystemNull.cpp Shutdown: ensure(IsUnique))
	if (IOnlineSubsystem::DoesInstanceExist(SearchSubsystemName))
	{
		IOnlineSessionPtr Sessions = IOnlineSubsystem::Get(SearchSubsystemName)->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		}
	}
	SessionSearch.Reset();

	if (IOnlineSubsystem::DoesInstanceExist(SearchSubsystemName))
	{
		IOnlineSubsystem::Destroy(SearchSubsystemName);
	}
	SearchSubsystemName = NAME_None;
}
