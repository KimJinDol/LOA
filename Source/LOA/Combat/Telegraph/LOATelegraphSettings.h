// LOATelegraphSettings.h
// [배관] 예고 설정. Project Settings → Game → LOA Telegraph 에 보인다. (값은 Config/DefaultGame.ini에 저장)
//
//   Time Collection : MPC_Telegraph. ULOATelegraphSubsystem이 매 프레임 여기의 ServerTime 칸 하나만 쓴다.
//                     ※ 이 MPC에는 "모든 예고가 같이 보는 값"(서버 시각)만 넣는다.
//                       예고마다 다른 값(Fill 등)을 넣으면 모든 장판이 같은 값으로 그려진다.
//   Prewarm Styles  : 맵이 시작할 때 풀을 미리 채워 둘 Style. 넣지 않은 Style은 처음 쓰일 때 채운다.
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
};
