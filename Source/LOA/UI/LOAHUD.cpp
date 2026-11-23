// LOAHUD.cpp

#include "UI/LOAHUD.h"
#include "UI/LOAUserWidget.h"
#include "UI/LOAOverlayWidgetController.h"
#include "UI/LOABossWidgetController.h"
#include "Game/LOARaidSubsystem.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "UI/Combat/CombatText/LOACombatTextLayer.h"
#include "UI/Status/LOAStatusTracker.h"
#include "UI/Combat/LOAJustGuardHoldBar.h"
#include "UI/Raid/LOARaidResultWidget.h"
#include "UI/Raid/LOABrawlBar.h"
#include "UI/Raid/LOACoopJustGuardResultWidget.h"
#include "Actors/LOABrawlArena.h"
#include "Game/LOAGameState.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

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

	// ⑤ 레이드 단계 구독 (헤더 [레이드 결과]). 이미 클리어면 바로
	if (ALOAGameState* LOAGameState = GetWorld() ? GetWorld()->GetGameState<ALOAGameState>() : nullptr)
	{
		RaidGameState = LOAGameState;
		RaidPhaseChangedHandle = LOAGameState->OnRaidPhaseChanged.AddUObject(this, &ThisClass::HandleRaidPhaseChanged);
		HandleRaidPhaseChanged(LOAGameState->GetRaidPhase());
	}

	// ⑥ 버프 · 디버프 글자 (헤더 [전투 글자], 2026-11-16): 내 ASC를 보는 Tracker. 지금 붙어 있는 것은 띄우지 않는다
	MyASC = InASC;
	MyStatusTracker = NewObject<ULOAStatusTracker>(this);
	MyStatusTracker->Init(InASC);
	MyStatusTracker->OnStatusTagAdded.AddUObject(this, &ThisClass::HandleMyStatusTagAdded);
}

void ALOAHUD::HandleMyStatusTagAdded(const FGameplayTag& StatusTag)
{
	// 버프 · 디버프 · 상태 이상 (사용자 2026-11-16 apply117). 피격 이상(Push)·경직(Paralysis)은 띄우지 않음
	FLOACombatTextInfo Info;
	if (StatusTag.MatchesTag(LOAGameplayTags::State_Buff))
	{
		Info.Kind = ELOACombatTextKind::Buff;
	}
	else if (StatusTag.MatchesTag(LOAGameplayTags::State_Debuff))
	{
		Info.Kind = ELOACombatTextKind::Debuff;
	}
	else if (StatusTag.MatchesTag(LOAGameplayTags::State_CrowdControl_Status))
	{
		Info.Kind = ELOACombatTextKind::Status;
	}
	else
	{
		return;
	}
	Info.StatusTag = StatusTag;

	// 글자를 띄울 몸 = 내 ASC의 지금 아바타 (이름은 레이어가 상태 정보 표에서 찾는다)
	const UAbilitySystemComponent* ASC = MyASC.Get();
	ShowCombatText(ASC ? ASC->GetAvatarActor() : nullptr, Info);
}

// =============================================================================
// 레이드 결과
// =============================================================================

void ALOAHUD::HandleRaidPhaseChanged(ELOARaidPhase NewPhase)
{
	if (NewPhase == ELOARaidPhase::Cleared)
	{
		ShowRaidResult();
	}
}

void ALOAHUD::ShowRaidResult()
{
	if (RaidResultWidget)
	{
		return;
	}

	const ALOAGameState* LOAGameState = RaidGameState.Get();
	if (!LOAGameState || !LOAGameState->GetClearResult().IsValid())
	{
		LOA_LOG(this, Warning, TEXT("[HUD] Raid cleared, but there is no clear result."));
		return;
	}

	if (!RaidResultWidgetClass)
	{
		LOA_LOG(this, Warning, TEXT("[HUD] RaidResultWidgetClass is not set on %s. Clear screen disabled."), *GetNameSafe(GetClass()));
		return;
	}

	RaidResultWidget = CreateWidget<ULOARaidResultWidget>(GetOwningPlayerController(), RaidResultWidgetClass);
	if (!RaidResultWidget)
	{
		return;
	}

	RaidResultWidget->ShowResult(LOAGameState->GetClearResult());
	RaidResultWidget->AddToViewport(RaidResultZOrder);

	LOA_LOG(this, Log, TEXT("[HUD] Raid result shown."));
}

void ALOAHUD::BeginPlay()
{
	Super::BeginPlay();

	// ---------- 전투 글자 레이어 ----------
	if (!CombatTextLayerClass)
	{
		LOA_LOG(this, Warning, TEXT("[HUD] CombatTextLayerClass is not set on %s. Combat text disabled."), *GetNameSafe(GetClass()));
	}
	else
	{
		// HUD는 본인 컴퓨터에만 있으므로 주인 PC는 항상 이 화면의 주인
		CombatTextLayer = CreateWidget<ULOACombatTextLayer>(GetOwningPlayerController(), CombatTextLayerClass);
		if (CombatTextLayer)
		{
			CombatTextLayer->AddToViewport(CombatTextLayerZOrder);
		}
	}

	// ---------- 홀딩 저스트 가드 게이지 ----------
	// 붙여 두기만 한다. 보일지는 위젯이 내 장부를 보고 정한다
	if (JustGuardHoldBarClass)
	{
		JustGuardHoldBar = CreateWidget<ULOAJustGuardHoldBar>(GetOwningPlayerController(), JustGuardHoldBarClass);
		if (JustGuardHoldBar)
		{
			JustGuardHoldBar->AddToViewport();
		}
	}

	// ---------- 대난투 UI (헤더 [대난투 UI]) ----------
	// 붙여 두기만 한다. 보일지는 위젯이 경기장을 보고 정한다
	if (BrawlBarClass)
	{
		BrawlBar = CreateWidget<ULOABrawlBar>(GetOwningPlayerController(), BrawlBarClass);
		if (BrawlBar)
		{
			BrawlBar->AddToViewport();
		}
	}

	// ---------- 협동 저가 결과 (헤더 [협동 저가 결과], apply128) ----------
	// 붙여 두기만 한다. 언제·어디 띄울지는 위젯이 경기장·보스를 보고 정한다. 전투 글자처럼 스킬바 아래
	if (CoopJustGuardResultClass)
	{
		CoopJustGuardResult = CreateWidget<ULOACoopJustGuardResultWidget>(GetOwningPlayerController(), CoopJustGuardResultClass);
		if (CoopJustGuardResult)
		{
			CoopJustGuardResult->AddToViewport(CombatTextLayerZOrder);
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

		// 대난투 경기장도 같은 방식 (대난투 중 보스 체력바 숨김)
		BrawlArenaChangedHandle = Raid->OnBrawlArenaChanged.AddUObject(this, &ThisClass::HandleBrawlArenaChanged);
		WatchBrawlArena(Raid->GetBrawlArena());
	}
}

void ALOAHUD::ShowCombatText(AActor* Target, const FLOACombatTextInfo& Info)
{
	if (CombatTextLayer)
	{
		CombatTextLayer->ShowText(Target, Info);
	}
}

void ALOAHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->OnBossChanged.RemoveDynamic(this, &ThisClass::HandleBossChanged);
		Raid->OnBrawlArenaChanged.Remove(BrawlArenaChangedHandle);
	}
	BrawlArenaChangedHandle.Reset();
	WatchBrawlArena(nullptr);
	HideBossHealthBar();

	if (BrawlBar)
	{
		BrawlBar->RemoveFromParent();
		BrawlBar = nullptr;
	}

	if (CoopJustGuardResult)
	{
		CoopJustGuardResult->RemoveFromParent();
		CoopJustGuardResult = nullptr;
	}

	if (ALOAGameState* LOAGameState = RaidGameState.Get())
	{
		LOAGameState->OnRaidPhaseChanged.Remove(RaidPhaseChangedHandle);
	}
	RaidGameState.Reset();

	if (RaidResultWidget)
	{
		RaidResultWidget->RemoveFromParent();
		RaidResultWidget = nullptr;
	}

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

	if (CombatTextLayer)
	{
		CombatTextLayer->RemoveFromParent();
		CombatTextLayer = nullptr;
	}

	if (MyStatusTracker)
	{
		MyStatusTracker->Shutdown();
		MyStatusTracker = nullptr;
	}
	MyASC.Reset();

	if (JustGuardHoldBar)
	{
		JustGuardHoldBar->RemoveFromParent();
		JustGuardHoldBar = nullptr;
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

	// ⑤ 대난투 중에 새로 띄웠으면 바로 숨긴다 (헤더 [대난투 UI])
	bBossBarHiddenForBrawl = false;
	UpdateBossBarForBrawl();

	LOA_LOG(this, Log, TEXT("[HUD] Boss health bar shown for %s."), *GetNameSafe(Boss));
}

// =============================================================================
// 대난투 UI (헤더 [대난투 UI])
// =============================================================================

void ALOAHUD::HandleBrawlArenaChanged(ALOABrawlArena* Arena)
{
	WatchBrawlArena(Arena);
}

void ALOAHUD::WatchBrawlArena(ALOABrawlArena* Arena)
{
	// 옛 경기장 알림부터 뗀다
	if (ALOABrawlArena* OldArena = WatchedBrawlArena.Get())
	{
		OldArena->OnPhaseChanged.Remove(BrawlPhaseChangedHandle);
	}
	BrawlPhaseChangedHandle.Reset();
	WatchedBrawlArena = Arena;

	if (Arena)
	{
		BrawlPhaseChangedHandle = Arena->OnPhaseChanged.AddUObject(this, &ThisClass::HandleBrawlPhaseChanged);
	}
	UpdateBossBarForBrawl();
}

void ALOAHUD::HandleBrawlPhaseChanged(ALOABrawlArena* Arena)
{
	UpdateBossBarForBrawl();
}

void ALOAHUD::UpdateBossBarForBrawl()
{
	if (!BossHealthBarWidget)
	{
		return;
	}

	const ALOABrawlArena* Arena = WatchedBrawlArena.Get();
	const bool bHide = Arena && Arena->ShouldShowBrawlUI();
	if (bHide == bBossBarHiddenForBrawl)
	{
		return;
	}
	bBossBarHiddenForBrawl = bHide;

	// 숨길 때 원래 보이기 값을 적어 두고 되돌린다 (상태 아이콘 툴팁 때문에 클릭 통과 여부를 바꾸지 않는다)
	if (bHide)
	{
		BossBarVisibilityBeforeBrawl = BossHealthBarWidget->GetVisibility();
		BossHealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		BossHealthBarWidget->SetVisibility(BossBarVisibilityBeforeBrawl);
	}
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
