// LOARaidData.h
// [데이터] 레이드 하나의 정보: 이름, 맵, 파티 인원. 파티 찾기 목록의 "목표" 칸과 입장하기가 읽는다. (2026-11-20 apply132)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 데이터 에셋]
//   이름·맵·인원은 레이드마다 다르다 (로아도 4인·8인 레이드가 따로 있다) → 레이드 하나 = 에셋 하나.
//   레이드가 늘면 에셋만 하나 더 만든다. 직업 데이터(ULOACharacterClassData)와 같은 방식 (에셋 매니저 종류 "Raid").
//
// [누가 읽나]
//   방 만들기 : ULOAPartySubsystem::CreateParty → Project Settings → LOA Lobby → Raid Data (고르는 화면이 생기기 전까지 하나)
//               → 파티 정보·방 광고에 RaidId를 싣는다 (예: Raid:DA_Raid_ReviveBeastHeart)
//   목록·상세 : ULOAPartyFinderWidget → LoadById(RaidId) → DisplayName ("부활한 마수의 심장")
//   입장하기 : ULOAPartySubsystem::EnterRaid → LoadById(파티의 RaidId) → RaidMap을 ?listen으로
//
// [네트워크] 각자. 컴퓨터끼리는 RaidId(에셋 매니저 ID)만 주고받는다 → 받는 쪽이 자기 에셋을 찾는다
//
// [만들기] 에디터 우클릭 → 기타 → 데이터 에셋 → LOARaidData. /Game/LOA 아래 아무 곳 (DefaultGame.ini Raid 등록이 /Game/LOA 전체)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LOARaidData.generated.h"

class UWorld;

UCLASS(BlueprintType, Const)
class LOA_API ULOARaidData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 에셋 매니저 ID = (Raid, 에셋 이름). 예: Raid:DA_Raid_ReviveBeastHeart */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/**
	 * 에셋 매니저 종류 이름 "Raid". 한 곳에서 정한다.
	 * DefaultGame.ini의 Primary Asset Types to Scan 이름과 같아야 한다
	 */
	static FPrimaryAssetType GetPrimaryAssetTypeName();

	/**
	 * ID로 레이드 데이터를 찾는다. 없거나 Raid 종류가 아니면 nullptr.
	 * 메모리에 없으면 바로 불러온다 (작은 에셋. 로비 화면·입장하기에서만 부름)
	 * 예) Raid:DA_Raid_ReviveBeastHeart → DA_Raid_ReviveBeastHeart
	 */
	static const ULOARaidData* LoadById(const FPrimaryAssetId& RaidId);

#if WITH_EDITOR
	/** 빈 이름·빈 맵을 저장할 때 잡는다 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** 화면에 보이는 레이드 이름 (파티 찾기 "목표" 칸, 상세 제목) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Raid")
	FText DisplayName;

	/** 입장하기로 여는 레이드 맵 (파티장이 ?listen으로 연다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Raid")
	TSoftObjectPtr<UWorld> RaidMap;

	/** 파티 최대 인원 (파티장 포함). 파티장 컴퓨터의 정원 검사와 목록의 "1 / 3"이 같이 쓴다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Raid", meta = (ClampMin = "1"))
	int32 MaxPartyMembers = 3;
};
