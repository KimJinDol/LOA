// LOADamageNumberLayer.cpp

#include "UI/Combat/DamageNumber/LOADamageNumberLayer.h"
#include "UI/Combat/DamageNumber/LOADamageNumberWidget.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOADamageNumberLayer)

void ULOADamageNumberLayer::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 화면 전체를 덮지만 클릭은 그대로 아래(게임)로 통과시킨다. (우클릭 이동이 막히지 않게)
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// ① Canvas 준비: WBP 루트가 Canvas면 그걸 쓰고, 비어 있으면 코드로 만든다.
	NumberCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!NumberCanvas)
	{
		if (GetRootWidget() || !WidgetTree)
		{
			LOA_LOG(this, Error, TEXT("[DamageNumber] Root widget must be empty or a Canvas Panel."));
			return;
		}

		NumberCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("NumberCanvas"));
		WidgetTree->RootWidget = NumberCanvas;
	}

	if (!NumberWidgetClass)
	{
		LOA_LOG(this, Warning, TEXT("[DamageNumber] NumberWidgetClass is not set on %s."), *GetNameSafe(GetClass()));
		return;
	}

	// ② 미리 만들어 두기 (전투 중에 처음 만드는 비용을 없앰)
	const int32 Count = FMath::Min(PrewarmCount, MaxCount);
	Entries.Reserve(MaxCount);
	for (int32 i = 0; i < Count; ++i)
	{
		CreateEntry();
	}
}

void ULOADamageNumberLayer::ShowDamage(AActor* Target, const FLOADamageNumberInfo& Info)
{
	if (!Target)
	{
		return;
	}

	const int32 Index = AcquireEntry();
	if (Index == INDEX_NONE)
	{
		return;
	}

	FLOADamageNumberEntry& Entry = Entries[Index];

	// 위치: 대상 기준 상대 위치를 지금 한 번 정한다. 이후로는 대상을 따라간다.
	Entry.Target = Target;
	Entry.Offset = CalcOffset(Target, Info);
	Entry.WorldLocation = Target->GetActorLocation() + Entry.Offset;
	Entry.Age = 0.f;

	// 쉬던 숫자면 활성으로. (가장 오래된 걸 재사용한 경우는 이미 활성)
	if (!Entry.bActive)
	{
		Entry.bActive = true;
		++ActiveCount;
	}

	// 새 숫자가 위에 그려지게
	Entry.CanvasSlot->SetZOrder(++NextZOrder);

	// 숫자 전달 → WBP가 글자 설정 + 애니메이션 처음부터 재생
	Entry.Widget->SetDamage(Info.Damage);

	// 다음 틱까지 기다리면 한 프레임 동안 이전 자리에 보일 수 있으니 바로 옮긴다.
	UpdateEntryPosition(Entry);
}

FVector ULOADamageNumberLayer::CalcOffset(const AActor* Target, const FLOADamageNumberInfo& Info) const
{
	const FVector TargetLocation = Target->GetActorLocation();
	FVector BaseOffset;

	if (Info.bHasHitOffset)
	{
		// ① 히트 판정 공격: 서버가 정한 맞은 지점 (이미 대상 기준 상대 위치)
		BaseOffset = Info.HitOffset;
	}
	else
	{
		// ② 도트 등 맞은 지점 없음: 대상 충돌체 표면에서 내 캐릭터와 가장 가까운 점 (클라 연출)
		FVector ClosestPoint;
		const APawn* MyPawn = GetOwningPlayerPawn();
		if (MyPawn && ULOACombatLibrary::GetClosestPointOnTarget(Target, MyPawn->GetActorLocation(), ClosestPoint))
		{
			BaseOffset = ClosestPoint - TargetLocation;
		}
		else
		{
			// ③ 둘 다 안 되면 (내 캐릭터 없음, 충돌체 없음): 대상 충돌체 위쪽 끝
			BaseOffset = FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
		}
	}

	// 같은 곳을 연타해도 겹치지 않게 수평으로 살짝 흩뿌리고, 필요하면 위로 올린다.
	const FVector2D Random2D = FMath::RandPointInCircle(RandomOffset);
	return BaseOffset + FVector(Random2D.X, Random2D.Y, HeightOffset);
}

void ULOADamageNumberLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ActiveCount == 0)
	{
		return;
	}

	for (FLOADamageNumberEntry& Entry : Entries)
	{
		if (!Entry.bActive)
		{
			continue;
		}

		Entry.Age += InDeltaTime;
		if (Entry.Age >= LifeTime)
		{
			ReleaseEntry(Entry);
			continue;
		}

		UpdateEntryPosition(Entry);
	}
}

int32 ULOADamageNumberLayer::CreateEntry()
{
	if (!NumberCanvas || !NumberWidgetClass)
	{
		return INDEX_NONE;
	}

	ULOADamageNumberWidget* NumberWidget = CreateWidget<ULOADamageNumberWidget>(this, NumberWidgetClass);
	if (!NumberWidget)
	{
		return INDEX_NONE;
	}

	UCanvasPanelSlot* CanvasSlot = NumberCanvas->AddChildToCanvas(NumberWidget);
	CanvasSlot->SetAutoSize(true);               // 숫자 길이에 맞는 크기
	CanvasSlot->SetAlignment(NumberAlignment);   // 기준점이 월드 위치에 오도록

	// 쉬는 상태로 시작 (Collapsed: 그리지도, 틱하지도 않음)
	NumberWidget->SetVisibility(ESlateVisibility::Collapsed);

	FLOADamageNumberEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Widget = NumberWidget;
	Entry.CanvasSlot = CanvasSlot;

	return Entries.Num() - 1;
}

int32 ULOADamageNumberLayer::AcquireEntry()
{
	// ① 쉬는 숫자
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (!Entries[i].bActive)
		{
			return i;
		}
	}

	// ② 최대 개수 전이면 새로 만들기
	if (Entries.Num() < MaxCount)
	{
		const int32 NewIndex = CreateEntry();
		if (NewIndex != INDEX_NONE)
		{
			return NewIndex;
		}
	}

	// ③ 가장 오래된 숫자 재사용 (곧 사라질 숫자라 눈에 덜 띈다)
	int32 OldestIndex = INDEX_NONE;
	float OldestAge = -1.f;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].Age > OldestAge)
		{
			OldestAge = Entries[i].Age;
			OldestIndex = i;
		}
	}
	return OldestIndex;
}

void ULOADamageNumberLayer::ReleaseEntry(FLOADamageNumberEntry& Entry)
{
	Entry.bActive = false;
	Entry.Target.Reset();
	Entry.Widget->SetVisibility(ESlateVisibility::Collapsed);

	--ActiveCount;
	if (ActiveCount == 0)
	{
		NextZOrder = 0;
	}
}

void ULOADamageNumberLayer::UpdateEntryPosition(FLOADamageNumberEntry& Entry) const
{
	// 대상을 따라간다. 대상이 사라졌으면 마지막 위치 그대로.
	if (const AActor* Target = Entry.Target.Get())
	{
		Entry.WorldLocation = Target->GetActorLocation() + Entry.Offset;
	}

	// 월드 위치 → 이 레이어 안의 위치 (DPI 배율까지 반영됨). 카메라 뒤면 false.
	FVector2D WidgetPosition;
	const bool bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		GetOwningPlayer(), Entry.WorldLocation, WidgetPosition, /*bPlayerViewportRelative*/ false);

	if (bOnScreen)
	{
		Entry.CanvasSlot->SetPosition(WidgetPosition);
	}

	// Hidden: 자리는 유지하고 그리지만 않음 (애니메이션은 계속 진행)
	const ESlateVisibility WantedVisibility = bOnScreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (Entry.Widget->GetVisibility() != WantedVisibility)
	{
		Entry.Widget->SetVisibility(WantedVisibility);
	}
}
