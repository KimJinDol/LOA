// LOAHUD.h
// [배관] 내 화면 UI의 "주인". 화면 위젯과 위젯 컨트롤러를 만들고, 서로 연결하고, 정리한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [HUD는 어디에 있나]
// 본인 컴퓨터에만 있다. PlayerController가 하나씩 만든다.
//   - 데디케이티드 서버: 없음 (화면이 없으니까)
//   - 리슨 서버 호스트: 호스트 본인 것 1개만 (다른 플레이어 것은 각자 컴퓨터에)
// 그래서 이 클래스 안에서는 "내 화면인가?" 검사가 필요 없다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [초기화 흐름]
//
//   PlayerController::TryInitHUD()          ← 캐릭터 준비 + HUD 존재 둘 다 확인된 뒤에만 호출
//      ▼
//   InitOverlay(PC, PS, ASC)                 ← 이 클래스. 한 번만 실행 (두 번째부터 무시)
//      ├─ ① 위젯 컨트롤러 생성 → Init (ASC에 알림 걸기)
//      ├─ ② 화면 위젯 생성 → SetWidgetController (BP가 알림에 바인딩)
//      ├─ ③ 컨트롤러 BroadcastInitialValues (위젯이 처음 값으로 채워짐)
//      └─ ④ 화면에 붙이기 (AddToViewport)
//
// ②가 ③보다 먼저인 이유: 위젯이 바인딩하기 전에 값을 보내면 아무도 못 받는다.
//
// [왜 한 번만인가]
// 위젯 컨트롤러가 지켜보는 ASC는 PlayerState 것이다. 캐릭터가 죽고 새 몸으로 부활해도 ASC는 그대로다.
// 그래서 부활 때 HUD를 다시 만들 필요가 없다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [전투 글자 - ShowCombatText] (2026-11-16 apply116: 옛 "데미지 숫자 - ShowDamageNumber". 회복·버프도 같은 길)
// 글자 하나를 레이어에 넘긴다. HUD는 넘겨주기만 하고, 풀/위치/수명/상태 표는 레이어가 담당한다.
//   데미지 숫자 : 서버(데미지 확정) → ULOACombatRPCComponent::ClientShowCombatText (PC에 붙은 컴포넌트) → ShowCombatText → ULOACombatTextLayer
//   회복 숫자   : 서버(회복 확정, 받은 사람에게만) → 같은 RPC (종류 Heal) → ShowCombatText
//   버프 이름   : 이 컴퓨터. InitOverlay에서 내 ASC를 보는 상태 Tracker를 하나 만든다 → OnStatusTagAdded(태그)
//                 → State.Buff.* 면 Buff, State.Debuff.* 면 Debuff, State.CrowdControl.Status.* 면 Status (상태 이상도 띄움, 사용자 11-16 apply117)
//                   피격 이상(Push)·경직(Paralysis)은 상태 Tracker가 보지 않으므로 오지 않는다
//                 → ShowCombatText(내 캐릭터, 태그). 태그는 이미 모든 컴퓨터에 복제되어 RPC가 필요 없다
//     예) 세라스가 공증 오라를 켬 → 내 ASC에 State.Buff.DamageDealt가 처음 붙음 → 내 캐릭터 캡슐 가운데에 "주는 피해 증가" (상아색, 글자만)
//     같은 버프가 다시 걸려 시간만 늘면 띄우지 않는다 (사용자: 처음 붙을 때만. ULOAStatusTracker 헤더 [알림])
//
// 레이어는 BeginPlay에서 만든다. (ASC와 상관없는 위젯이라 InitOverlay를 기다릴 필요 없음)
// Overlay보다 아래에 그린다. (스킬바가 글자에 가려지지 않게)
//
// ─────────────────────────────────────────────────────────────────────────────
// [홀딩 저스트 가드 게이지] (2026-11-05)
// BeginPlay에서 만들어 화면에 붙여 둔다. 보일지·몇 %인지는 위젯이 내 장부를 보고 스스로 정한다 (ULOAJustGuardHoldBar)
//
// ─────────────────────────────────────────────────────────────────────────────
// [보스 체력바]
// 레이드 상황판(ULOARaidSubsystem)의 "지금 보스가 바뀜" 알림을 BeginPlay에서 구독한다.
//   OnBossChanged(보스)    → ShowBossHealthBar : 보스 위젯 컨트롤러 생성 → Init(보스) → 위젯 생성 → 연결 → 첫 값 → 화면에
//   OnBossChanged(nullptr) → HideBossHealthBar : 컨트롤러 Shutdown → 위젯 제거
// BeginPlay 때 이미 보스가 등록돼 있으면(보스 BeginPlay가 먼저 끝난 경우) 바로 띄운다. → 순서가 어느 쪽이든 동작
// InitOverlay(내 캐릭터 준비)를 기다리지 않는다. 보스 체력바는 내 ASC와 상관없다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [대난투 UI] (2026-11-18 apply125)
// BeginPlay에서 대난투 UI(ULOABrawlBar)를 만들어 붙여 둔다. 보일지·값은 위젯이 경기장을 보고 스스로 정한다
// 보스 체력바 숨기기는 여기서: 레이드 상황판 OnBrawlArenaChanged로 경기장을 찾고 → 경기장 OnPhaseChanged마다
//   ShouldShowBrawlUI(진행 중 · 나오는 중)면 보스 체력바 숨김, 아니면 원래대로 (사용자: 보스 HP UI 대신 대난투 UI)
// 경기장·HUD의 BeginPlay 순서가 어느 쪽이든 동작 (보스 체력바와 같은 방식)
//
// [협동 저가 결과] (2026-11-18 apply128)
// BeginPlay에서 협동 저가 결과 위젯(ULOACoopJustGuardResultWidget)을 만들어 붙여 둔다. 언제·어디 띄울지는 위젯이 경기장·보스를 보고 정한다
// 전투 글자 레이어와 같은 그리기 순서 (스킬바 아래)
//
// [레이드 결과] (2026-11-15 apply115)
// InitOverlay 끝에 GameState의 레이드 단계 알림을 구독한다 (Overlay 컨트롤러와 같은 때: 캐릭터가 준비됐으면 GameState도 와 있다)
//   단계 = 클리어 → ShowRaidResult: 결과 위젯 생성 → ShowResult(GameState의 결과) → 화면 맨 위에
// 이미 클리어 단계면(늦게 구독) 바로 띄운다. 결과는 단계와 같이 복제되어 이때 이미 있다 (ALOAGameState 헤더 [클리어])
//
// [BP에서 설정할 것] BP_LOAHUD (부모: LOAHUD)
//   Combat Text Layer Class         = WBP_DamageNumberLayer (전투 글자 레이어. 옛 칸 이름 Damage Number Layer Class, CoreRedirects)
//   Overlay Widget Class            = WBP_Overlay
//   Overlay Widget Controller Class = LOAOverlayWidgetController (BP 자식을 만들었다면 그것)
//   Boss Health Bar Widget Class    = WBP_BossHealthBar (비워 두면 보스 체력바 없음)
//   Boss Widget Controller Class    = 비워 두면 LOABossWidgetController
//   Raid Result Widget Class        = WBP_RaidResult (비워 두면 클리어 화면 없음)
//   Brawl Bar Class                 = WBP_BrawlBar (비워 두면 대난투 UI 없음, 2026-11-18)
//   Coop Just Guard Result Class    = WBP_CoopJustGuardResult (비워 두면 협동 저가 결과 없음, 2026-11-18 apply128)
// 그리고 BP_LOAGameMode의 HUD Class = BP_LOAHUD

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/Combat/CombatText/LOACombatTextTypes.h"
#include "GameplayTagContainer.h"
#include "Components/SlateWrapperTypes.h"	// ESlateVisibility (대난투 중 보스 체력바 숨김)
#include "LOAHUD.generated.h"

class APlayerController;
class APlayerState;
class UAbilitySystemComponent;
class ULOAUserWidget;
class ULOAOverlayWidgetController;
class ULOACombatTextLayer;
class ULOAStatusTracker;
class ULOABossWidgetController;
class ALOABossCharacter;
class ULOAJustGuardHoldBar;
class ULOARaidResultWidget;
class ALOAGameState;
class ALOABrawlArena;
class ULOABrawlBar;
class ULOACoopJustGuardResultWidget;
enum class ELOARaidPhase : uint8;

UCLASS()
class LOA_API ALOAHUD : public AHUD
{
	GENERATED_BODY()

public:
	/**
	 * 화면 위젯 + 컨트롤러를 만들어 연결한다. [본인 컴퓨터]
	 * 이미 만들었으면 아무것도 하지 않는다. (몇 번 불러도 안전)
	 */
	void InitOverlay(APlayerController* InPlayerController, APlayerState* InPlayerState, UAbilitySystemComponent* InASC);

	/** InitOverlay가 끝났는지 */
	bool IsOverlayInitialized() const { return OverlayWidget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "LOA|UI")
	ULOAOverlayWidgetController* GetOverlayWidgetController() const { return OverlayWidgetController; }

	/** 지금 보스 체력바의 컨트롤러. 보스가 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "LOA|UI")
	ULOABossWidgetController* GetBossWidgetController() const { return BossWidgetController; }

	/** 대상에 전투 글자를 띄운다 (데미지 · 회복 숫자, 버프 · 디버프 이름). [본인 컴퓨터] 레이어에 넘기기만 한다. 헤더 [전투 글자] */
	void ShowCombatText(AActor* Target, const FLOACombatTextInfo& Info);

protected:
	//~ Begin AActor Interface
	/** 전투 글자 레이어 생성 (글자 위젯 미리 만들기 포함) + 레이드 상황판 구독 (보스 체력바) */
	virtual void BeginPlay() override;

	/** 컨트롤러의 ASC 알림 해제, 위젯 제거, 상황판 구독 해제 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** 레이드 상황판 알림. 보스가 생기면 체력바를 띄우고, 없어지면(nullptr) 내린다. [본인 컴퓨터] */
	UFUNCTION()
	void HandleBossChanged(ALOABossCharacter* NewBoss);

private:
	// ---------- 데이터 (BP 기본값에서 지정) ----------
	/** 내 화면 최상위 위젯 (체력/마나 바, 스킬 슬롯들이 들어 있는 WBP_Overlay) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI")
	TSubclassOf<ULOAUserWidget> OverlayWidgetClass;

	/** 내 화면 위젯 컨트롤러 클래스. 비워 두면 기본 C++ 클래스(ULOAOverlayWidgetController)를 쓴다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI")
	TSubclassOf<ULOAOverlayWidgetController> OverlayWidgetControllerClass;

	/** 전투 글자 레이어 (WBP_DamageNumberLayer). 비어 있으면 글자를 띄우지 않는다 (옛 칸 DamageNumberLayerClass, CoreRedirects) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|CombatText")
	TSubclassOf<ULOACombatTextLayer> CombatTextLayerClass;

	/** 레이어의 그리기 순서. Overlay(0)보다 작아야 스킬바 아래에 그려진다 (옛 칸 DamageNumberLayerZOrder) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|CombatText")
	int32 CombatTextLayerZOrder = -1;

	/** 홀딩 저스트 가드 게이지 (WBP_JustGuardHoldBar). 비어 있으면 띄우지 않는다 (2026-11-05) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|JustGuard")
	TSubclassOf<ULOAJustGuardHoldBar> JustGuardHoldBarClass;

	/** 보스 체력바 위젯 (WBP_BossHealthBar). 비어 있으면 보스 체력바를 띄우지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Boss")
	TSubclassOf<ULOAUserWidget> BossHealthBarWidgetClass;

	/** 보스 체력바 컨트롤러 클래스. 비워 두면 기본 C++ 클래스(ULOABossWidgetController)를 쓴다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Boss")
	TSubclassOf<ULOABossWidgetController> BossWidgetControllerClass;

	// ---------- 만든 것 (GC가 지우지 않도록 UPROPERTY) ----------
	UPROPERTY(Transient)
	TObjectPtr<ULOAUserWidget> OverlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULOAOverlayWidgetController> OverlayWidgetController;

	UPROPERTY(Transient)
	TObjectPtr<ULOACombatTextLayer> CombatTextLayer;

	UPROPERTY(Transient)
	TObjectPtr<ULOAUserWidget> BossHealthBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULOAJustGuardHoldBar> JustGuardHoldBar;

	UPROPERTY(Transient)
	TObjectPtr<ULOABossWidgetController> BossWidgetController;

	// ---------- 레이드 결과 (헤더 [레이드 결과]) ----------
	/** 클리어 결과 화면 (WBP_RaidResult). 비어 있으면 띄우지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Raid")
	TSubclassOf<ULOARaidResultWidget> RaidResultWidgetClass;

	/** 결과 화면의 그리기 순서. 다른 화면보다 위 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Raid")
	int32 RaidResultZOrder = 100;

	UPROPERTY(Transient)
	TObjectPtr<ULOARaidResultWidget> RaidResultWidget;

	/** 단계 알림을 건 GameState와 핸들 */
	TWeakObjectPtr<ALOAGameState> RaidGameState;
	FDelegateHandle RaidPhaseChangedHandle;

	/** 레이드 단계가 바뀜 → 클리어면 결과 화면 */
	void HandleRaidPhaseChanged(ELOARaidPhase NewPhase);

	/** 결과 화면을 만든다. 이미 있으면 아무것도 안 함 */
	void ShowRaidResult();

	// ---------- 대난투 UI (헤더 [대난투 UI], 2026-11-18 apply125) ----------
	/** 대난투 UI (WBP_BrawlBar). 비어 있으면 띄우지 않는다 (보스 체력바는 그래도 숨김) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Raid")
	TSubclassOf<ULOABrawlBar> BrawlBarClass;

	UPROPERTY(Transient)
	TObjectPtr<ULOABrawlBar> BrawlBar;

	/** 협동 저가 결과 (WBP_CoopJustGuardResult). 비어 있으면 띄우지 않는다 (헤더 [협동 저가 결과], apply128) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Raid")
	TSubclassOf<ULOACoopJustGuardResultWidget> CoopJustGuardResultClass;

	UPROPERTY(Transient)
	TObjectPtr<ULOACoopJustGuardResultWidget> CoopJustGuardResult;

	/** 상태 알림을 건 경기장과 핸들 */
	TWeakObjectPtr<ALOABrawlArena> WatchedBrawlArena;
	FDelegateHandle BrawlPhaseChangedHandle;
	FDelegateHandle BrawlArenaChangedHandle;

	/** 보스 체력바를 대난투 때문에 숨겼나 / 숨기기 전 보이기 값 (되돌릴 때) */
	bool bBossBarHiddenForBrawl = false;
	ESlateVisibility BossBarVisibilityBeforeBrawl = ESlateVisibility::SelfHitTestInvisible;

	/** 상황판: 경기장이 등록됨 / 해제됨 → 상태 알림을 옮겨 건다 */
	void HandleBrawlArenaChanged(ALOABrawlArena* Arena);
	void WatchBrawlArena(ALOABrawlArena* Arena);

	/** 경기장 상태가 바뀜 → 보스 체력바 숨김 / 되돌림 */
	void HandleBrawlPhaseChanged(ALOABrawlArena* Arena);
	void UpdateBossBarForBrawl();

	// ---------- 버프 · 디버프 글자 (헤더 [전투 글자], 2026-11-16) ----------
	/** 내 ASC의 상태를 본다. 새로 붙은 태그만 알려 준다 (InitOverlay에서 만듦) */
	UPROPERTY(Transient)
	TObjectPtr<ULOAStatusTracker> MyStatusTracker;

	/** 내 ASC (글자를 띄울 몸 = 지금 아바타를 그때 읽는다. 부활해도 ASC는 그대로) */
	TWeakObjectPtr<UAbilitySystemComponent> MyASC;

	/** 내 캐릭터에 상태 태그가 처음 붙음 → Buff·Debuff·Status면 내 캐릭터 캡슐 가운데에 이름 */
	void HandleMyStatusTagAdded(const FGameplayTag& StatusTag);

	// ---------- 보스 체력바 ----------
	/** 이 보스로 체력바를 만든다. 이미 있으면 먼저 내린다 */
	void ShowBossHealthBar(ALOABossCharacter* Boss);

	/** 체력바를 내린다. 없으면 아무것도 안 함 (몇 번 불러도 안전) */
	void HideBossHealthBar();
};
