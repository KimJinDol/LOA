// LOAMyStatusBar.h
// [화면] 내 버프 줄 (하단 체력바 위). 내 파티원 컨트롤러의 버프를 남은 초와 함께. (2026-11-10)
//
// [흐름]
//   오버레이 컨트롤러가 들어옴 → OnPartyChanged 구독
//   파티가 바뀔 때마다 그중 "나"(IsLocalPlayer)를 찾아 그 Tracker를 StatusRow에 넣는다 (시간 보이기)
//
// [WBP] WBP_MyStatusBar (부모 = 이 클래스). WBP_Overlay에 놓으면 컨트롤러를 물려받는다
//   StatusRow (WBP_StatusIconRow, 필수)

#pragma once

#include "CoreMinimal.h"
#include "UI/LOAUserWidget.h"
#include "LOAMyStatusBar.generated.h"

class ULOAStatusIconRow;
class ULOAOverlayWidgetController;
class ULOAPartyMemberController;

UCLASS(Abstract)
class LOA_API ULOAMyStatusBar : public ULOAUserWidget
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
	TObjectPtr<ULOAStatusIconRow> StatusRow;

private:
	UFUNCTION()
	void HandlePartyChanged(const TArray<ULOAPartyMemberController*>& Members);

	void UnbindController();

	TWeakObjectPtr<ULOAOverlayWidgetController> OverlayController;
};
