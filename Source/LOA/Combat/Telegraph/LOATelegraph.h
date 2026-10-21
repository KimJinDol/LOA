// LOATelegraph.h
// [배관] 예고 한 장. 받은 데이터(FLOATelegraphData)대로 바닥에 데칼을 켜고 끄기만 한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   하는 일 : "받은 데이터를 바탕으로 예고 장판을 그린다." 그게 전부다.
//   아는 것   : 데칼로 그리는 법 전부 (모양 → 데칼 상자 크기·위치, 바닥으로 눕히기, 머티리얼 칸 이름)
//   모르는 것 : 판정 방법(대상 찾기, 몸통 판정), 스킬 모듈, 누가 불렀는지, 언제 사라질지, 풀
//              모양은 판정과 같은 FLOAHitShape 값을 받는다. (값만. 전투 함수는 모름)
//              → 데이터 만들기는 부르는 쪽(ALOAAreaActor), 꺼내고 돌려놓기는 ULOATelegraphSubsystem
//   예고는 뜬 자리에 고정이다. 어떤 액터도 따라가지 않는다. (로아 장판 규칙)
//
//   이 액터는 만들고 지우지 않는다. ULOATelegraphSubsystem이 미리 만들어 숨겨 두었다가 꺼내 쓰고 다시 숨긴다. (풀)
//     SetupLayers  : 만들 때 한 번. Style의 층마다 데칼 + 머티리얼 사본을 만든다. (비싼 일은 여기서만)
//     ShowTelegraph: 꺼낼 때. 자리, 크기, 칸 값만 바꾸고 보이게 한다. (싼 일)
//     HideTelegraph: 돌려놓을 때. 숨긴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [차오름]
//   이 액터는 매 프레임 아무것도 하지 않는다. (Tick 없음)
//   꺼낼 때 머티리얼 사본에 StartTime, HitTime을 한 번 쓰면, 머티리얼이 MPC_Telegraph의 ServerTime을 보고 스스로 차오른다.
//   예) StartTime 100.0, HitTime 101.2, 지금 ServerTime 101.0 → (101.0 − 100.0) ÷ 1.2 = 83%
//
// [데칼 방향]
//   데칼은 자기 X축 방향으로 투영된다. 데칼을 Pitch −90으로 붙이면 X가 바닥을 향한다.
//   그러면 데칼의 Y = 좌우, Z = 앞(액터가 보는 방향). DecalSize = (위아래 깊이, 좌우 반, 앞뒤 반)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   복제하지 않는다. 컴퓨터마다 자기 ULOATelegraphSubsystem이 자기 것을 만들어 쓴다. (네트워크 세 규칙 3번)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LOATelegraph.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class ULOATelegraphStyle;
struct FLOATelegraphData;

UCLASS(NotBlueprintable, Transient)
class LOA_API ALOATelegraph : public AActor
{
	GENERATED_BODY()

public:
	ALOATelegraph(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 만들 때 한 번. Style의 층마다 데칼과 머티리얼 사본을 만들어 둔다. 숨겨진 채로 시작한다. */
	void SetupLayers(ULOATelegraphStyle* InStyle);

	/** 데이터대로 자리·크기·칸 값을 바꾸고 보이게 한다. (모양 → 데칼 상자 계산도 여기서) */
	void ShowTelegraph(const FLOATelegraphData& Data);

	/** 숨긴다. (풀로 돌아갈 때) */
	void HideTelegraph();

	/** 이 예고가 만들어진 Style (풀을 고를 때) */
	ULOATelegraphStyle* GetStyle() const { return Style; }

private:
	/** 데칼들을 붙이는 루트. 이 루트의 위치·방향 = 예고 중심·보는 방향 */
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	/** 층마다 데칼 하나 (층 순서) */
	UPROPERTY()
	TArray<TObjectPtr<UDecalComponent>> Decals;

	/** 층마다 머티리얼 사본. 칸 값(모양, 시각)을 바꾸려면 사본이 필요하다. 만들 때 한 번만 만든다. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LayerMaterials;

	/** 이 예고가 만들어진 Style */
	UPROPERTY()
	TObjectPtr<ULOATelegraphStyle> Style;

	/** 숨기기/보이기. 액터와 데칼 둘 다 (데칼은 액터 숨김만으로 안 꺼지는 경우가 있어 직접 끈다) */
	void SetTelegraphVisible(bool bVisible);
};
