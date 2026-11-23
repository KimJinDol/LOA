// LOAPartySubsystem.h
// [배관] 이 컴퓨터의 파티 상태. 방 만들기·목록 검색(LAN 세션) + 신청~입장(Beacon). UI와 치트의 입구. (2026-11-19 apply131, 11-20 apply132 레이드 데이터)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   ① 파티장: 방 만들기 = LAN 세션 광고 + Beacon 호스트(ALOAPartyBeaconHostObject) 열기. 수락·거절·강퇴·입장 지시를 넘긴다
//   ② 지원자·파티원: 목록 검색(LAN 세션) → 신청 = Beacon 연결(ALOAPartyBeaconClient) → 받은 파티 정보·알림 보관
//   ③ 입장: 파티장은 신호를 보내고 레이드 맵을 ?listen으로 연다 / 파티원은 잠시 뒤 그 주소로 접속 (실패하면 다시 시도)
//   UI(ULOAPartyFinderWidget, apply132)는 이 클래스만 안다 → 파티 정보가 Beacon에서 오는지 백엔드에서 오는지 모른다
//   어느 레이드인가 = 레이드 데이터 ID(RaidId). 방 만들기는 Project Settings → LOA Lobby → Raid Data, 입장은 파티의 RaidId
//
// [왜 둘로 나눴나] (UE 5.5 소스 확인)
//   목록 = LAN 세션 (OnlineSubsystemNull FindLANSession: UDP 방송으로 방을 찾음). Beacon은 방을 광고하지 않는다
//   신청~입장 = Beacon (OnlineBeaconHost: 게임과 다른 포트로 듣고, 접속 없이 RPC). 방 광고에 Beacon 포트를 싣는다(SETTING_BEACONPORT)
//   세션 접속(JoinSession)은 쓰지 않는다 → 세션의 빈자리 수가 줄지 않아 "다 차도 목록에 보임"이 된다
//     (Null은 빈자리 > 0인 방만 응답: IsSessionJoinable. 실제 인원은 따로 LOA_MEMBERS 칸에 실어 보낸다)
//
// [왜 세션 기능을 둘 쓰나] (11-21 사용자: 로아는 파티장이 다른 글을 봐도 내 글이 안 사라진다)
//   Null은 세션 기능 하나에 LAN 통로(FLANSession)가 하나 → "광고 중"과 "검색 중"을 동시에 못 한다
//     (LANBeacon.cpp Search가 먼저 StopLANSession → 검색하는 약 5초 동안 남의 물음에 답을 못 함)
//   → 광고 = 이 창의 기본 Null (PIE면 Context_1) / 검색 = 같은 Null을 이름만 다르게 하나 더 (Context_1_PartySearch)
//     엔진이 이름마다 따로 만든다 (OnlineSubsystemModule.cpp GetOnlineSubsystem). 소켓은 같은 포트를 같이 쓴다 (LANBeacon.cpp SetReuseAddr)
//   검색에 내 광고도 답한다 → 내 파티도 목록에 나오고, 맨 위에 둔다 (11-21 사용자)
//
// [왜 GameInstance 서브시스템]
//   기억할 것이 있다(파티 정보, 입장 주소). 맵이 바뀌어도 남아야 한다(접속 실패 → 로비 다시 열림 → 다시 시도)
//   Beacon 액터는 월드에 있어서 맵을 옮기면 사라진다 → 입장 순서를 여기서 정한다
//
// [흐름] 예) 지혜리(파티장) · 인생운(지원자)
//   지혜리 CreateParty("2수 …") → Beacon 호스트(15000) + 세션 광고 → Role Leader
//   인생운 FindParties → OnPartiesFound(목록) → ApplyToParty(0) → Beacon 연결 → ServerApply → Role Applicant
//   지혜리 화면 OnPartyNotice(Applied) → AcceptApplicant(1) → 인생운 Role Member + 파티 정보 받음
//   지혜리 EnterRaid → 인생운에게 "192.168.0.5:7777" → Beacon·세션 닫기 → 레이드 데이터의 RaidMap?listen
//   인생운 EnterConnectDelay(2초) 뒤 open 192.168.0.5:7777 → 실패면 2초마다 최대 5번 → 레이드 맵 도착 = 파티 끝 (흩어짐, 11-19)
//
// [네트워크] 세 규칙
//   1 파티장만 : 판단(정원·수락·강퇴·입장)은 ALOAPartyBeaconHostObject. 여기는 지시를 넘길 뿐
//   2 전달     : 신청·나가기(지원자 → 파티장), 파티 정보 통째로·알림·입장 주소(파티장 → 파티원). Beacon RPC
//   3 각자     : 목록 검색, 알림 글 만들기(UI), 입장 다시 시도
//
// [하지 않는 일]
//   레이드가 끝난 뒤 파티 유지 (첫 판은 흩어짐, 11-19). 채팅·초대·목록 갱신·모집 설정 (나중)
//   데디 서버 (나중: 입장 주소를 파티장 IP 대신 설정의 데디 주소로 보내면 된다)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Engine/EngineBaseTypes.h"
#include "Party/LOAPartyTypes.h"
#include "LOAPartySubsystem.generated.h"

class AOnlineBeaconHost;
class ALOAPartyBeaconHostObject;
class ALOAPartyBeaconClient;
class FOnlineSessionSearch;
class UNetDriver;
class UWorld;
namespace ENetworkFailure { enum Type : int; }

DECLARE_MULTICAST_DELEGATE(FLOAPartyStateChangedSignature);
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAPartiesFoundSignature, const TArray<FLOAPartyListEntry>& /*Listings*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FLOAPartyNoticeSignature, const FLOAPartyNotice& /*Notice*/);

UCLASS()
class LOA_API ULOAPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ─── 입구 (UI·치트) ───

	/** 방 만들기 (파티장이 된다). 레이드 = Project Settings → LOA Lobby → Raid Data. 이름을 안 정했거나 이미 파티에 있으면 false */
	bool CreateParty(const FString& Title);

	/**
	 * 목록 검색 시작 (검색 전용 세션 기능으로). 끝나면 OnPartiesFound. 이미 검색 중이면 false
	 * 파티 중에도 된다 (로아도 파티장이 다른 모집글을 봄, 11-21 사용자). 내 파티는 맨 위 (bMyParty)
	 */
	bool FindParties();

	/** 목록의 ListingIndex번 방에 신청 (Beacon 연결 → 신청). 결과는 OnPartyNotice(Rejected) 또는 OnPartyChanged */
	bool ApplyToParty(int32 ListingIndex);

	/** 파티장: 신청자 수락 / 거절 / 파티원 강퇴 (MemberId = 파티장 컴퓨터가 정한 번호) */
	bool AcceptApplicant(int32 MemberId);
	bool RejectApplicant(int32 MemberId);
	bool KickMember(int32 MemberId);

	/** 나가기. 파티장이면 파티를 닫는다 (모두에게 Disbanded) */
	void LeaveParty();

	/** 파티장: 입장하기. 3명이 다 안 차도 된다 (11-19 사용자) */
	bool EnterRaid();

	// ─── 읽기 ───
	ELOAPartyRole GetRole() const { return Role; }
	const FLOAPartyInfo& GetPartyInfo() const { return PartyInfo; }
	const TArray<FLOAPartyListEntry>& GetListings() const { return Listings; }
	bool IsSearching() const { return SessionSearch.IsValid(); }
	bool IsEnteringRaid() const { return !PendingRaidAddress.IsEmpty(); }

	// ─── 알림 (멀티캐스트. 듣는 곳: ULOAPartyFinderWidget(apply132 파티 찾기 창). 치트는 듣지 않고 로그로 본다) ───
	/** 파티 정보·내 역할이 바뀜 */
	FLOAPartyStateChangedSignature OnPartyChanged;
	/** 목록 검색 끝 */
	FLOAPartiesFoundSignature OnPartiesFound;
	/** 시스템 알림 한 줄 */
	FLOAPartyNoticeSignature OnPartyNotice;

private:
	// ─── 파티장 ───
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleHostPartyChanged();
	void HandleHostNotice(const FLOAPartyNotice& Notice);
	/** 광고의 인원 칸 고치기 (목록에 "2/3") */
	void UpdateAdvertisedMembers();
	/** 레이드 게임 서버 주소 = 방 광고의 IP(지원자가 찾아온 주소) + 게임 포트. 예) 192.168.0.5:7777 */
	bool MakeRaidAddress(FString& OutAddress) const;
	/** Beacon 호스트·세션 닫기 */
	void CloseHost();

	// ─── 검색 ───
	void HandleFindSessionsComplete(bool bWasSuccessful);

	// ─── 지원자·파티원 (ALOAPartyBeaconClient 알림) ───
	void HandleBeaconConnected();
	void HandleApplyResult(bool bAccepted);
	void HandlePartyInfoReceived(const FLOAPartyInfo& Info);
	void HandleNoticeReceived(const FLOAPartyNotice& Notice);
	void HandleRemoved(ELOAPartyNoticeType Reason);
	void HandleEnterRaid(const FString& Address);
	void HandleBeaconClosed();
	/** 연결 알림을 끊고, 다음 프레임에 연결을 없앤다 (지금 그 연결의 RPC 안일 수 있음) */
	void DetachClientBeacon();
	void DestroyDetachedClientBeacon();

	// ─── 입장 (파티원) ───
	void ConnectToRaid();
	void ScheduleRaidRetry();
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	// ─── 공통 ───
	/** 파티 없음으로 되돌린다 (연결·세션 정리, 역할 None) */
	void ResetParty();
	void SetRole(ELOAPartyRole NewRole);
	void NotifySelf(ELOAPartyNoticeType Type);
	/** 내 정보 (이름·직업). 이름이 비면 false */
	bool MakeMyMemberInfo(FLOAPartyMember& OutMember) const;
	/** 광고용 = 이 창의 기본 세션 기능 (방 만들기·닫기·입장 주소) */
	IOnlineSessionPtr GetSessionInterface() const;
	/** 검색용 = 이름만 다른 Null 하나 더. 처음 부를 때 엔진이 만든다 */
	IOnlineSessionPtr GetSearchSessionInterface();
	/** 검색용 세션 기능을 지운다 (서브시스템이 꺼질 때) */
	void DestroySearchSubsystem();

	ELOAPartyRole Role = ELOAPartyRole::None;

	/** 지금 파티 (파티장 = 원본 사본, 파티원 = 받은 사본) */
	FLOAPartyInfo PartyInfo;

	/** 내 파티의 PartyCode (파티장 = 만들 때 정함, 지원자 = 신청한 줄의 것). 역할이 None이 되면 비운다 */
	FString MyPartyCode;

	/**
	 * 검색용 세션 기능 이름. 예) "NULL:Context_1_PartySearch" (패키징 게임은 "NULL:DefaultInstance_PartySearch")
	 * 처음 만들 때 기억한다 → 끌 때(PIE 끝) 월드가 없어도 지울 수 있게
	 */
	FName SearchSubsystemName;

	/** 마지막 검색 결과 */
	TArray<FLOAPartyListEntry> Listings;

	/**
	 * 지금 하는 검색. 엔진이 검색하는 동안 결과(SearchResults)를 여기에 채운다 → 끝날 때까지 들고 있어야 한다.
	 * 끝나면(HandleFindSessionsComplete) 비운다 → "검색 중인가" 표시도 겸한다 (IsSearching)
	 */
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;

	/** 파티장: 포트를 듣는 엔진 액터 + 판단 객체 */
	TWeakObjectPtr<AOnlineBeaconHost> BeaconHost;
	TWeakObjectPtr<ALOAPartyBeaconHostObject> HostObject;

	/** 지원자·파티원: 파티장과의 연결 */
	TWeakObjectPtr<ALOAPartyBeaconClient> ClientBeacon;
	/** 알림을 끊고 없애기를 기다리는 연결 */
	TWeakObjectPtr<ALOAPartyBeaconClient> DetachedClientBeacon;

	/** 파티원: 입장할 주소 (비어 있으면 입장 중 아님) */
	FString PendingRaidAddress;
	int32 RaidConnectAttempts = 0;
	FTimerHandle RaidConnectTimer;
};
