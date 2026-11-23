// LOARepairExit.h
// [배관] 정비소 출구. 레벨의 정비소 구역에 놓는 상자. 살아 있는 플레이어 전원이 이 상자 안에 들어오면 전투 구역으로 출발한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (사용자 결정 2026-10-25) 예) 3명, 정비소에 도착
//   A가 상자에 들어옴 → [서버] 겹침 시작 → ALOAGameState::CheckRepairExit → 1 / 3 → 기다림
//   B가 들어옴        → 2 / 3 → 기다림
//   A가 나감          → 겹침 끝 → 1 / 3
//   A, C가 들어옴     → 3 / 3 → 화면 전환 → 전투 구역 입구(Location.Raid.Entrance)로
//   판단은 GameState가 한다 (단계가 정비소일 때만, 누가 살아 있나). 이 상자는 "누가 안에 있나"만 안다.
//
// [네트워크]
//   1. 서버만 : 겹침 알림 → GameState 확인 (클라에서도 겹침은 일어나지만 무시)
//   2. 복제   : 없음. 레벨에 미리 놓인 액터라 모든 컴퓨터에 원래 있다. 결과(단계, 순간이동)는 GameState·캐릭터가 복제
//   3. 각자   : 없음
//
// [에디터] 레벨의 정비소 구역에 끌어다 놓고 Exit Box 크기 조절. 게임 중에는 안 보인다.
//   정비소 자리(Location.Raid.Respawn 위치 표시)는 이 상자 밖에 둔다 (도착하자마자 출발하지 않게).
//   레벨에 하나만 둔다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LOARepairExit.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class LOA_API ALOARepairExit : public AActor
{
	GENERATED_BODY()

public:
	ALOARepairExit(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 이 캐릭터가 상자 안에 있나 (캡슐이 상자와 겹치면 안) */
	bool IsCharacterInside(const AActor* Character) const;

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

private:
	/** [서버] 누가 들어옴 / 나감 → GameState에 확인 요청 */
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** [서버] GameState에 "다 모였나" 확인 요청 */
	void RequestCheck() const;

	/** 출구 상자. 크기는 레벨에서 조절 */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Repair")
	TObjectPtr<UBoxComponent> ExitBox;
};
