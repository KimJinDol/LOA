// LOAPartyTypes.h
// [데이터] 파티 찾기에서 주고받는 것: 멤버 한 명 / 파티 전체 / 목록 한 줄 / 알림. (2026-11-19 apply131, 11-20 apply132 RaidId · 목록 한 줄 이름)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   파티장 컴퓨터(Beacon 호스트)와 지원자·파티원 컴퓨터가 Beacon RPC로 주고받는 값 묶음.
//   목록 한 줄(FLOAPartyListEntry)만 LAN 세션 검색 결과에서 만든다 (Beacon 연결 전).
//
// [누가 무엇을 아나]
//   파티장   : FLOAPartyInfo 원본 (Members + Applicants)
//   파티원   : 파티장이 보낸 사본 (Applicants는 비어 있음. 신청자 탭은 파티장만, 11-19 사용자)
//   지원자   : 아무것도 안 받는다. 수락·거절 결과만 (ClientApplyResult)
//
// [MemberId]
//   파티장 컴퓨터가 정하는 번호. 파티장 = 0, 신청이 들어올 때마다 1, 2, 3 …  (같은 파티 안에서만 뜻이 있다)
//   수락·거절·강퇴를 "몇 번"으로 고른다. 이름은 겹칠 수 있어 쓰지 않는다.
//
// [RaidId]
//   어느 레이드 모집인가 = 레이드 데이터의 에셋 매니저 ID (예: Raid:DA_Raid_ReviveBeastHeart).
//   이름·맵은 보내지 않는다. 받는 컴퓨터가 ULOARaidData::LoadById로 자기 에셋을 찾는다
//
// [네트워크] 2 복제·전달 (Beacon RPC 인자). 레이드 맵으로 넘어가지 않는다 (레이드는 로그인 주소로 이름·직업을 따로 보냄)

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "LOAPartyTypes.generated.h"

/** 이 컴퓨터가 파티에서 무엇인가 */
UENUM(BlueprintType)
enum class ELOAPartyRole : uint8
{
	None,		// 파티 없음
	Leader,		// 방을 만든 사람 (Beacon 호스트)
	Applicant,	// 신청하고 기다리는 중 (Beacon 연결됨, 아직 파티원 아님)
	Member,		// 수락된 파티원
};

/** 시스템 알림 종류. 글은 받는 컴퓨터가 만든다 (3 각자) */
UENUM(BlueprintType)
enum class ELOAPartyNoticeType : uint8
{
	Applied,		// 신청이 들어옴 (파티장만 받음)
	Joined,			// 파티에 참여함
	Left,			// 파티에서 나감
	Kicked,			// 강퇴됨
	Rejected,		// 신청이 거절됨 (지원자 본인만)
	Disbanded,		// 파티장이 파티를 닫음
	ConnectionLost,	// 파티장과 연결이 끊김
	EnterFailed,	// 레이드 입장 실패 (다시 시도를 다 씀)
};

/** 멤버 한 명 (파티원·지원자 공통) */
USTRUCT(BlueprintType)
struct LOA_API FLOAPartyMember
{
	GENERATED_BODY()

	/** 파티장 컴퓨터가 정한 번호. 파티장 = 0. 신청 보낼 때는 비워 둔다 (INDEX_NONE) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	int32 MemberId = INDEX_NONE;

	/** 로비에서 정한 이름 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FString PlayerName;

	/** 고른 직업 (에셋 매니저 ID). 직업 아이콘을 그릴 때 쓴다 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FPrimaryAssetId ClassId;

	/** 파티장인가 (사진의 왕관) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	bool bLeader = false;
};

/** 파티 전체 */
USTRUCT(BlueprintType)
struct LOA_API FLOAPartyInfo
{
	GENERATED_BODY()

	/** 어느 레이드 모집인가 (레이드 데이터 ID). 상세 창 제목의 레이드 이름 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FPrimaryAssetId RaidId;

	/** 파티장이 쓴 한 줄 글 (사진의 "2수 1부식신호 …") */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FString Title;

	/** 최대 인원 (레이드 데이터의 MaxPartyMembers) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	int32 MaxMembers = 0;

	/** 파티원 (파티장 포함, 들어온 순서) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	TArray<FLOAPartyMember> Members;

	/** 신청자. 파티장 컴퓨터에만 있다 (파티원에게 보낼 때는 비운다) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	TArray<FLOAPartyMember> Applicants;

	/** 다 찼나 */
	bool IsFull() const { return Members.Num() >= MaxMembers; }

	/** 번호로 찾기. 없으면 nullptr */
	const FLOAPartyMember* FindMember(int32 MemberId) const;
	const FLOAPartyMember* FindApplicant(int32 MemberId) const;
};

/** 파티 목록 한 줄 (LAN 세션 검색 결과에서 만든다) */
USTRUCT(BlueprintType)
struct LOA_API FLOAPartyListEntry
{
	GENERATED_BODY()

	/** 어느 레이드 모집인가 (목록의 "목표" 칸) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FPrimaryAssetId RaidId;

	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FString LeaderName;

	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	int32 NumMembers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	int32 MaxMembers = 0;

	/** 신청할 때 Beacon이 붙을 주소 (파티장 IP:Beacon 포트). 예) 192.168.0.5:15000 */
	UPROPERTY()
	FString BeaconAddress;

	/**
	 * 파티마다 하나인 글자 (파티장이 방을 만들 때 FGuid로 정해 광고에 싣는다). 내 파티 찾기용
	 * 이름으로 찾지 않는 이유: 파티장 이름은 겹칠 수 있다
	 */
	UPROPERTY()
	FString PartyCode;

	/** 이 컴퓨터가 든 파티인가 (파티장·파티원·신청한 파티). 목록 맨 위에 온다 (11-21 사용자) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	bool bMyParty = false;
};

/** 시스템 알림 한 줄. 예) { Joined, "인생운" } → "인생운님이 모집에 참여하였습니다." (글은 UI가 만든다) */
USTRUCT(BlueprintType)
struct LOA_API FLOAPartyNotice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	ELOAPartyNoticeType Type = ELOAPartyNoticeType::Joined;

	/** 누가. 내 일이면(거절·연결 끊김 등) 빈 글자 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Party")
	FString PlayerName;
};
