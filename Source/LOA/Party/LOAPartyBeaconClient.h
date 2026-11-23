// LOAPartyBeaconClient.h
// [통로] 파티 Beacon 연결 하나. 지원자·파티원 컴퓨터와 파티장 컴퓨터가 RPC를 주고받는 곳. (2026-11-19 apply131)
//
// ─────────────────────────────────────────────────────────────────────────────
// [Beacon이란] (엔진 OnlineSubsystemUtils)
//   게임에 접속하지 않고 두 컴퓨터를 잇는 가벼운 연결. 맵 로딩·폰·PlayerState가 없다.
//   파티장 컴퓨터의 AOnlineBeaconHost가 따로 연 포트(기본 15000)로 듣고, 지원자가 이 액터로 붙는다.
//   붙으면 파티장 쪽에도 같은 클래스의 짝이 생긴다 (AOnlineBeaconHostObject::SpawnBeaconActor) → 둘 사이에 RPC.
//
// [짝 두 개] 연결 하나에 이 클래스 액터가 양쪽 컴퓨터에 하나씩 있다 (11-20 apply132 주석)
//   | 어디              | 누가 만드나                                   | 주인(GetBeaconOwner) | 여기서 실행되는 것           |
//   |-------------------|-----------------------------------------------|----------------------|------------------------------|
//   | 지원자 컴퓨터     | ULOAPartySubsystem::ApplyToParty (SpawnActor) | 없음                 | 클라 RPC → 델리게이트 알림   |
//   | 파티장 컴퓨터     | 엔진 (SpawnBeaconActor → SetBeaconOwner)      | HostObject           | 서버 RPC → HostObject에 넘김 |
//   예) 인생운이 신청: 인생운 컴퓨터의 짝이 ServerApply를 부름 → 지혜리 컴퓨터의 짝에서 실행 → GetBeaconOwner() = HostObject
//
// [규칙] 통로만. 판단하지 않는다 (ULOACombatRPCComponent와 같은 규칙)
//   서버 RPC(파티장 쪽에서 실행) → GetBeaconOwner() = ALOAPartyBeaconHostObject에 넘김 (판단은 거기)
//   클라 RPC(지원자 쪽에서 실행) → 델리게이트로 알림. 누가 듣는지 모른다 (ULOAPartySubsystem이 듣는다)
//
// [하지 않는 일]
//   파티 정보를 들고 있지 않는다. 연결이 끊기면 이 액터는 사라진다 (엔진이 Destroy).
//
// [흐름] 지원자 컴퓨터 기준
//   InitClient(파티장 IP:15000) → 엔진 손 인사 → OnConnected → OnConnectedToHost 알림 → (서브시스템) ServerApply
//   파티장이 수락 → ClientApplyResult(true) → ClientPartyInfo …  /  거절 → ClientApplyResult(false) → 연결 끊김
//   입장하기 → ClientEnterRaid(주소) → 파티장이 연결을 닫음 → 이 액터 사라짐 → (서브시스템) 잠시 뒤 open 주소
//   끊김(강퇴·파티 닫힘·시간 초과) → OnFailure → OnConnectionClosed 알림
//
// [네트워크] 2 복제·전달. Reliable만 쓴다 (신청·수락은 한 번이라도 빠지면 안 됨, 양이 적음)

#pragma once

#include "CoreMinimal.h"
#include "OnlineBeaconClient.h"
#include "Party/LOAPartyTypes.h"
#include "LOAPartyBeaconClient.generated.h"

DECLARE_DELEGATE(FLOAPartyBeaconEventSignature);
DECLARE_DELEGATE_OneParam(FLOAPartyApplyResultSignature, bool /*bAccepted*/);
DECLARE_DELEGATE_OneParam(FLOAPartyInfoSignature, const FLOAPartyInfo& /*Info*/);
DECLARE_DELEGATE_OneParam(FLOAPartyNoticeReceivedSignature, const FLOAPartyNotice& /*Notice*/);
DECLARE_DELEGATE_OneParam(FLOAPartyRemovedSignature, ELOAPartyNoticeType /*Reason*/);
DECLARE_DELEGATE_OneParam(FLOAPartyEnterRaidSignature, const FString& /*Address*/);

UCLASS(Transient, NotPlaceable)
class LOA_API ALOAPartyBeaconClient : public AOnlineBeaconClient
{
	GENERATED_BODY()

public:
	// ─── 서버 RPC: 지원자·파티원 → 파티장 (파티장 컴퓨터에서 실행) ───

	/** 신청. MemberId는 비워 보낸다 (파티장이 정함) */
	UFUNCTION(Server, Reliable)
	void ServerApply(const FLOAPartyMember& Request);

	/** 스스로 나가기 (지원 취소 포함) */
	UFUNCTION(Server, Reliable)
	void ServerLeave();

	// ─── 클라 RPC: 파티장 → 지원자·파티원 (지원자 컴퓨터에서 실행) ───

	/** 신청 결과. 거절이면 파티장이 곧 연결을 닫는다 */
	UFUNCTION(Client, Reliable)
	void ClientApplyResult(bool bAccepted);

	/** 바뀐 파티 정보 통째로 (Applicants는 비어 있음) */
	UFUNCTION(Client, Reliable)
	void ClientPartyInfo(const FLOAPartyInfo& Info);

	/** 시스템 알림 */
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FLOAPartyNotice& Notice);

	/** 파티에서 빠지게 됨 (강퇴 / 파티 닫힘). 파티장이 곧 연결을 닫는다 */
	UFUNCTION(Client, Reliable)
	void ClientRemoved(ELOAPartyNoticeType Reason);

	/** 입장 신호. Address = 레이드 게임 서버 주소 (지금은 파티장 IP:7777) */
	UFUNCTION(Client, Reliable)
	void ClientEnterRaid(const FString& Address);

	// ─── 알림 (지원자 컴퓨터. ULOAPartySubsystem이 듣는다) ───
	FLOAPartyBeaconEventSignature OnConnectedToHost;
	FLOAPartyBeaconEventSignature OnConnectionClosed;
	FLOAPartyApplyResultSignature OnApplyResult;
	FLOAPartyInfoSignature OnPartyInfo;
	FLOAPartyNoticeReceivedSignature OnNotice;
	FLOAPartyRemovedSignature OnRemoved;
	FLOAPartyEnterRaidSignature OnEnterRaid;

protected:
	//~ Begin AOnlineBeaconClient Interface
	/** 엔진: 파티장이 첫 RPC(ClientOnConnected)를 보냄 = RPC를 써도 됨 */
	virtual void OnConnected() override;
	/** 엔진: 연결 실패·끊김 (시간 초과, 파티장이 닫음 등) */
	virtual void OnFailure() override;
	//~ End AOnlineBeaconClient Interface

	//~ Begin AActor Interface
	/** 연결이 닫히면 엔진이 이 액터를 없앤다 (OnNetCleanup → Destroy). OnFailure가 안 오고 닫혀도 알리기 위해 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

private:
	/** 끊김 알림을 한 번만 (OnFailure와 EndPlay가 둘 다 올 수 있다) */
	void NotifyConnectionClosed();

	bool bClosedNotified = false;
};
