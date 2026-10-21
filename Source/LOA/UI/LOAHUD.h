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
// [데미지 숫자 - ShowDamageNumber]
// 내가 때린 데미지를 대상 머리 위에 띄운다. HUD는 넘겨주기만 하고, 풀/위치/수명은 레이어가 담당한다.
//   서버(데미지 확정) → ULOACombatRPCComponent::ClientShowDamageNumber (PC에 붙은 컴포넌트) → ShowDamageNumber → ULOADamageNumberLayer
//
// 레이어는 BeginPlay에서 만든다. (ASC와 상관없는 위젯이라 InitOverlay를 기다릴 필요 없음)
// Overlay보다 아래에 그린다. (스킬바가 숫자에 가려지지 않게)
//
// ─────────────────────────────────────────────────────────────────────────────
// [보스 체력바]
// 레이드 상황판(ULOARaidSubsystem)의 "지금 보스가 바뀜" 알림을 BeginPlay에서 구독한다.
//   OnBossChanged(보스)    → ShowBossHealthBar : 보스 위젯 컨트롤러 생성 → Init(보스) → 위젯 생성 → 연결 → 첫 값 → 화면에
//   OnBossChanged(nullptr) → HideBossHealthBar : 컨트롤러 Shutdown → 위젯 제거
// BeginPlay 때 이미 보스가 등록돼 있으면(보스 BeginPlay가 먼저 끝난 경우) 바로 띄운다. → 순서가 어느 쪽이든 동작
// InitOverlay(내 캐릭터 준비)를 기다리지 않는다. 보스 체력바는 내 ASC와 상관없다.
//
// [BP에서 설정할 것] BP_LOAHUD (부모: LOAHUD)
//   Damage Number Layer Class       = WBP_DamageNumberLayer
//   Overlay Widget Class            = WBP_Overlay
//   Overlay Widget Controller Class = LOAOverlayWidgetController (BP 자식을 만들었다면 그것)
//   Boss Health Bar Widget Class    = WBP_BossHealthBar (비워 두면 보스 체력바 없음)
//   Boss Widget Controller Class    = 비워 두면 LOABossWidgetController
// 그리고 BP_LOAGameMode의 HUD Class = BP_LOAHUD

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/Combat/DamageNumber/LOADamageNumberTypes.h"
#include "LOAHUD.generated.h"

class APlayerController;
class APlayerState;
class UAbilitySystemComponent;
class ULOAUserWidget;
class ULOAOverlayWidgetController;
class ULOADamageNumberLayer;
class ULOABossWidgetController;
class ALOABossCharacter;

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

	/** 대상 머리 위에 데미지 숫자를 띄운다. [본인 컴퓨터] 레이어에 넘기기만 한다. */
	void ShowDamageNumber(AActor* Target, const FLOADamageNumberInfo& Info);

protected:
	//~ Begin AActor Interface
	/** 데미지 숫자 레이어 생성 (숫자 위젯 미리 만들기 포함) + 레이드 상황판 구독 (보스 체력바) */
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

	/** 데미지 숫자 레이어 (WBP_DamageNumberLayer). 비어 있으면 숫자를 띄우지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|DamageNumber")
	TSubclassOf<ULOADamageNumberLayer> DamageNumberLayerClass;

	/** 레이어의 그리기 순서. Overlay(0)보다 작아야 스킬바 아래에 그려진다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|DamageNumber")
	int32 DamageNumberLayerZOrder = -1;

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
	TObjectPtr<ULOADamageNumberLayer> DamageNumberLayer;

	UPROPERTY(Transient)
	TObjectPtr<ULOAUserWidget> BossHealthBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULOABossWidgetController> BossWidgetController;

	// ---------- 보스 체력바 ----------
	/** 이 보스로 체력바를 만든다. 이미 있으면 먼저 내린다 */
	void ShowBossHealthBar(ALOABossCharacter* Boss);

	/** 체력바를 내린다. 없으면 아무것도 안 함 (몇 번 불러도 안전) */
	void HideBossHealthBar();
};
