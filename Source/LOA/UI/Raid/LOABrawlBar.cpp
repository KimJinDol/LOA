// LOABrawlBar.cpp

#include "UI/Raid/LOABrawlBar.h"
#include "UI/Raid/LOABrawlStageSegment.h"
#include "Actors/LOABrawlArena.h"
#include "Game/LOARaidSubsystem.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOABrawlBar)

ULOABrawlBar::ULOABrawlBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 글자 기본 모양 (BP 기본값에서 바꾼다)
	StageFormat = NSLOCTEXT("LOA", "BrawlStageFormat", "{Stage}단계");
	LastStageText = NSLOCTEXT("LOA", "BrawlLastStage", "마지막 단계");
	TimeFormat = NSLOCTEXT("LOA", "BrawlTimeFormat", "{Seconds}s");
}

void ULOABrawlBar::NativeConstruct()
{
	Super::NativeConstruct();

	// 처음엔 숨김 (bShown 기본값 true → 한 번은 반드시 숨긴다)
	SetShown(false);
}

void ULOABrawlBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this);
	const ALOABrawlArena* Arena = Raid ? Raid->GetBrawlArena() : nullptr;
	const bool bShow = Arena && Arena->ShouldShowBrawlUI();

	SetShown(bShow);
	if (!bShow)
	{
		return;
	}

	// 누적 피해. 예) 7500.4 → "7,500"
	TotalDamageText->SetText(FText::AsNumber(FMath::FloorToInt64(Arena->GetTotalDamage())));

	if (StageText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Stage"), Arena->GetStage());
		StageText->SetText(Arena->IsLastStage() ? LastStageText : FText::Format(StageFormat, Args));
	}

	UpdateStageSegments(*Arena);

	if (TimeText)
	{
		// 소수 자리 고정. 예) 3자리: 12.3456 → "12.346s", 0 → "0.000s"
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = TimeFractionalDigits;
		Options.MaximumFractionalDigits = TimeFractionalDigits;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Seconds"), FText::AsNumber(Arena->GetRemainingSeconds(), &Options));
		TimeText->SetText(FText::Format(TimeFormat, Args));
	}
}

void ULOABrawlBar::UpdateStageSegments(const ALOABrawlArena& Arena)
{
	if (!StageSegmentBox || !StageSegmentClass)
	{
		return;
	}

	// 단계 수가 다르면 다시 만든다 (처음 한 번, 데이터가 바뀌었을 때)
	const int32 StageCount = Arena.GetStageCount();
	if (StageSegments.Num() != StageCount)
	{
		StageSegmentBox->ClearChildren();
		StageSegments.Reset();
		for (int32 Index = 0; Index < StageCount; ++Index)
		{
			ULOABrawlStageSegment* Segment = CreateWidget<ULOABrawlStageSegment>(this, StageSegmentClass);
			if (!Segment)
			{
				return;
			}
			StageSegmentBox->AddChild(Segment);
			StageSegments.Add(Segment);
		}
	}

	// 칸 번호(1부터) vs 지금 단계: 지난 단계 가득 / 지금 단계 게이지 / 다음 단계 잠금
	//   예) 지금 2단계, 게이지 0.4 → 1: 1.0 / 2: 0.4 / 3: 0, 잠금
	const int32 CurrentStage = Arena.GetStage();
	for (int32 Index = 0; Index < StageSegments.Num(); ++Index)
	{
		const int32 SegmentStage = Index + 1;
		if (SegmentStage < CurrentStage)
		{
			StageSegments[Index]->SetState(1.f, false);
		}
		else if (SegmentStage == CurrentStage)
		{
			StageSegments[Index]->SetState(Arena.GetStageGaugePercent(), false);
		}
		else
		{
			StageSegments[Index]->SetState(0.f, true);
		}
	}
}

void ULOABrawlBar::SetShown(bool bShow)
{
	if (bShown == bShow)
	{
		return;
	}
	bShown = bShow;

	// 숨길 때 Collapsed. 보일 때 클릭은 통과 (마우스를 막지 않게)
	const ESlateVisibility NewVisibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (BrawlPanel)
	{
		BrawlPanel->SetVisibility(NewVisibility);
		return;
	}

	TArray<UWidget*> Widgets = { TotalDamageText.Get(), StageText.Get(), StageSegmentBox.Get(), TimeText.Get() };
	for (UWidget* Widget : Widgets)
	{
		if (Widget)
		{
			Widget->SetVisibility(NewVisibility);
		}
	}
}
