// LOALocationMarker.h
// [배관] 위치 표시. 레벨에 놓고 태그를 붙이면, GA(에셋)가 그 태그로 이 위치를 찾는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가]
//   GA는 에셋이라 레벨 안의 액터를 직접 가리킬 수 없다. (엔진 규칙) 좌표를 GA에 적으면 맵이 바뀔 때마다 고쳐야 한다.
//   → GA에는 "이름(태그)"만 적고, 실제 위치는 레벨에 놓은 이 표시가 정한다.
//   예) 패턴 D : Move Destination = Location.Arena.Center → 맵 중앙의 표시까지 이동
//               Spawn Location     = Location.Arena.Center → 맵 중앙에 큰 원 장판
//       (나중) 1페이즈 기믹 기둥 자리 : Location.Arena.PillarA / PillarB
//
// [흐름]
//   BeginPlay (모든 컴퓨터) : ULOARaidSubsystem에 등록 (태그 → 이 표시)
//   EndPlay                : 등록 해제
//   찾기                    : ULOARaidSubsystem::FindLocation(태그, 위치)  ← Move·Spawn 모듈 (서버)
//   같은 태그가 둘이면 나중 것으로 바꾸고 경고.
//
// [네트워크]
//   1. 서버만 : 없음 (찾아서 쓰는 쪽이 서버)
//   2. 복제   : 없음. 레벨에 미리 놓인 액터라 모든 컴퓨터에 원래 있다.
//   3. 각자   : 각 컴퓨터에서 각자 등록 (보스 등록과 같은 방식)
//
// [에디터] 레벨에 끌어다 놓기 → Location Tag 고르기 (Project Settings → Gameplay Tags에 Location.* 추가)
//   게임 중에는 보이지 않는다. 에디터에서만 아이콘이 보인다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "LOALocationMarker.generated.h"

class UBillboardComponent;

UCLASS()
class LOA_API ALOALocationMarker : public AActor
{
	GENERATED_BODY()

public:
	ALOALocationMarker(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 이 표시의 이름 (예: Location.Arena.Center) */
	const FGameplayTag& GetLocationTag() const { return LocationTag; }

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** GA가 이 위치를 찾을 때 쓰는 이름. 레벨 안에서 하나만 */
	UPROPERTY(EditInstanceOnly, Category = "LOA|Location", meta = (Categories = "Location"))
	FGameplayTag LocationTag;

#if WITH_EDITORONLY_DATA
	/** 에디터에서만 보이는 아이콘 */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorIcon;
#endif
};
