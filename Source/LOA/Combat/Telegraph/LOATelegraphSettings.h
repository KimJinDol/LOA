// LOATelegraphSettings.h
// [배관] 예고 설정. Project Settings → Game → LOA Telegraph 에 보인다. (값은 Config/DefaultGame.ini에 저장)
//
//   Time Collection : MPC_Telegraph. ULOATelegraphSubsystem이 매 프레임 여기의 ServerTime 칸 하나만 쓴다.
//                     ※ 이 MPC에는 "모든 예고가 같이 보는 값"(서버 시각)만 넣는다.
//                       예고마다 다른 값(Fill 등)을 넣으면 모든 장판이 같은 값으로 그려진다.
//   Prewarm Styles  : 맵이 시작할 때 풀을 미리 채워 둘 Style. 넣지 않은 Style은 처음 쓰일 때 채운다.
//   Lead Margin     : 예고 앞당김 여유 (초). 클라 예고 = 서버 판정 − 내 편도 − 이 값에 꽉 찬다 (ULOATelegraphSubsystem [예고 앞당김])
//
// 서브시스템은 BP 기본값 칸을 가질 수 없어서 설정을 여기 둔다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LOATelegraphSettings.generated.h"

class UMaterialParameterCollection;
class ULOATelegraphStyle;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "LOA Telegraph"))
class LOA_API ULOATelegraphSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Project Settings의 "Game" 묶음에 보이게 */
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 서버 시각을 넣을 MPC (MPC_Telegraph). Scalar 칸 ServerTime이 있어야 한다. 비우면 예고가 차오르지 않는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraph")
	TSoftObjectPtr<UMaterialParameterCollection> TimeCollection;

	/** 맵 시작 때 풀을 미리 채울 Style. 예) DA_Telegraph_BossDanger */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraph")
	TArray<TSoftObjectPtr<ULOATelegraphStyle>> PrewarmStyles;

	/**
	 * 예고 앞당김 여유 (초, 2026-10-29 측정으로 정함). 원격 클라에서만 쓴다 (리슨 호스트는 서버 위치 = 자기 위치)
	 * 편도만 앞당기면 남는 어긋남 약 0.02초 = 서버 프레임 하나 (클라 시계가 약 한 프레임 늦고, 서버 위치가 한 프레임 묵음. 핑과 상관없음)
	 * 0.04 = 측정 최대 0.024 + 서버 30fps 한 프레임(0.033)까지. 크면 예고가 그만큼 짧아진다
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0", ClampMax = "0.2", Units = "Seconds"))
	float LeadMargin = 0.04f;
};
