// LOAPartyFrameWidget.h
// [화면] 파티 틀 (화면 왼쪽). 파티원마다 WBP_PartyMember 하나, 나를 맨 위에. (2026-11-10)
//
// [흐름]
//   오버레이 컨트롤러가 들어옴 → OnPartyChanged 구독
//   파티가 바뀔 때마다(들어옴·나감) 받은 순서대로 칸을 맞춘다. 칸은 지우지 않고 남으면 숨긴다
//
// [WBP] WBP_PartyFrame (부모 = 이 클래스). WBP_Overlay에 놓으면 컨트롤러를 물려받는다
//   MemberBox (VerticalBox, 필수)
//   Class Defaults: MemberWidgetClass = WBP_PartyMember

#pragma once

#include "CoreMinimal.h"
#include "UI/LOAUserWidget.h"
#include "LOAPartyFrameWidget.generated.h"

class UVerticalBox;
class ULOAPartyMemberWidget;
class ULOAPartyMemberController;
class ULOAOverlayWidgetController;

UCLASS(Abstract)
class LOA_API ULOAPartyFrameWidget : public ULOAUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin ULOAUserWidget Interface
	virtual void NativeOnWidgetControllerSet() override;
	//~ End ULOAUserWidget Interface

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> MemberBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI|Party")
	TSubclassOf<ULOAPartyMemberWidget> MemberWidgetClass;

	/** 칸 사이 간격 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI|Party", meta = (ClampMin = "0.0"))
	float MemberSpacing = 6.f;

private:
	UFUNCTION()
	void HandlePartyChanged(const TArray<ULOAPartyMemberController*>& Members);

	void UnbindController();

	TWeakObjectPtr<ULOAOverlayWidgetController> OverlayController;

	/** 만들어 둔 칸 (다시 씀) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULOAPartyMemberWidget>> MemberWidgets;
};
