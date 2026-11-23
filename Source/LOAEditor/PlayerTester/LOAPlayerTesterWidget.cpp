// LOAPlayerTesterWidget.cpp

#include "PlayerTester/LOAPlayerTesterWidget.h"
#include "Debug/LOACheatManager.h"
#include "Debug/LOAPlayerCheats.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Player.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CheatManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPlayerTesterWidget)

// =============================================================================
// 열기·닫기
// =============================================================================

void ULOAPlayerTesterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 연결은 여기서. NativeOnInitialized는 주인 플레이어가 없는 에디터 창에서는 안 불린다 (엔진 UserWidget.cpp Initialize)
	// 탭을 다시 열면 또 불리므로 AddUnique (같은 연결이 두 번 생기지 않게)
	if (AutoGuardOnButton)
	{
		AutoGuardOnButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAutoGuardOnClicked);
	}
	if (AutoGuardOffButton)
	{
		AutoGuardOffButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAutoGuardOffClicked);
	}
	if (IdentityFillButton)
	{
		IdentityFillButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnIdentityFillClicked);
	}
	if (IdentityEmptyButton)
	{
		IdentityEmptyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnIdentityEmptyClicked);
	}

	Refresh(true);

	// 에디터 타이머 (PIE 월드의 타이머가 아니다 → PIE를 껐다 켜도 계속 돈다)
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &ThisClass::RefreshByTimer), RefreshInterval, true);
	}
}

void ULOAPlayerTesterWidget::NativeDestruct()
{
	if (GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(RefreshTimer);
	}

	Super::NativeDestruct();
}

// =============================================================================
// 다시 그리기
// =============================================================================

void ULOAPlayerTesterWidget::Refresh(bool bForceLog)
{
	FString Diagnostic;
	Players = FindPlayers(Diagnostic);

	// 진단: 숫자가 바뀔 때와 버튼을 누를 때만 (0.5초마다 찍으면 로그가 쌓인다)
	if (bForceLog || Diagnostic != LastDiagnostic)
	{
		UE_LOG(LogLOA, Display, TEXT("[PlayerTester] %s"), *Diagnostic);
		LastDiagnostic = Diagnostic;
	}

	UpdateCombo();
	UpdateStatus();
}

TArray<ULOAPlayerTesterWidget::FTesterPlayer> ULOAPlayerTesterWidget::FindPlayers(FString& OutDiagnostic) const
{
	TArray<FTesterPlayer> Found;
	int32 PIEWorldCount = 0;
	int32 LocalPCCount = 0;
	int32 WithCheatsCount = 0;
	if (!GEngine)
	{
		OutDiagnostic = TEXT("no GEngine");
		return Found;
	}

	// PIE 월드 = 서버(리슨 호스트) 1 + 클라 N. 월드마다 그 화면의 PC만 본다 (서버 월드의 원격 PC는 그 사람 화면이 아니다)
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType != EWorldType::PIE || !World)
		{
			continue;
		}
		++PIEWorldCount;

		const ENetMode NetMode = World->GetNetMode();
		const TCHAR* RoleName = (NetMode == NM_ListenServer) ? TEXT("호스트")
			: (NetMode == NM_Client) ? TEXT("클라") : TEXT("혼자");

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->IsLocalController())
			{
				continue;
			}
			++LocalPCCount;
			if (!PC->CheatManager)
			{
				continue;
			}
			++WithCheatsCount;
			ULOAPlayerCheats* Cheats = PC->CheatManager->FindCheatManagerExtension<ULOAPlayerCheats>();
			if (!Cheats)
			{
				continue;
			}

			FTesterPlayer& Player = Found.AddDefaulted_GetRef();
			Player.Cheats = Cheats;
			Player.Pawn = PC->GetPawn();
			Player.PlayerId = PC->PlayerState ? PC->PlayerState->GetPlayerId() : MAX_int32;
			Player.bHasPawn = (PC->GetPawn() != nullptr);
			Player.Label = FString::Printf(TEXT("%s · %s"), RoleName, *ULOACheatManager::GetPlayerClassName(PC->GetPawn()));
		}
	}

	// 게임 안과 같은 번호 (ULOACheatManager::GetPlayersInJoinOrder: PlayerId 순서)
	Found.Sort([](const FTesterPlayer& A, const FTesterPlayer& B) { return A.PlayerId < B.PlayerId; });
	for (int32 Index = 0; Index < Found.Num(); ++Index)
	{
		Found[Index].Label = FString::Printf(TEXT("P%d (%s)"), Index + 1, *Found[Index].Label);
	}

	// 어느 단계에서 빠지는지 보려고: PIE 월드 → 그 화면의 PC → 치트 매니저 있음 → 확장 있음 (= players)
	OutDiagnostic = FString::Printf(TEXT("PIE worlds %d, local PCs %d, with cheats %d, players %d"),
		PIEWorldCount, LocalPCCount, WithCheatsCount, Found.Num());
	return Found;
}

void ULOAPlayerTesterWidget::UpdateCombo()
{
	if (!PlayerCombo)
	{
		return;
	}

	TArray<FString> NewOptions;
	NewOptions.Add(TEXT("모두"));
	for (const FTesterPlayer& Player : Players)
	{
		NewOptions.Add(Player.Label);
	}

	// 같으면 그대로 (다시 채우면 고르고 있던 콤보 박스가 닫힌다)
	if (NewOptions == ComboOptions)
	{
		return;
	}

	const FString Selected = PlayerCombo->GetSelectedOption();
	PlayerCombo->ClearOptions();
	for (const FString& Option : NewOptions)
	{
		PlayerCombo->AddOption(Option);
	}
	ComboOptions = NewOptions;

	const int32 KeepIndex = NewOptions.IndexOfByKey(Selected);
	PlayerCombo->SetSelectedIndex(KeepIndex != INDEX_NONE ? KeepIndex : 0);
}

void ULOAPlayerTesterWidget::UpdateStatus() const
{
	if (!StatusText)
	{
		return;
	}

	FString Status;
	if (Players.IsEmpty())
	{
		Status = TEXT("PIE 실행 중이 아님 (플레이를 누르면 플레이어가 나온다)");
	}
	else
	{
		for (const FTesterPlayer& Player : Players)
		{
			const ULOAPlayerCheats* Cheats = Player.Cheats.Get();
			const float Delay = Cheats ? Cheats->GetAutoGuardDelay() : -1.f;
			Status += FString::Printf(TEXT("%s: 자동 저가 %s%s%s\n"), *Player.Label,
				Delay >= 0.f ? *FString::Printf(TEXT("켬 %.2f초"), Delay) : TEXT("꺼짐"),
				*GetIdentityText(Player.Pawn.Get()),
				Player.bHasPawn ? TEXT("") : TEXT(" (캐릭터 없음)"));
		}
	}

	// 여러 명으로 띄웠는데 한 프로그램이 아니면 다른 플레이어 창이 여기서 안 보인다
	const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>();
	bool bRunUnderOneProcess = true;
	if (PlaySettings)
	{
		PlaySettings->GetRunUnderOneProcess(bRunUnderOneProcess);
	}
	if (!bRunUnderOneProcess)
	{
		Status += TEXT("\n※ Run Under One Process가 꺼져 있어서 다른 프로그램으로 뜬 플레이어는 여기서 안 보인다 (에디터 설정 Play → Multiplayer Options)");
	}

	StatusText->SetText(FText::FromString(Status.TrimEnd()));
}

// =============================================================================
// 버튼
// =============================================================================

TArray<ULOAPlayerCheats*> ULOAPlayerTesterWidget::GetSelectedCheats() const
{
	TArray<ULOAPlayerCheats*> Result;
	const int32 Selected = PlayerCombo ? PlayerCombo->GetSelectedIndex() : 0;

	// 0번 = 모두, 1번부터 P1 …
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		if (Selected <= 0 || Selected - 1 == Index)
		{
			if (ULOAPlayerCheats* Cheats = Players[Index].Cheats.Get())
			{
				Result.Add(Cheats);
			}
		}
	}
	return Result;
}

void ULOAPlayerTesterWidget::OnAutoGuardOnClicked()
{
	SetAutoGuard(DelaySpin ? FMath::Max(0.f, DelaySpin->GetValue()) : 0.3f);
}

void ULOAPlayerTesterWidget::OnAutoGuardOffClicked()
{
	SetAutoGuard(-1.f);
}

void ULOAPlayerTesterWidget::SetAutoGuard(float Delay)
{
	// 목록이 0.5초 전 것일 수 있다 → 누른 순간 다시 찾고 부른다 (진단 로그도 한 줄)
	Refresh(true);

	// 그 플레이어 화면(월드)의 확장에 바로. 누르기는 원래 그 화면에서 한다 (ULOAPlayerCheats::LOAAutoGuardLocal)
	for (ULOAPlayerCheats* Cheats : GetSelectedCheats())
	{
		Cheats->LOAAutoGuardLocal(Delay);
	}

	UpdateStatus();
}

// =============================================================================
// 아이덴티티 (2026-11-09)
// =============================================================================

void ULOAPlayerTesterWidget::OnIdentityFillClicked()
{
	SetIdentity(-1.f);
}

void ULOAPlayerTesterWidget::OnIdentityEmptyClicked()
{
	SetIdentity(0.f);
}

void ULOAPlayerTesterWidget::SetIdentity(float Value)
{
	Refresh(true);

	// "Me" = 그 확장의 주인. 클라 화면 확장이면 콘솔과 같은 길로 서버에 보낸다 (게이지는 서버만 바꾼다)
	for (ULOAPlayerCheats* Cheats : GetSelectedCheats())
	{
		Cheats->LOAIdentity(Value, TEXT("Me"));
	}

	UpdateStatus();
}

FString ULOAPlayerTesterWidget::GetIdentityText(const APawn* Pawn)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC)
	{
		return FString();
	}

	const float Max = ASC->GetNumericAttribute(ULOAAttributeSet_Player::GetMaxIdentityGaugeAttribute());
	if (Max <= 0.f)
	{
		return FString();	// 게이지가 없는 직업
	}

	return FString::Printf(TEXT(" · 아덴 %.0f/%.0f%s"),
		ASC->GetNumericAttribute(ULOAAttributeSet_Player::GetIdentityGaugeAttribute()), Max,
		ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Identity_Active) ? TEXT(" (켜짐)") : TEXT(""));
}
