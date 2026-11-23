// LOACombatTextLayer.cpp

#include "UI/Combat/CombatText/LOACombatTextLayer.h"
#include "UI/Combat/CombatText/LOACombatTextWidget.h"
#include "UI/Status/LOAStatusInfoData.h"
#include "Library/LOACombatLibrary.h"
#include "Core/LOALog.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACombatTextLayer)

void ULOACombatTextLayer::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 화면 전체를 덮지만 클릭은 그대로 아래(게임)로 통과시킨다. (우클릭 이동이 막히지 않게)
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// ① Canvas 준비: WBP 루트가 Canvas면 그걸 쓰고, 비어 있으면 코드로 만든다.
	TextCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!TextCanvas)
	{
		if (GetRootWidget() || !WidgetTree)
		{
			LOA_LOG(this, Error, TEXT("[CombatText] Root widget must be empty or a Canvas Panel."));
			return;
		}

		TextCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TextCanvas"));
		WidgetTree->RootWidget = TextCanvas;
	}

	if (!TextWidgetClass)
	{
		LOA_LOG(this, Warning, TEXT("[CombatText] TextWidgetClass is not set on %s."), *GetNameSafe(GetClass()));
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

void ULOACombatTextLayer::ShowText(AActor* Target, const FLOACombatTextInfo& Info)
{
	if (!Target)
	{
		return;
	}

	// ① Buff·Debuff·Status: 상태 정보 표에서 이름 줄을 찾는다. 표에 없는 태그는 띄우지 않는다 (아이콘 줄과 같은 규칙, 2026-11-16)
	//    예) State.Buff.DamageDealt → "주는 피해 증가" / State.CrowdControl.Status.Stunned → "기절"
	const FLOAStatusInfo* StatusRow = nullptr;
	if (Info.Kind == ELOACombatTextKind::Buff || Info.Kind == ELOACombatTextKind::Debuff || Info.Kind == ELOACombatTextKind::Status)
	{
		StatusRow = StatusInfoData ? StatusInfoData->FindRow(Info.StatusTag) : nullptr;
		if (!StatusRow)
		{
			return;
		}
	}

	const int32 Index = AcquireEntry();
	if (Index == INDEX_NONE)
	{
		return;
	}

	FLOACombatTextEntry& Entry = Entries[Index];

	// 위치: 대상 기준 상대 위치를 지금 한 번 정한다. 이후로는 대상을 따라간다.
	Entry.Target = Target;
	Entry.Offset = CalcOffset(Target, Info);
	Entry.WorldLocation = Target->GetActorLocation() + Entry.Offset;
	Entry.Age = 0.f;

	// 쉬던 글자면 활성으로. (가장 오래된 걸 재사용한 경우는 이미 활성)
	if (!Entry.bActive)
	{
		Entry.bActive = true;
		++ActiveCount;
	}

	// 새 글자가 위에 그려지게
	Entry.CanvasSlot->SetZOrder(++NextZOrder);

	// 기준점: 데미지는 글자 아래쪽이 맞은 지점에, 나머지는 글자 한가운데가 캡슐 가운데에 (apply118. 풀 위젯이라 매번 정한다)
	const bool bIsDamage = Info.Kind == ELOACombatTextKind::Damage;
	Entry.CanvasSlot->SetAlignment(bIsDamage ? TextAlignment : BodyTextAlignment);

	// 밝기: 원래대로 (전에 지난 데미지 글자로 어두워졌을 수 있다) → 내 데미지면 바로 전 데미지 글자를 어둡게 (헤더 [가장 최근 데미지만 또렷하게])
	Entry.Kind = Info.Kind;
	Entry.Widget->SetColorAndOpacity(FLinearColor::White);
	if (bIsDamage)
	{
		FadeLatestDamage(Index);
		LatestDamageIndex = Index;
	}

	// 글자 전달 → WBP가 Kind를 보고 모양 + 애니메이션 처음부터 재생 (풀 위젯이라 매번 모든 칸을 다시 정한다)
	Entry.Widget->Setup(Info, StatusRow);

	// 다음 틱까지 기다리면 한 프레임 동안 이전 자리에 보일 수 있으니 바로 옮긴다.
	UpdateEntryPosition(Entry);
}

FVector ULOACombatTextLayer::CalcOffset(const AActor* Target, const FLOACombatTextInfo& Info) const
{
	const FVector TargetLocation = Target->GetActorLocation();
	FVector BaseOffset;

	if (Info.Kind != ELOACombatTextKind::Damage)
	{
		// 회복·버프·디버프·상태 이상: 대상 캡슐 가운데 = 액터 위치 그대로 (사용자 2026-11-16 apply117. 맞은 지점이 없는 효과)
		BaseOffset = FVector::ZeroVector;
	}
	else if (Info.bHasHitOffset)
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

void ULOACombatTextLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ActiveCount == 0)
	{
		return;
	}

	for (FLOACombatTextEntry& Entry : Entries)
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

int32 ULOACombatTextLayer::CreateEntry()
{
	if (!TextCanvas || !TextWidgetClass)
	{
		return INDEX_NONE;
	}

	ULOACombatTextWidget* NumberWidget = CreateWidget<ULOACombatTextWidget>(this, TextWidgetClass);
	if (!NumberWidget)
	{
		return INDEX_NONE;
	}

	UCanvasPanelSlot* CanvasSlot = TextCanvas->AddChildToCanvas(NumberWidget);
	CanvasSlot->SetAutoSize(true);               // 글자 길이에 맞는 크기
	CanvasSlot->SetAlignment(TextAlignment);   // 기준점이 월드 위치에 오도록

	// 쉬는 상태로 시작 (Collapsed: 그리지도, 틱하지도 않음)
	NumberWidget->SetVisibility(ESlateVisibility::Collapsed);

	FLOACombatTextEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Widget = NumberWidget;
	Entry.CanvasSlot = CanvasSlot;

	return Entries.Num() - 1;
}

int32 ULOACombatTextLayer::AcquireEntry()
{
	// ① 쉬는 글자
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

	// ③ 가장 오래된 글자 재사용 (곧 사라질 글자라 눈에 덜 띈다)
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

void ULOACombatTextLayer::ReleaseEntry(FLOACombatTextEntry& Entry)
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

void ULOACombatTextLayer::FadeLatestDamage(int32 NewIndex)
{
	// 바로 전 데미지 글자가 아직 떠 있고, 그 자리가 다른 종류로 다시 쓰이지 않았을 때만
	if (!Entries.IsValidIndex(LatestDamageIndex) || LatestDamageIndex == NewIndex)
	{
		return;
	}

	FLOACombatTextEntry& Previous = Entries[LatestDamageIndex];
	if (Previous.bActive && Previous.Kind == ELOACombatTextKind::Damage)
	{
		// 위젯 전체 색에 곱한다 → 주황 치명타는 어두운 주황, 흰 숫자는 회색 (색깔은 그대로, 명도만, 사용자)
		Previous.Widget->SetColorAndOpacity(FLinearColor(FadedBrightness, FadedBrightness, FadedBrightness, 1.f));
	}
}

void ULOACombatTextLayer::UpdateEntryPosition(FLOACombatTextEntry& Entry) const
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
