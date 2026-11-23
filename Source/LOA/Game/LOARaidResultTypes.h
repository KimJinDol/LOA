// LOARaidResultTypes.h
// [값] 레이드 클리어 결과. 서버가 클리어 순간 한 번 만들고(ULOARaidRecordSubsystem::BuildClearResult) GameState가 복제, 결과 화면이 읽는다. (2026-11-15 apply115)
//
// [칸] 모두 "파티 안에서의 내 몫" 0~1 (사용자 결정 2026-11-15)
//   준 피해     = 내가 보스에게 깎은 양           ÷ 파티 전체가 깎은 양
//   카운터 성공 = 내 카운터 성공 횟수              ÷ 파티 전체 카운터 성공 횟수
//   조력        = 내 버프 때문에 더 들어간 피해    ÷ 파티 전체가 깎은 양
//   파티 방어   = 내 보호막이 막은 양              ÷ 파티 전원이 맞은 양 (보호막에 막힌 것 + 체력이 깎인 것)
//   파티 회복   = 내가 실제로 채운 양              ÷ 파티 전체 회복 합 (나중에 회복 아이템이 들어오면 분모가 커짐)
//   예) 버서커A 6만 / 버서커B 4만 깎음 → A 준 피해 0.6, B 0.4
//
// [MVP] 다섯 칸 몫을 더한 점수가 가장 큰 사람. 같으면 준 피해가 큰 사람 (대신 정함)
//   예) 세라스 피해 0.05 + 조력 1.0 + 방어 1.0 + 회복 1.0 = 3.05 / 버서커A 피해 0.6 + 카운터 0.5 = 1.1 → 세라스
//
// [네트워크] 값만 담은 구조체. GameState의 복제 칸(ClearResult)에 실려 클리어 단계와 같이 도착한다.

#pragma once

#include "CoreMinimal.h"
#include "LOARaidResultTypes.generated.h"

class ULOACharacterClassData;
class UTexture2D;

/** 결과 화면의 칸. 화면 글자는 DisplayName (결과 카드가 그대로 쓴다) */
UENUM(BlueprintType)
enum class ELOARaidStat : uint8
{
	Damage	UMETA(DisplayName = "준 피해"),
	Counter	UMETA(DisplayName = "카운터 성공"),
	Assist	UMETA(DisplayName = "조력"),
	Protect	UMETA(DisplayName = "파티 방어"),
	Heal	UMETA(DisplayName = "파티 회복"),

	Count	UMETA(Hidden),
};

/** 한 사람의 결과 */
USTRUCT(BlueprintType)
struct LOA_API FLOARaidPlayerResult
{
	GENERATED_BODY()

	/** 플레이어 이름 (PlayerState 이름) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	FString PlayerName;

	/** 직업 (아이콘·이름). 몸이 없었으면 비어 있음 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	TObjectPtr<const ULOACharacterClassData> ClassData;

	/** 칸별 몫 0~1. 순서 = ELOARaidStat (길이 = Count) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	TArray<float> Shares;

	/** MVP 점수 = Shares의 합 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	float Score = 0.f;

	float GetShare(ELOARaidStat Stat) const
	{
		const int32 Index = static_cast<int32>(Stat);
		return Shares.IsValidIndex(Index) ? Shares[Index] : 0.f;
	}

	/**
	 * 몫이 큰 칸부터 MaxCount개. 0인 칸은 뺀다 (결과 카드의 줄, 대신 정함)
	 *   예) 피해 0.6, 카운터 0.5, 나머지 0 → [피해, 카운터]
	 */
	TArray<ELOARaidStat> GetTopStats(int32 MaxCount) const;
};

/** 클리어 결과 전체 */
USTRUCT(BlueprintType)
struct LOA_API FLOARaidClearResult
{
	GENERATED_BODY()

	/** 사람별 결과 (PlayerArray 순서) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	TArray<FLOARaidPlayerResult> Players;

	/** MVP가 Players의 몇 번째인가. 사람이 없으면 INDEX_NONE */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	int32 MvpIndex = INDEX_NONE;

	/** 걸린 시간 (초) = 마지막 전투 시작 → 보스 쓰러짐 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	float ClearSeconds = 0.f;

	/** 쓰러뜨린 보스 이름 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	FText BossName;

	/** 배너 가운데 보스 얼굴 (보스의 ClearPortrait). 비어 있으면 결과 화면이 숨긴다 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Raid")
	TSoftObjectPtr<UTexture2D> BossPortrait;

	bool IsValid() const { return Players.Num() > 0; }
};
