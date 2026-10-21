// LOAGE_CrowdControl.h
// [배관] 플레이어가 받는 CC GE의 공용 부모. 밀거나 날리는 "방향 규칙"과 "거리 기본값"만 가진다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 있나] (2026-10-21) 띄움(ULOAGE_Airborne)과 경직(ULOAGE_Paralysis)이 같은 방향 규칙을 쓴다.
//   전에는 ULOAGE_Airborne이 방향 칸을 갖고, ULOAGA_Airborne이 방향을 계산했다 → 경직이 같은 걸 또 만들지 않게 여기로 모았다.
//   칸 이름(Launch Direction, Yaw Offset, Default Distance)은 그대로 → 기존 GE_CC_Airborne에서 설정한 값은 유지된다.
//
//   ULOAGE_CrowdControl  방향 규칙 + 기본 거리 + 방향 계산 (ComputePushDirection)
//     ├ ULOAGE_Airborne   + 높이, 누워 있는 시간            → ULOAGA_Airborne
//     └ ULOAGE_Paralysis  + 경직 시간                       → ULOAGA_Paralysis
//
// [방향] Launch Direction (자식 GE 에셋 칸)
//   Away From Source : 때린 것의 판정 기준점 → 맞은 사람 (장판 중심, 부채꼴 꼭짓점, 투사체 위치). 기본
//   Source Forward   : 때린 것(장판·투사체, 없으면 보스)의 앞 방향 + Yaw Offset. 장풍처럼 한 방향으로만
//
// [네트워크] 방향 계산은 서버만 (밀기·날리기를 서버만 하므로). 결과 위치·속도는 엔진의 이동 보정으로 간다

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_CrowdControl.generated.h"

/** 밀거나 날리는 방향 규칙 */
UENUM(BlueprintType)
enum class ELOALaunchDirection : uint8
{
	/** 판정 기준점 → 맞은 사람. 장판·내려찍기 (기본) */
	AwayFromSource	UMETA(DisplayName = "Away From Source"),
	/** 때린 것의 앞 방향 + Yaw Offset. 장풍처럼 한 방향 */
	SourceForward	UMETA(DisplayName = "Source Forward"),
};

UCLASS(Abstract)
class LOA_API ULOAGE_CrowdControl : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/**
	 * 이 GE를 맞은 사람이 밀려날 수평 방향 (길이 1). [서버]
	 * Spec의 GE가 ULOAGE_CrowdControl이 아니면 Away From Source로 계산한다.
	 *   예) 장판 중심이 (0, 0), 맞은 사람이 (300, 0) → (1, 0)
	 *   예) Source Forward, 보스가 (1, 0)을 보고 Yaw Offset 90 → (0, 1) (보스의 오른쪽)
	 */
	static FVector ComputePushDirection(const FGameplayEffectSpec& Spec, const AActor* Victim);

	/** 어느 쪽으로 밀거나 날리나 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CrowdControl")
	ELOALaunchDirection LaunchDirection = ELOALaunchDirection::AwayFromSource;

	/** Source Forward일 때 앞 방향에서 더 돌릴 각도. 예) 90 → 때린 것의 오른쪽으로 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CrowdControl", meta = (EditCondition = "LaunchDirection == ELOALaunchDirection::SourceForward", Units = "Degrees"))
	float YawOffset = 0.f;

	/** 밀리는(날아가는) 거리 (cm). Payload에 SetByCaller.LaunchDistance가 없을 때. 0이면 밀리지 않음 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CrowdControl", meta = (ClampMin = "0", Units = "Centimeters"))
	float DefaultDistance = 300.f;
};
