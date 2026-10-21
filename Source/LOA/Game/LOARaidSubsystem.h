// LOARaidSubsystem.h
// [배관] 레이드 상황판. "지금 싸우는 보스가 누구인가"를 컴퓨터마다 하나씩 들고 있다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가]
// 보스 체력바는 모든 플레이어 화면(HUD)에 떠야 한다. HUD는 "보스가 누구인지" 알아야 한다.
// 레벨을 뒤지거나(GetAllActorsOfClass) 보스가 HUD를 직접 찾는 대신, 보스가 여기 등록하고 HUD는 여기 알림을 구독한다.
//   보스 → RegisterBoss(this)     ─▶  OnBossChanged(보스)    ─▶  HUD가 체력바 생성
//   보스 → UnregisterBoss(this)   ─▶  OnBossChanged(nullptr) ─▶  HUD가 체력바 제거
// 나중에 기믹·페이즈 코드도 "지금 보스"를 여기서 찾는다.
//
// [토벌 완료 - OnBossDefeated]
// 이 상황판은 보스가 죽었는지 "판단"하지 않는다. 보스 몸의 사망 반응 부품(ULOADeathReactionComponent)이
// 알려 주는 것을 HUD 등에 그대로 전한다.
//   (서버) GA_Death → State.Dead ─복제→ 각 컴퓨터의 보스 사본에 태그
//   각 컴퓨터: 보스의 사망 반응 부품 OnDied → 이 상황판 → OnBossDefeated(보스) → HUD가 토벌 완료 UI
// 늦게 들어온 플레이어: 보스가 이미 죽은 채로 등록되면 등록 직후 바로 OnBossDefeated.
// HUD가 상황판보다 늦게 구독했으면 IsCurrentBossDefeated()로 지금 상태를 확인한다.
//
// [월드 서브시스템이란]
// 월드(맵) 하나에 하나씩 엔진이 자동으로 만들어 주는 객체. 맵이 바뀌면 새로 만들어진다.
// 따로 배치하거나 생성할 필요가 없다: ULOARaidSubsystem::Get(this)로 어디서나 꺼낸다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크 - 복제하지 않는다]
// 서브시스템은 컴퓨터마다 따로 있고 복제되지 않는다.
// 대신 보스 액터가 복제되어 모든 컴퓨터에 존재하고, 각 컴퓨터에서 보스의 BeginPlay가 각자 등록한다.
// → 복제 변수를 새로 만들지 않아도 모든 컴퓨터가 같은 보스를 안다.
//   (GameState에 CurrentBoss를 복제하는 방식보다 단순하고, 복제 도착 순서를 신경 쓸 필요가 없다)
//
// (같은 이유로 "토벌 완료"도 복제하지 않는다. 보스의 State.Dead가 이미 모든 컴퓨터에 복제되기 때문.
//  나중에 "클리어 → 연출 → 보상"처럼 주인 없는 레이드 진행 상태가 생기면 그건 GameState 복제 변수에 둔다)
//
// [위치 표시] (ALOALocationMarker)
// 레벨에 놓은 위치 표시가 BeginPlay에서 태그와 함께 등록한다. GA의 Move·Spawn 모듈이 태그로 위치를 찾는다.
//   예) FindLocation(Location.Arena.Center) → 맵 중앙. 보스 등록과 같은 방식(각 컴퓨터에서 각자 등록, 복제 없음).
//
// [보스는 한 번에 하나]
// 이 레이드는 보스 하나다. 다른 보스가 등록돼 있는데 또 등록하면 경고를 남기고 새 보스로 바꾼다.
// (보스가 여럿 동시에 나오는 레이드를 만들면 목록으로 바꾼다)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "LOARaidSubsystem.generated.h"

class ALOABossCharacter;
class ALOALocationMarker;

/** 지금 보스가 바뀌었다. 없어졌으면 nullptr */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOABossChangedSignature, ALOABossCharacter*, NewBoss);

/** 지금 보스가 쓰러졌다 (토벌 완료) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOABossDefeatedSignature, ALOABossCharacter*, Boss);

UCLASS()
class LOA_API ULOARaidSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 레이드 상황판. 월드가 없거나 게임 월드가 아니면 nullptr */
	static ULOARaidSubsystem* Get(const UObject* WorldContextObject);

	/** [모든 컴퓨터] 보스가 BeginPlay에서 호출. 지금 보스로 정하고 OnBossChanged를 보낸다. */
	void RegisterBoss(ALOABossCharacter* Boss);

	/** [모든 컴퓨터] 보스가 EndPlay에서 호출. 지금 보스가 그 보스일 때만 비우고 OnBossChanged(nullptr)를 보낸다. */
	void UnregisterBoss(ALOABossCharacter* Boss);

	/** 지금 보스. 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "LOA|Raid")
	ALOABossCharacter* GetCurrentBoss() const { return CurrentBoss.Get(); }

	/** [모든 컴퓨터] 위치 표시가 BeginPlay에서 호출. 같은 태그가 이미 있으면 경고 후 새 것으로 바꾼다. */
	void RegisterLocation(ALOALocationMarker* Marker);

	/** [모든 컴퓨터] 위치 표시가 EndPlay에서 호출. 그 태그가 이 표시일 때만 지운다. */
	void UnregisterLocation(ALOALocationMarker* Marker);

	/**
	 * 태그로 위치 찾기. (예: Location.Arena.Center → 맵 중앙)
	 * @return 그 태그의 표시가 없으면 false (OutLocation은 그대로)
	 */
	bool FindLocation(const FGameplayTag& LocationTag, FVector& OutLocation) const;

	/** 지금 보스가 쓰러졌나. HUD가 늦게 구독했을 때 지금 상태를 확인하는 용도 */
	UFUNCTION(BlueprintPure, Category = "LOA|Raid")
	bool IsCurrentBossDefeated() const { return bCurrentBossDefeated; }

	/** 지금 보스가 바뀔 때. (HUD, 나중에 기믹·연출 BP가 구독) 이 컴퓨터 안에서만 알린다. */
	UPROPERTY(BlueprintAssignable, Category = "LOA|Raid")
	FLOABossChangedSignature OnBossChanged;

	/** 지금 보스가 쓰러졌을 때 한 번. (HUD의 토벌 완료 UI가 구독) 이 컴퓨터 안에서만 알린다. */
	UPROPERTY(BlueprintAssignable, Category = "LOA|Raid")
	FLOABossDefeatedSignature OnBossDefeated;

protected:
	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 만든다. 에디터 화면, 에셋 미리보기 월드에는 필요 없다. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

private:
	/** 보스의 사망 반응 부품이 "죽었다"고 알릴 때 (AddDynamic이라 UFUNCTION) */
	UFUNCTION()
	void HandleBossDied(AActor* DeadActor);

	/** 지금 보스를 쓰러진 상태로 바꾸고 OnBossDefeated를 보낸다. (한 번만) */
	void MarkCurrentBossDefeated();

	/** 지금 보스. 보스 액터가 사라지면 자동으로 비는 약한 참조 */
	TWeakObjectPtr<ALOABossCharacter> CurrentBoss;

	/** 지금 보스가 쓰러졌나. 보스가 바뀌거나 등록 해제되면 false */
	bool bCurrentBossDefeated = false;

	/** 위치 표시들 (태그 → 표시). 표시가 사라지면 자동으로 비는 약한 참조 */
	TMap<FGameplayTag, TWeakObjectPtr<ALOALocationMarker>> Locations;
};
