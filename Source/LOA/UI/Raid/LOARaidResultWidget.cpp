// LOARaidResultWidget.cpp

#include "UI/Raid/LOARaidResultWidget.h"
#include "UI/Raid/LOARaidResultCardWidget.h"
#include "Core/LOALog.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOARaidResultWidget)

void ULOARaidResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 결과 화면에서 Esc를 받으려면 키보드 초점을 받을 수 있어야 한다
	SetIsFocusable(true);

	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddDynamic(this, &ThisClass::Leave);
	}
}

void ULOARaidResultWidget::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BannerTimerHandle);
	}

	Super::NativeDestruct();
}

void ULOARaidResultWidget::ShowResult(const FLOARaidClearResult& Result)
{
	// ---------- ① 배너 ----------
	// 걸린 시간. 예) 222.4초 → 3분 42초 (초는 두 자리: 3분 05초)
	const int32 TotalSeconds = FMath::FloorToInt(Result.ClearSeconds);
	FNumberFormattingOptions TwoDigits;
	TwoDigits.SetMinimumIntegralDigits(2);
	const FText Format = ClearTimeFormat.IsEmpty() ? INVTEXT("{Minutes}:{Seconds}") : ClearTimeFormat;
	FFormatNamedArguments Args;
	Args.Add(TEXT("Minutes"), FText::AsNumber(TotalSeconds / 60));
	Args.Add(TEXT("Seconds"), FText::AsNumber(TotalSeconds % 60, &TwoDigits));
	if (ClearTimeText)
	{
		ClearTimeText->SetText(FText::Format(Format, Args));
	}

	if (BossNameText)
	{
		BossNameText->SetText(Result.BossName);
	}

	if (BossPortraitImage)
	{
		if (Result.BossPortrait.IsNull())
		{
			BossPortraitImage->SetVisibility(ESlateVisibility::Hidden);	// 보스 그림이 정해지기 전 (사용자: 보스 에셋 미확정)
		}
		else
		{
			BossPortraitImage->SetBrushFromSoftTexture(Result.BossPortrait);
			BossPortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// ---------- ② 결과 화면 카드 (지금은 접힌 채로 채워 둔다) ----------
	if (MvpCard && Result.Players.IsValidIndex(Result.MvpIndex))
	{
		MvpCard->Setup(Result.Players[Result.MvpIndex]);
	}

	if (OtherCardsBox)
	{
		OtherCardsBox->ClearChildren();

		if (!OtherCardClass)
		{
			LOA_LOG(GetOwningPlayer(), Warning, TEXT("[RaidResult] OtherCardClass is not set on %s."), *GetNameSafe(GetClass()));
		}
		else
		{
			for (int32 Index = 0; Index < Result.Players.Num(); ++Index)
			{
				if (Index == Result.MvpIndex)
				{
					continue;
				}

				if (ULOARaidResultCardWidget* Card = CreateWidget<ULOARaidResultCardWidget>(GetOwningPlayer(), OtherCardClass))
				{
					Card->Setup(Result.Players[Index]);
					OtherCardsBox->AddChild(Card);
				}
			}
		}
	}

	// ---------- 배너부터 ----------
	bResultPanelShown = false;
	if (BannerPanel)
	{
		BannerPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	OnBannerShown();

	if (const UWorld* World = GetWorld())
	{
		// 0초 타이머는 예약되지 않으므로 최소 0.01초
		World->GetTimerManager().SetTimer(BannerTimerHandle, this, &ThisClass::ShowResultPanel, FMath::Max(BannerSeconds, 0.01f), false);
	}
}

void ULOARaidResultWidget::ShowResultPanel()
{
	if (BannerPanel)
	{
		BannerPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	bResultPanelShown = true;

	// 입력 = UI만 (게임 입력 막힘, 대신 정함). 초점을 이 위젯에 → Esc가 NativeOnKeyDown으로 온다
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}

	OnResultPanelShown();
}

FReply ULOARaidResultWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bResultPanelShown && InKeyEvent.GetKey() == EKeys::Escape)
	{
		Leave();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void ULOARaidResultWidget::Leave()
{
	// 헤더 [로비로 가는 길이 하나인 이유]. 리슨 방장이면 방이 닫혀 남은 사람도 엔진이 같은 맵으로 보낸다
	const FString LobbyMap = UGameMapsSettings::GetGameDefaultMap();
	LOA_LOG(GetOwningPlayer(), Log, TEXT("[RaidResult] Leaving to %s."), *LobbyMap);
	UGameplayStatics::OpenLevel(this, FName(*LobbyMap));
}
