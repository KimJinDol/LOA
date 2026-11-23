// LOABossHealthBarWidget.h
// [화면] 보스 체력바의 C++ 부모. 아이콘 줄에 보스 상태를 넣어 주고, 무력화 게이지를 그린다. (2026-11-10, 무력화 2026-11-18 apply120)
//   체력·이름·줄 수 그리기는 지금처럼 WBP_BossHealthBar의 BP 그래프가 한다 (OnWidgetControllerSet)
//
// [흐름]
//   HUD가 보스 위젯 컨트롤러를 넣음 → NativeOnWidgetControllerSet
//     → StatusRow에 컨트롤러의 Tracker (남은 초 보임)
//     → 컨트롤러의 OnBossStaggerChanged 구독 → StaggerBar 채움 = 현재 ÷ 최대. 최대가 0이면(무력화 없는 보스) 숨김
//   예) 최대 100, 버서커 스킬 20 → 1.0 → 0.8 → … → 0 (다 깎음) → 전멸·대난투 끝에 다시 채우면 1.0
//
// [WBP] WBP_BossHealthBar (부모 = 이 클래스)
//   StatusRow  (WBP_StatusIconRow, 선택) : 체력바 밑. 없으면 상태 아이콘 없이 체력바만
//   StaggerBar (Progress Bar, 선택)      : 체력바 밑 얇은 막대 (2026-11-18). 없으면 무력화 게이지 없이

#pragma once

#include "CoreMinimal.h"
#include "UI/LOAUserWidget.h"
#include "LOABossHealthBarWidget.generated.h"

class ULOAStatusIconRow;
class UProgressBar;
class ULOABossWidgetController;

UCLASS(Abstract)
class LOA_API ULOABossHealthBarWidget : public ULOAUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin ULOAUserWidget Interface
	virtual void NativeOnWidgetControllerSet() override;
	//~ End ULOAUserWidget Interface

	//~ Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULOAStatusIconRow> StatusRow;

	/** 무력화 게이지 (2026-11-18). BP 그래프에서도 읽을 수 있게 BlueprintReadOnly (작업 규칙 BindWidget 줄) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StaggerBar;

private:
	/** 컨트롤러 알림 (AddDynamic이라 UFUNCTION). 최대가 0이면 숨김, 아니면 채움 = 현재 ÷ 최대 */
	UFUNCTION()
	void HandleStaggerChanged(float CurrentValue, float MaxValue);

	/** 구독 해제 (ULOAChargeGauge와 같은 방식) */
	void UnbindController();

	/** 구독한 컨트롤러 (위젯이 사라질 때·다른 컨트롤러로 바뀔 때 구독 해제) */
	TWeakObjectPtr<ULOABossWidgetController> BoundController;
};
