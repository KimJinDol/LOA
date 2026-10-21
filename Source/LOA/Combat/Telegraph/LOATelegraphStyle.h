// LOATelegraphStyle.h
// [배관] 예고 겉모습 데이터 에셋. "어떤 머티리얼을 몇 층으로 깔까"만 담는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [예]
//   DA_Telegraph_BossDanger   : 층 [0] M_Telegraph (빨강)              일반 보스 장판
//   DA_Telegraph_BossWipe     : 층 [0] MI_Telegraph_Purple, [1] 차오름   즉사기·전멸기
//   같은 판정 모양이라도 범위 액터 BP(ALOAAreaActor)의 Telegraph Style 칸만 바꾸면 겉모습이 바뀐다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [데칼 층과 머티리얼의 약속 = 칸 이름]
//   예고 한 장 = 데칼 여러 장을 같은 자리에 겹친 것. 층마다 머티리얼 하나. 뒤 칸일수록 위에 그려진다.
//   C++이 모든 층에 같은 이름으로 채운다. 쓰지 않는 칸은 머티리얼이 무시하면 된다.
//     ShapeType  : 0 = 원/부채꼴 계열, 1 = 사각형
//     InnerRatio : 안쪽 반지름 ÷ 바깥 반지름 (도넛 구멍. 0 = 구멍 없음)     예) 도넛 400~800 → 0.5
//     Angle      : 부채꼴 전체 각도 (도). 원은 360                            예) 앞 베기 → 90
//     StartTime  : 차오르기 시작한 서버 시각 (초)                              예) 100.0
//     HitTime    : 꽉 차는 서버 시각 = 판정 시각 (초)                          예) 101.2
//   차오름(Fill)은 C++이 쓰지 않는다. 머티리얼이 MPC_Telegraph의 ServerTime으로 직접 계산한다. (MF_TelegraphFill)
//     Fill = Saturate((ServerTime − StartTime) ÷ (HitTime − StartTime))
//
// [풀]
//   ULOATelegraphSubsystem은 Style마다 예고를 따로 모아 둔다. (층 머티리얼이 다르면 만들어 둔 머티리얼 사본을 다시 쓸 수 없어서)
//   Pool Size만큼 미리 만들어 두고, 모자라면 하나씩 더 만들며 경고를 남긴다.
//   미리 만드는 시점: Project Settings → Game → LOA Telegraph → Prewarm Styles에 넣은 Style은 맵 시작 때,
//                    넣지 않은 Style은 처음 쓰일 때 (그 순간 한 번 멈칫할 수 있다)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LOATelegraphStyle.generated.h"

class UMaterialInterface;

UCLASS(BlueprintType)
class LOA_API ULOATelegraphStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<TObjectPtr<UMaterialInterface>>& GetDecalLayers() const { return DecalLayers; }
	float GetProjectionDepth() const { return ProjectionDepth; }
	int32 GetPoolSize() const { return PoolSize; }

#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사. 오류: 층이 없음 / 머티리얼을 안 고른 층 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** 데칼 층. 칸마다 머티리얼 하나 (Material Domain = Deferred Decal). 뒤 칸일수록 위에 그려진다. */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph")
	TArray<TObjectPtr<UMaterialInterface>> DecalLayers;

	/**
	 * 데칼이 위아래로 닿는 깊이 (cm, 바닥 위아래 각각). 바닥이 울퉁불퉁하거나 계단이 있으면 키운다.
	 * 너무 크면 아래층 바닥, 벽, 캐릭터 몸에도 찍힌다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float ProjectionDepth = 200.f;

	/** 미리 만들어 둘 예고 수. 동시에 이보다 많이 깔리면 더 만들고 경고를 남긴다. 예) 장판 30개 패턴이면 32 */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph", meta = (ClampMin = "1"))
	int32 PoolSize = 16;
};
