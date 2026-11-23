// LOAPartyBeaconHostObject.h
// [판단] 파티장 컴퓨터의 파티 판단. 파티 정보 원본, 정원 검사, 수락·거절·강퇴, 입장 신호. (2026-11-19 apply131, 11-20 apply132 단일 알림)
//
// ─────────────────────────────────────────────────────────────────────────────
// [엔진 Beacon 구조에서 이 클래스의 자리]
//   AOnlineBeaconHost (엔진 그대로)    : 포트를 듣고 손 인사를 한다. "어느 종류 Beacon인가"로 이 객체에 넘긴다
//   ALOAPartyBeaconHostObject (이것)   : 붙은 사람마다 ALOAPartyBeaconClient 짝을 만들고(엔진 기본), 파티를 판단한다
//   ALOAPartyBeaconClient              : 통로. 서버 RPC를 받으면 GetBeaconOwner() = 이 객체에 넘긴다
//
// [규칙]
//   판단은 여기 한 곳 (1 파티장만): 정원, 신청 검사, 수락·거절·강퇴, 나가기, 입장
//   현업이면 백엔드 파티 서버가 하는 일. 이 프로젝트는 백엔드가 없는 LAN이라 파티장 컴퓨터가 맡는다 (11-19 상의)
//   이 객체는 누가 듣는지 모른다 → OnPartyChanged / OnNotice 알림만 (ULOAPartySubsystem이 듣는다)
//   알림은 단일 델리게이트 (작업 규칙: 아래 객체 → 주인 하나 = 단일, 서브시스템 → 여러 화면 = 멀티캐스트, 11-20)
//     주인은 이 객체를 만든 ULOAPartySubsystem 하나뿐. 화면들은 서브시스템의 멀티캐스트를 듣는다
//
// [흐름] 예) 파티장 지혜리(0), 인생운이 신청
//   인생운 연결 → (엔진) 짝 액터 생성 → ServerApply{인생운, 버서커} → HandleApply
//     이름 검사·정원 검사 → Applicants에 {1, 인생운} → OnNotice(Applied) + OnPartyChanged (파티장 화면의 신청자 탭)
//   파티장 Accept(1) → 정원 검사 → Members로 옮김 → 인생운에게 ClientApplyResult(true)
//     → 파티원 모두에게 ClientPartyInfo(신청자 뺀 사본) + ClientNotice(Joined) / 파티장 화면에도 같은 알림
//   파티장 Reject(1) → ClientApplyResult(false) → 연결 닫기 (엔진 DisconnectClient가 보낼 것을 먼저 다 보낸 뒤 닫음)
//   파티장 Kick(1)   → ClientRemoved(Kicked) → 연결 닫기 → 남은 사람에게 정보·알림
//   파티장 SendEnterAndClose(주소) → 파티원에게 ClientEnterRaid(주소), 신청자에게 거절 → 모두 연결 닫기
//
// [끊김]
//   상대가 나가기(ServerLeave)·창 닫기·시간 초과 → 엔진 NotifyClientDisconnected → 목록에서 빼고 알림 (나가기와 같은 처리)
//
// [네트워크] 1 파티장만 (파티장 컴퓨터에만 있다). 지원자 컴퓨터에는 없다

#pragma once

#include "CoreMinimal.h"
#include "OnlineBeaconHostObject.h"
#include "Party/LOAPartyTypes.h"
#include "LOAPartyBeaconHostObject.generated.h"

class ALOAPartyBeaconClient;

DECLARE_DELEGATE(FLOAHostPartyChangedSignature);
DECLARE_DELEGATE_OneParam(FLOAHostPartyNoticeSignature, const FLOAPartyNotice& /*Notice*/);

UCLASS(Transient, NotPlaceable)
class LOA_API ALOAPartyBeaconHostObject : public AOnlineBeaconHostObject
{
	GENERATED_BODY()

public:
	ALOAPartyBeaconHostObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 파티를 연다. Leader = 이 컴퓨터의 사람 (MemberId 0이 된다). RaidId·MaxMembers = 레이드 데이터에서 */
	void InitParty(const FLOAPartyMember& Leader, const FPrimaryAssetId& RaidId, const FString& Title, int32 MaxMembers);

	/** 파티 정보 원본 (신청자 포함) */
	const FLOAPartyInfo& GetPartyInfo() const { return PartyInfo; }

	// ─── 파티장의 지시 (이 컴퓨터의 UI·치트 → ULOAPartySubsystem → 여기) ───

	/** 수락. 다 찼거나 없는 번호면 false */
	bool Accept(int32 MemberId);

	/** 거절. 없는 번호면 false */
	bool Reject(int32 MemberId);

	/** 강퇴. 파티장 자신(0)이나 없는 번호면 false */
	bool Kick(int32 MemberId);

	/** 입장: 파티원에게 주소를 보내고, 신청자는 거절하고, 모두 연결을 닫는다. 그 뒤 파티장은 레이드 맵으로 */
	void SendEnterAndClose(const FString& Address);

	/** 파티 닫기 (파티장이 나감): 모두에게 Disbanded → 연결 닫기 */
	void Disband();

	// ─── 통로(ALOAPartyBeaconClient의 서버 RPC)에서 ───
	void HandleApply(ALOAPartyBeaconClient* Client, const FLOAPartyMember& Request);
	void HandleLeave(ALOAPartyBeaconClient* Client);

	// ─── 알림 (주인 ULOAPartySubsystem 하나가 듣는다. 단일) ───
	/** 파티 정보가 바뀜 (멤버·신청자) */
	FLOAHostPartyChangedSignature OnPartyChanged;
	/** 파티장 화면에 띄울 알림 */
	FLOAHostPartyNoticeSignature OnNotice;

	//~ Begin AOnlineBeaconHostObject Interface
	/** 엔진: 연결이 끊김 (나가기·창 닫기·시간 초과·우리가 닫음) */
	virtual void NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor) override;
	//~ End AOnlineBeaconHostObject Interface

private:
	/** 목록(Members 또는 Applicants)에서 빼고 연결 표에서도 뺀다. 빠진 사람이 파티원이었으면 true + 이름 */
	bool RemoveEntry(int32 MemberId, FString& OutRemovedMemberName);

	/** 이 연결의 번호. 없으면 INDEX_NONE */
	int32 FindMemberIdByClient(const AOnlineBeaconClient* Client) const;

	/** 이 번호의 연결. 없으면 nullptr */
	ALOAPartyBeaconClient* FindClient(int32 MemberId) const;

	/** 파티원(파티장 제외) 모두에게 신청자를 뺀 사본 + 파티장 화면에 알림 */
	void BroadcastPartyInfo();

	/** 파티원(파티장 제외) 모두 + 파티장 화면에 알림 */
	void BroadcastNotice(ELOAPartyNoticeType Type, const FString& PlayerName);

	/** 이름 검사 (빈 이름, 규칙 위반). 클라 화면 검사만 믿지 않는다 (서버 쪽에서도) */
	bool IsValidApplyRequest(const FLOAPartyMember& Request) const;

	/** 파티 정보 원본 */
	FLOAPartyInfo PartyInfo;

	/** 다음 신청자에게 줄 번호 (파티장 = 0) */
	int32 NextMemberId = 1;

	/** 번호 → 연결 (신청자·파티원). 파티장은 연결이 없다 */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<ALOAPartyBeaconClient>> ClientsByMemberId;
};
