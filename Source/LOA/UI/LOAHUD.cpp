// LOAHUD.cpp

#include "UI/LOAHUD.h"
#include "UI/LOAUserWidget.h"
#include "UI/LOAOverlayWidgetController.h"
#include "UI/LOABossWidgetController.h"
#include "Game/LOARaidSubsystem.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "UI/Combat/DamageNumber/LOADamageNumberLayer.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAHUD)

void ALOAHUD::InitOverlay(APlayerController* InPlayerController, APlayerState* InPlayerState, UAbilitySystemComponent* InASC)
{
	// 이미 만들었으면 끝. (부활해도 ASC는 그대로라 다시 만들 필요 없음)
	if (OverlayWidget)
	{
		return;
	}

	if (!InPlayerController || !InASC)
	{
		LOA_LOG(this, Warning, TEXT("[HUD] InitOverlay: PlayerController or ASC is null."));
		return;
	}

	if (!OverlayWidgetClass)
	{
		LOA_LOG(this, Error, TEXT("[HUD] OverlayWidgetClass is not set on %s."), *GetNameSafe(GetClass()));
		return;
	}

	// ① 컨트롤러 생성 → ASC에 알림 걸기
	//    BP에서 클래스를 비워 두면 기본 C++ 클래스를 쓴다.
	UClass* ControllerClass = OverlayWidgetControllerClass
		? OverlayWidgetControllerClass.Get()
		: ULOAOverlayWidgetController::StaticClass();

	OverlayWidgetController = NewObject<ULOAOverlayWidgetController>(this, ControllerClass);
	OverlayWidgetController->Init(InPlayerController, InPlayerState, InASC);

	// ② 위젯 생성 → 컨트롤러 넣기 (BP가 OnWidgetControllerSet에서 알림에 바인딩)
	OverlayWidget = CreateWidget<ULOAUserWidget>(InPlayerController, OverlayWidgetClass);
	if (!OverlayWidget)
	{
		LOA_LOG(this, Error, TEXT("[HUD] Failed to create overlay widget."));
		OverlayWidgetController->Shutdown();
		OverlayWidgetController = nullptr;
		return;
	}
	OverlayWidget->SetWidgetController(OverlayWidgetController);

	// ③ 바인딩이 끝났으니 지금 값을 한 번 보낸다. (이 줄이 없으면 처음엔 빈 바로 보이다가 값이 바뀔 때 채워짐)
	OverlayWidgetController->BroadcastInitialValues();

	// ④ 화면에 붙이기
	OverlayWidget->AddToViewport();

	LOA_LOG(this, Log, TEXT("[HUD] Overlay initialized."));
}

void ALOAHUD::BeginPlay()
{
	Super::BeginPlay();

	// ---------- 데미지 숫자 레이어 ----------
	if (!DamageNumberLayerClass)
	{
		LOA_LOG(this, Warning, TEXT("[HUD] DamageNumberLayerClass is not set on %s. Damage numbers disabled."), *GetNameSafe(GetClass()));
	}
	else
	{
		// HUD는 본인 컴퓨터에만 있으므로 주인 PC는 항상 이 화면의 주인
		DamageNumberLayer = CreateWidget<ULOADamageNumberLayer>(GetOwningPlayerController(), DamageNumberLayerClass);
		if (DamageNumberLayer)
		{
			DamageNumberLayer->AddToViewport(DamageNumberLayerZOrder);
		}
	}

	// ---------- 보스 체력바 ----------
	// 상황판을 구독하고, 이미 등록된 보스가 있으면 바로 띄운다. (보스 BeginPlay가 HUD보다 먼저 끝났을 수 있다)
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->OnBossChanged.AddDynamic(this, &ThisClass::HandleBossChanged);

		if (ALOABossCharacter* Boss = Raid->GetCurrentBoss())
		{
			ShowBossHealthBar(Boss);
		}
	}
}

void ALOAHUD::ShowDamageNumber(AActor* Target, const FLOADamageNumberInfo& Info)
{
	if (DamageNumberLayer)
	{
		DamageNumberLayer->ShowDamage(Target, Info);
	}
}

void ALOAHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->OnBossChanged.RemoveDynamic(this, &ThisClass::HandleBossChanged);
	}
	HideBossHealthBar();

	if (OverlayWidgetController)
	{
		OverlayWidgetController->Shutdown();
		OverlayWidgetController = nullptr;
	}

	if (OverlayWidget)
	{
		OverlayWidget->RemoveFromParent();
		OverlayWidget = nullptr;
	}

	if (DamageNumberLayer)
	{
		DamageNumberLayer->RemoveFromParent();
		DamageNumberLayer = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// 보스 체력바
// =============================================================================

void ALOAHUD::HandleBossChanged(ALOABossCharacter* NewBoss)
{
	if (NewBoss)
	{
		ShowBossHealthBar(NewBoss);
	}
	else
	{
		HideBossHealthBar();
	}
}

void ALOAHUD::ShowBossHealthBar(ALOABossCharacter* Boss)
{
	// 보스가 바뀌는 경우(교체) 예전 체력바부터 내린다.
	HideBossHealthBar();

	if (!Boss)
	{
		return;
	}

	if (!BossHealthBarWidgetClass)
	{
		LOA_LOG(this, Warning, TEXT("[HUD] BossHealthBarWidgetClass is not set on %s. Boss health bar disabled."), *GetNameSafe(GetClass()));
		return;
	}

	// ① 컨트롤러 생성 → 보스 ASC에 체력 알림 걸기. BP에서 클래스를 비워 두면 기본 C++ 클래스를 쓴다.
	UClass* ControllerClass = BossWidgetControllerClass
		? BossWidgetControllerClass.Get()
		: ULOABossWidgetController::StaticClass();

	BossWidgetController = NewObject<ULOABossWidgetController>(this, ControllerClass);
	BossWidgetController->Init(Boss);

	// ② 위젯 생성 → 컨트롤러 넣기 (BP가 OnWidgetControllerSet에서 알림에 바인딩)
	BossHealthBarWidget = CreateWidget<ULOAUserWidget>(GetOwningPlayerController(), BossHealthBarWidgetClass);
	if (!BossHealthBarWidget)
	{
		LOA_LOG(this, Error, TEXT("[HUD] Failed to create boss health bar widget."));
		BossWidgetController->Shutdown();
		BossWidgetController = nullptr;
		return;
	}
	BossHealthBarWidget->SetWidgetController(BossWidgetController);

	// ③ 바인딩이 끝났으니 이름/줄 수 + 지금 체력을 한 번 보낸다.
	BossWidgetController->BroadcastInitialValues();

	// ④ 화면에 붙이기. 위치(화면 위쪽 가운데 등)는 위젯 BP가 정한다.
	BossHealthBarWidget->AddToViewport();

	LOA_LOG(this, Log, TEXT("[HUD] Boss health bar shown for %s."), *GetNameSafe(Boss));
}

void ALOAHUD::HideBossHealthBar()
{
	if (BossWidgetController)
	{
		BossWidgetController->Shutdown();
		BossWidgetController = nullptr;
	}

	if (BossHealthBarWidget)
	{
		BossHealthBarWidget->RemoveFromParent();
		BossHealthBarWidget = nullptr;
	}
}
