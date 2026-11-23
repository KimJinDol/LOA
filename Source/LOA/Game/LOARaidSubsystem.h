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
//   (서버) GA_Death → State.Body.Dead ─복제→ 각 컴퓨터의 보스 사본에 태그
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
// [지형 구역] (ALOATerrainZone, 2026-11-14 2페이즈 지형 파괴)
//   구역도 위치 표시처럼 BeginPlay에서 각자 등록한다. 상황판이 하는 일 [서버]:
//     고르기     : RequestTerrainCollapse → 아직 멀쩡한 구역 중 무작위 하나 StartCollapse (보스 StateTree 작업·치트가 부름)
//                  예) 왼·오 둘 다 멀쩡 → 오른쪽 / 다음 요청 → 남은 왼쪽 / 그다음 → 없음 (false)
//     옮기기     : 구역이 무너지면(OnCollapsed) 그 안에 있는 시체·보스를 가운데 원(안 무너짐)으로 옮긴다
//                  시체: 이동이 꺼져 있어 바닥이 없어도 떨어지지 않고 허공에 남는다 → 부활하면 다시 떨어짐. 사용자 규칙: 시체는 언제나 부활할 수 있는 곳에
//                  보스: 떨어지면 엔진이 지운다 (FellOutOfWorld 기본)
//     되돌리기   : ResetTerrain (전멸)
//   찾기 [모든 컴퓨터]: FindRecoverLocation → 무너진 구역 안 자리면 가운데 원 안쪽 자리. 낙사·사망한 캐릭터가 부른다
//   구역은 누가 고르는지·누가 듣는지 모른다. "언제"는 보스 StateTree(LOA Collapse Terrain)가 정한다
//
// [대난투 경기장] (ALOABrawlArena, 2026-11-18 apply122)
//   경기장도 BeginPlay에서 각자 등록한다. 레이드에 하나 (보스처럼). 찾기만 한다: GetBrawlArena
//   부르는 곳: 보스 그로기 GA(ULOAGA_BossBrawl, 서버) → StartBrawl / 내 컨트롤러(ALOAPlayerController, 각자) → 페이드·줌인 값
//   (2026-11-18 apply125) 등록·해제 때 OnBrawlArenaChanged → HUD가 대난투 UI를 붙인다 (HUD와 경기장의 BeginPlay 순서가 어느 쪽이든)

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
class ALOATerrainZone;
class ALOABrawlArena;
enum class ELOATerrainSide : uint8;

/** 대난투 경기장이 등록됨 / 해제됨(nullptr) (2026-11-18 apply125) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOABrawlArenaChangedNative, ALOABrawlArena* /*Arena*/);

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

	/** 태그로 위치 + 방향 찾기. (예: Location.Raid.Entrance → 입구 자리와 표시가 보는 방향. 여러 명을 옆으로 벌려 세울 때) */
	bool FindLocationTransform(const FGameplayTag& LocationTag, FTransform& OutTransform) const;

	// ---------- 지형 구역 (헤더 [지형 구역]) ----------

	/** [모든 컴퓨터] 구역이 BeginPlay에서 호출 */
	void RegisterTerrainZone(ALOATerrainZone* Zone);

	/** [모든 컴퓨터] 구역이 EndPlay에서 호출 */
	void UnregisterTerrainZone(ALOATerrainZone* Zone);

	/** [서버] 아직 멀쩡한 구역 중 무작위 하나를 무너뜨리기 시작한다. 남은 구역이 없으면 false */
	bool RequestTerrainCollapse();

	/** [서버] 그쪽 구역을 무너뜨리기 시작한다 (치트). 그쪽 구역이 없거나 이미 무너지는 중이면 false */
	bool RequestTerrainCollapse(ELOATerrainSide Side);

	/** [서버] 모든 구역을 멀쩡하게 (전멸, 치트) */
	void ResetTerrain();

	/** 멀쩡하지 않은 구역 수 (예고 중 + 무너짐). StateTree 조건 LOA Terrain Collapse Count가 본다 */
	int32 GetCollapsedTerrainCount() const;

	/**
	 * 이 자리가 무너진 구역 안이면 가운데 원 안쪽 자리를 OutLocation에. 아니면 false (OutLocation 그대로)
	 *   예) 가운데 원 반지름 800, 여유 100, 중심에서 서쪽 1200에서 죽음 → 중심에서 서쪽 700
	 */
	bool FindRecoverLocation(const FVector& Location, FVector& OutLocation) const;

	// ---------- 대난투 경기장 (헤더 [대난투 경기장]) ----------

	/** [모든 컴퓨터] 경기장이 BeginPlay에서 호출. 이미 있으면 경고 후 새 것으로 */
	void RegisterBrawlArena(ALOABrawlArena* Arena);

	/** [모든 컴퓨터] 경기장이 EndPlay에서 호출. 그 경기장일 때만 비운다 */
	void UnregisterBrawlArena(ALOABrawlArena* Arena);

	/** 대난투 경기장. 레벨에 없으면 nullptr */
	ALOABrawlArena* GetBrawlArena() const { return BrawlArena.Get(); }

	/** [모든 컴퓨터] 경기장이 등록됨 / 해제됨(nullptr) (2026-11-18 apply125). 듣는 쪽: ALOAHUD (대난투 UI) */
	FLOABrawlArenaChangedNative OnBrawlArenaChanged;

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

	/** [서버] 구역이 무너졌다 → 그 안의 시체·보스를 가운데 원으로 */
	void HandleTerrainZoneCollapsed(ALOATerrainZone* Zone);

	/** 대난투 경기장 (사라지면 자동으로 비는 약한 참조) */
	TWeakObjectPtr<ALOABrawlArena> BrawlArena;

	/** 등록된 구역들 (구역이 사라지면 자동으로 비는 약한 참조) */
	TArray<TWeakObjectPtr<ALOATerrainZone>> TerrainZones;

	/** 지금 보스. 보스 액터가 사라지면 자동으로 비는 약한 참조 */
	TWeakObjectPtr<ALOABossCharacter> CurrentBoss;

	/** 지금 보스가 쓰러졌나. 보스가 바뀌거나 등록 해제되면 false */
	bool bCurrentBossDefeated = false;

	/** 위치 표시들 (태그 → 표시). 표시가 사라지면 자동으로 비는 약한 참조 */
	TMap<FGameplayTag, TWeakObjectPtr<ALOALocationMarker>> Locations;
};
