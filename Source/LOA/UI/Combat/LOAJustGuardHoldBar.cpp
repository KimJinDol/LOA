// LOAJustGuardHoldBar.cpp

#include "UI/Combat/LOAJustGuardHoldBar.h"
#include "Combat/LOAJustGuardComponent.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAJustGuardHoldBar)

void ULOAJustGuardHoldBar::NativeConstruct()
{
	Super::NativeConstruct();

	// 처음엔 숨김 (bShown 기본값 true → 한 번은 반드시 숨긴다)
	SetShown(false);
}

void ULOAJustGuardHoldBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	float Progress = 0.f;
	const ULOAJustGuardComponent* JustGuardComp = FindMyJustGuardComponent();
	const bool bHolding = JustGuardComp && JustGuardComp->GetHoldProgress(Progress);

	SetShown(bHolding);
	if (!bHolding)
	{
		return;
	}

	if (HoldBar)
	{
		HoldBar->SetPercent(Progress);
	}
	if (HoldText)
	{
		HoldText->SetText(FText::AsPercent(Progress));
	}
}

ULOAJustGuardComponent* ULOAJustGuardHoldBar::FindMyJustGuardComponent()
{
	// 캐릭터가 바뀌면(부활) 다시 찾는다
	APawn* Pawn = GetOwningPlayerPawn();
	if (Pawn != CachedPawn.Get())
	{
		CachedPawn = Pawn;
		CachedJustGuardComp = Pawn ? Pawn->FindComponentByClass<ULOAJustGuardComponent>() : nullptr;
	}
	return CachedJustGuardComp.Get();
}

void ULOAJustGuardHoldBar::SetShown(bool bShow)
{
	if (bShown == bShow)
	{
		return;
	}
	bShown = bShow;

	// 숨길 때 Collapsed (자리도 차지하지 않게). 보일 때 클릭은 통과 (게이지가 마우스를 막지 않게)
	const ESlateVisibility NewVisibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (HoldPanel)
	{
		HoldPanel->SetVisibility(NewVisibility);
	}
	else if (HoldBar)
	{
		HoldBar->SetVisibility(NewVisibility);
		if (HoldText)
		{
			HoldText->SetVisibility(NewVisibility);
		}
	}
}
