// LOAPlayerController.cpp

#include "Player/LOAPlayerController.h"
#include "Player/LOAPlayerState.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "Input/LOAInputConfig.h"
#include "Input/LOAClickMoveComponent.h"
#include "Combat/LOACombatRPCComponent.h"
#include "Character/Player/LOACharacter.h"
#include "UI/LOAHUD.h"
#include "Core/LOALog.h"
#include "Debug/LOACheatManager.h"
#include "Player/LOASpectateComponent.h"
#include "Game/LOAGameState.h"
#include "Game/LOARaidSubsystem.h"
#include "Actors/LOABrawlArena.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/HUD.h"
#include "Camera/PlayerCameraManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPlayerController)

ALOAPlayerController::ALOAPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 탑쿼터뷰: 마우스 커서를 항상 보이게 (이동 클릭, 스킬 조준에 사용)
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	// 클릭 이동 부품. 모든 PlayerController에 붙지만, 실제로 켜지는 건 본인 컴퓨터(SetupInputComponent)에서만이다.
	// 꺼져 있는 동안은 Tick도 입력도 없으므로 서버 쪽 비용은 없다.
	ClickMoveComponent = CreateDefaultSubobject<ULOAClickMoveComponent>(TEXT("ClickMoveComponent"));

	// 전투 RPC 컴포넌트. 서버의 PC와 본인 컴퓨터의 PC에 모두 있어야 RPC가 오간다. (복제는 컴포넌트가 직접 켬)
	CombatRPCComponent = CreateDefaultSubobject<ULOACombatRPCComponent>(TEXT("CombatRPCComponent"));

	// 관전 부품. 클릭 이동처럼 모든 PlayerController에 붙지만 본인 컴퓨터에서만 켠다 (SetupInputComponent)
	SpectateComponent = CreateDefaultSubobject<ULOASpectateComponent>(TEXT("SpectateComponent"));

	// 치트 매니저 (개발 빌드의 디버그 명령). 창 위젯을 지정하려면 BP에서 Cheat Class를 B_LOACheatManager로 바꾼다.
	CheatClass = ULOACheatManager::StaticClass();
}

void ALOAPlayerController::AddCheats(bool bForce)
{
#if UE_WITH_CHEAT_MANAGER
	// 개발 빌드: 클라 PC도 치트 매니저를 갖는다. (명령을 받아 ServerCheat로 서버에 보내려고)
	Super::AddCheats(true);
#else
	Super::AddCheats(bForce);
#endif
}

void ALOAPlayerController::ServerCheat_Implementation(const FString& Command)
{
#if UE_WITH_CHEAT_MANAGER
	// 서버에 있는 이 PC의 치트 매니저로 실행 → ULOACheatManager의 Exec 함수가 서버 권한으로 돈다.
	if (CheatManager)
	{
		LOA_LOG(this, Display, TEXT("[Cheat] %s from client: %s"), *GetNameSafe(this), *Command);
		ConsoleCommand(Command);
	}
#endif
}

void ALOAPlayerController::ClientCheat_Implementation(const FString& Command)
{
#if UE_WITH_CHEAT_MANAGER
	// 이 컴퓨터의 이 PC 치트 매니저(와 그 확장)로 실행
	if (CheatManager)
	{
		LOA_LOG(this, Display, TEXT("[Cheat] %s from server: %s"), *GetNameSafe(this), *Command);
		ConsoleCommand(Command);
	}
#endif
}

void ALOAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		// 게임 입력 + UI 입력을 모두 받는 모드.
		// 기본 설정에서는 마우스 버튼을 누르는 동안 커서가 숨겨지므로, 우클릭 이동 중에도 커서가 보이도록 끈다.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void ALOAPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateTransferFade();
}

void ALOAPlayerController::UpdateTransferFade()
{
	if (!IsLocalController() || !PlayerCameraManager)
	{
		return;
	}

	const ALOAGameState* LOAGameState = GetWorld() ? GetWorld()->GetGameState<ALOAGameState>() : nullptr;
	float Alpha = LOAGameState ? LOAGameState->GetTransferFadeAlpha() : 0.f;

	// 대난투 차원 이동 (2026-11-18 apply122): 어둡기는 큰 쪽, 줌인은 내 캐릭터 카메라
	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this);
	if (const ALOABrawlArena* Arena = Raid ? Raid->GetBrawlArena() : nullptr)
	{
		Alpha = FMath::Max(Alpha, Arena->GetFadeAlpha());

		if (ALOACharacter* MyCharacter = GetPawn<ALOACharacter>())
		{
			MyCharacter->SetCameraZoomScale(Arena->GetCameraZoomScale());
		}
	}

	if (Alpha > 0.f)
	{
		// 서버 시각으로 계산한 값을 그대로 칠한다 (엔진의 자동 애니메이션은 쓰지 않음)
		PlayerCameraManager->SetManualCameraFade(Alpha, FLinearColor::Black, /*bFadeAudio*/ false);
		bTransferFadeActive = true;
	}
	else if (bTransferFadeActive)
	{
		// 끄면 엔진이 화면에 칠하지 않는다 (ULocalPlayer::CalcSceneView가 bEnableFading일 때만 칠함, 엔진 코드 확인)
		PlayerCameraManager->StopCameraFade();
		bTransferFadeActive = false;
	}
}

void ALOAPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 프로젝트 설정 > Input > Default Input Component Class 가 EnhancedInputComponent여야 한다. (5.x 템플릿 기본값)
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		LOA_LOG(this, Error, TEXT("InputComponent is not UEnhancedInputComponent. Check Project Settings > Input."));
		return;
	}

	if (!InputConfig)
	{
		LOA_LOG(this, Error, TEXT("InputConfig is not set on %s."), *GetNameSafe(GetClass()));
		return;
	}

	// ---------- 1) 스킬 입력 ----------

	// 스킬 키 설정(IMC) 등록
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (AbilityIMC)
			{
				Subsystem->AddMappingContext(AbilityIMC, AbilityIMCPriority);
			}
		}
	}

	// InputConfig에 적힌 스킬 입력 액션 전부를 "태그를 들고" 바인딩한다.
	// → IA_Skill_Q를 누르면 Input_AbilityTagPressed(InputTag.Skill.Q)가 호출된다.
	// 스킬 슬롯이 늘어나도 InputConfig에 한 줄 추가하면 되고, 이 코드는 바뀌지 않는다.
	for (const FLOAInputAction& Entry : InputConfig->GetAbilityInputActions())
	{
		if (!Entry.InputAction || !Entry.InputTag.IsValid())
		{
			continue;
		}

		// Started   : 누른 순간 1회
		// Completed : 뗀 순간 1회
		// (누르고 있는 동안 매 프레임 오는 Triggered는 쓰지 않는다. 쓰면 발동 요청이 매 프레임 나간다)
		EnhancedInput->BindAction(Entry.InputAction, ETriggerEvent::Started, this, &ThisClass::Input_AbilityTagPressed, Entry.InputTag);
		EnhancedInput->BindAction(Entry.InputAction, ETriggerEvent::Completed, this, &ThisClass::Input_AbilityTagReleased, Entry.InputTag);
	}

	// ---------- 2) 이동 입력 ----------
	if (ClickMoveComponent)
	{
		ClickMoveComponent->ActivateClickMove(EnhancedInput, InputConfig);
	}

	// ---------- 3) 관전 입력 (← / →) ----------
	if (SpectateComponent)
	{
		SpectateComponent->ActivateSpectate(EnhancedInput);
	}
}

// =============================================================================
// 부활
// =============================================================================

void ALOAPlayerController::RequestRevive()
{
	// 리슨 호스트 본인이면 RPC가 그냥 함수 호출이 된다.
	ServerRequestRevive();
}

void ALOAPlayerController::ServerRequestRevive_Implementation()
{
	// ① 내 캐릭터가 죽어 있나 (살아 있는데 목숨을 쓰면 안 됨)
	ALOACharacter* MyCharacter = GetPawn<ALOACharacter>();
	if (!MyCharacter || !MyCharacter->IsDead())
	{
		LOA_LOG(this, Verbose, TEXT("[Revive] Request ignored: not dead."));
		return;
	}

	// ② 공용 목숨을 하나 쓸 수 있나 (없으면 끝. 버튼은 위젯이 꺼 두지만 서버가 한 번 더 막는다)
	ALOAGameState* LOAGameState = GetWorld() ? GetWorld()->GetGameState<ALOAGameState>() : nullptr;
	if (!LOAGameState || !LOAGameState->TryConsumeReviveLife())
	{
		LOA_LOG(this, Log, TEXT("[Revive] Request denied: no revive life."));
		return;
	}

	// ③ 부활 (죽은 자리, 체력 가득, 무적)
	MyCharacter->ReviveFromDeath();
}

// =============================================================================
// 스킬 입력 → ASC
// =============================================================================

ULOAAbilitySystemComponent* ALOAPlayerController::GetReadyAbilitySystemComponent() const
{
	const ALOAPlayerState* LOAPlayerState = GetPlayerState<ALOAPlayerState>();
	ULOAAbilitySystemComponent* ASC = LOAPlayerState ? LOAPlayerState->GetLOAAbilitySystemComponent() : nullptr;

	// ASC에 "지금 조종 중인 캐릭터"가 몸으로 등록된 뒤에만 입력을 넘긴다.
	// (스폰 직후 캐릭터 초기화가 끝나기 전, 또는 사망 후 몸이 없을 때 입력을 무시)
	if (!ASC || !GetPawn() || ASC->GetAvatarActor() != GetPawn())
	{
		return nullptr;
	}

	return ASC;
}

void ALOAPlayerController::Input_AbilityTagPressed(FGameplayTag InputTag)
{
	if (ULOAAbilitySystemComponent* ASC = GetReadyAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ALOAPlayerController::Input_AbilityTagReleased(FGameplayTag InputTag)
{
	if (ULOAAbilitySystemComponent* ASC = GetReadyAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

// =============================================================================
// HUD 초기화
// =============================================================================

void ALOAPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	// HUD는 본인 컴퓨터에만 있다. 서버의 다른 플레이어용 PlayerController는 할 일이 없다.
	if (!IsLocalController())
	{
		return;
	}

	// 이전 캐릭터를 기다리던 중이었다면 그만 기다린다. (같은 캐릭터로 다시 불려도 아래에서 다시 건다)
	StopWaitingForCharacter();

	ALOACharacter* LOACharacter = Cast<ALOACharacter>(InPawn);
	if (!LOACharacter)
	{
		return;   // 몸이 없어짐(사망 등) 또는 우리 캐릭터가 아님
	}

	// HUD 경로 (가)
	if (LOACharacter->IsCharacterInitialized())
	{
		// 캐릭터가 먼저 준비됨 → 바로 시도
		TryInitHUD();
	}
	else
	{
		// 캐릭터가 아직 준비 전 → 준비 완료 알림을 기다린다. (클라이언트에서 직업 데이터/PlayerState가 늦게 오는 경우)
		LOACharacter->OnCharacterInitialized.AddUniqueDynamic(this, &ThisClass::OnCharacterReadyForHUD);
		PendingCharacter = LOACharacter;
	}
}

void ALOAPlayerController::ClientSetHUD_Implementation(TSubclassOf<AHUD> NewHUDClass)
{
	// Super 안에서 이전 HUD를 지우고 새 HUD를 만든다.
	Super::ClientSetHUD_Implementation(NewHUDClass);

	// HUD 경로 (나): HUD가 늦게 생긴 경우 여기서 초기화된다.
	TryInitHUD();
}

void ALOAPlayerController::OnCharacterReadyForHUD(ALOACharacter* ReadyCharacter)
{
	// 한 번만 필요하므로 구독을 끊는다.
	StopWaitingForCharacter();
	TryInitHUD();
}

void ALOAPlayerController::StopWaitingForCharacter()
{
	if (ALOACharacter* OldCharacter = PendingCharacter.Get())
	{
		OldCharacter->OnCharacterInitialized.RemoveDynamic(this, &ThisClass::OnCharacterReadyForHUD);
	}
	PendingCharacter.Reset();
}

void ALOAPlayerController::TryInitHUD()
{
	if (!IsLocalController())
	{
		return;
	}

	// (나) HUD가 있나. GameMode의 HUD Class가 BP_LOAHUD가 아니면 여기서 걸린다.
	ALOAHUD* LOAHUD = GetHUD<ALOAHUD>();
	if (!LOAHUD || LOAHUD->IsOverlayInitialized())
	{
		return;
	}

	// (가) 조종 중인 캐릭터가 준비됐나
	ALOACharacter* LOACharacter = GetPawn<ALOACharacter>();
	if (!LOACharacter || !LOACharacter->IsCharacterInitialized())
	{
		return;
	}

	// PlayerState / ASC는 캐릭터에게서 받는다.
	// 캐릭터가 준비됐다 = 캐릭터가 PlayerState의 ASC에 연결됐다는 뜻이므로 둘 다 확실히 있다.
	// (PlayerController 자신의 PlayerState는 클라이언트에서 따로 복제되어 이 시점에 아직 없을 수 있다)
	APlayerState* MyPlayerState = LOACharacter->GetPlayerState();
	UAbilitySystemComponent* MyASC = LOACharacter->GetAbilitySystemComponent();
	if (!MyPlayerState || !MyASC)
	{
		return;
	}

	LOAHUD->InitOverlay(this, MyPlayerState, MyASC);
}
