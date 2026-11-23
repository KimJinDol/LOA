// LOACoopJustGuardResultWidget.cpp

#include "UI/Raid/LOACoopJustGuardResultWidget.h"
#include "Actors/LOABrawlArena.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "Game/LOARaidSubsystem.h"
#include "Core/LOALog.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACoopJustGuardResultWidget)

ULOACoopJustGuardResultWidget::ULOACoopJustGuardResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 글자 기본 모양 (BP 기본값에서 바꾼다)
	GradeTexts.Add(ELOAJustGuardGrade::Perfect, NSLOCTEXT("LOA", "CoopJustGuardPerfect", "PERFECT"));
	GradeTexts.Add(ELOAJustGuardGrade::Great, NSLOCTEXT("LOA", "CoopJustGuardGreat", "GREAT"));
	GradeTexts.Add(ELOAJustGuardGrade::Bad, NSLOCTEXT("LOA", "CoopJustGuardBad", "BAD"));
	BonusFormat = NSLOCTEXT("LOA", "CoopJustGuardBonusFormat", "+{Seconds}초");
}

void ULOACoopJustGuardResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 움직이려면 루트 캔버스 바로 아래여야 한다
	PanelSlot = Cast<UCanvasPanelSlot>(ResultPanel->Slot);
	if (!PanelSlot)
	{
		LOA_LOG(this, Warning, TEXT("[CoopJustGuardResult] ResultPanel must be a direct child of a Canvas Panel (it will not follow the boss)."));
	}

	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void ULOACoopJustGuardResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this);
	const ALOABrawlArena* Arena = Raid ? Raid->GetBrawlArena() : nullptr;

	if (Arena)
	{
		const FLOACoopJustGuardResult& Result = Arena->GetCoopJustGuardResult();

		// 처음 본 번호는 기억만 (위젯이 생길 때 이미 있던 결과)
		if (LastSeenResultId < 0)
		{
			LastSeenResultId = Result.ResultId;
		}
		else if (Result.ResultId != LastSeenResultId)
		{
			LastSeenResultId = Result.ResultId;
			Target = Raid->GetCurrentBoss();
			ShowResult(Result);
		}
	}

	if (HideTime <= 0.0)
	{
		return;
	}

	// 보여 줄 시간이 지남 → 숨김, 아니면 보스를 따라간다
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now >= HideTime)
	{
		HideResult();
		return;
	}
	UpdatePosition();
}

void ULOACoopJustGuardResultWidget::ShowResult(const FLOACoopJustGuardResult& Result)
{
	// 등급 글자. 예) Great → "GREAT"
	if (GradeText)
	{
		GradeText->SetText(GradeTexts.FindRef(Result.Grade));
	}

	// 추가 초. 예) 3 → "+3초" / 0 → 숨김
	if (BonusText)
	{
		const int32 Seconds = FMath::RoundToInt(Result.BonusSeconds);
		if (Seconds > 0)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Seconds"), Seconds);
			BonusText->SetText(FText::Format(BonusFormat, Args));
			BonusText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			BonusText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	HideTime = Now + ShowSeconds;

	// 위치를 먼저 잡고 보인다 (한 프레임 엉뚱한 자리에 뜨지 않게)
	ResultPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	UpdatePosition();

	OnShowResult(Result.Grade, Result.BonusSeconds);
}

void ULOACoopJustGuardResultWidget::HideResult()
{
	HideTime = 0.0;
	Target.Reset();
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void ULOACoopJustGuardResultWidget::UpdatePosition()
{
	// 보스 캡슐 가운데(= 액터 위치) + 보스 앞 × Forward Offset. 보스가 없어졌으면 마지막 위치 그대로
	//   예) 보스 (1690, 1770, 20), 앞 +X, 400 → (2090, 1770, 20)
	if (const AActor* TargetActor = Target.Get())
	{
		LastWorldLocation = TargetActor->GetActorLocation() + TargetActor->GetActorForwardVector() * ForwardOffset;
	}

	// 월드 위치 → 이 위젯 안의 위치 (DPI 배율 반영). 카메라 뒤면 false
	FVector2D WidgetPosition;
	const bool bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		GetOwningPlayer(), LastWorldLocation, WidgetPosition, /*bPlayerViewportRelative*/ false);

	if (bOnScreen && PanelSlot)
	{
		PanelSlot->SetPosition(WidgetPosition);
	}

	// Hidden: 그리지만 않음 (애니메이션은 계속)
	const ESlateVisibility WantedVisibility = bOnScreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (ResultPanel->GetVisibility() != WantedVisibility)
	{
		ResultPanel->SetVisibility(WantedVisibility);
	}
}
