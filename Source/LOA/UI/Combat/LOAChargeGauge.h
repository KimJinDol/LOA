// LOAChargeGauge.h
// [화면] 차지 게이지. 차지 스킬을 누르고 있는 동안 막대(프로그레스 바) + 퍼펙트 존 배경 + 지금 단계를 보여 준다. (2026-11-06)
// 전에는 WBP_ChargeGauge의 BP 그래프가 머티리얼(M_ChargeGauge) 파라미터를 바꿨다. 그 일을 여기로 옮겼다 (홀딩 저가 게이지와 같은 방식)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (내 화면만)
//   컨트롤러가 들어옴 (NativeOnWidgetControllerSet) → 오버레이 컨트롤러의 알림 구독
//     OnChargeStarted           → 보이기 (숨기기 예약이 있으면 취소)
//     OnChargeReleased(단계, 퍼펙트) → 단계 글자 = 다 채운 단계 수 → Hide Delay(0.5초) 뒤 숨기기
//     OnChargeCancelled         → 바로 숨기기
//   NativeTick: 차지 중이면 ULOAOverlayWidgetController::GetChargeGaugeState를 그대로 그린다 (계산은 컨트롤러)
//     막대 채움 = Progress (칸마다 0부터 다시)
//     막대 색   = 지금 떼면 퍼펙트면 Perfect Fill Color, 아니면 Fill Color
//     퍼펙트 존 배경 = 지금 칸에 걸쳐 있으면 보임. 자리는 가로 박스 칸 비율 (앞 빈칸 | 존 | 뒤 빈칸)
//       예) 칸 안 0.6 ~ 0.8 → 앞 0.6 : 존 0.2 : 뒤 0.2
//     단계 글자 = 지금 채우는 단계. 0이면 숨김
//   발사 뒤에는 차지 중이 아니므로 마지막 모습 그대로 두고 숨기기만 기다린다 (전 BP와 같음)
//
// [WBP] WBP_ChargeGauge (부모 = 이 클래스)
//   ChargePanel        (아무 위젯, 필수)       : 숨기고 보일 묶음. 루트는 숨기지 않는다 (숨기면 Tick이 멈춘다)
//   ChargeBar          (Progress Bar, 필수)    : 막대. 배경은 투명하게 (뒤의 퍼펙트 존이 보이게)
//   StageText          (Text Block, 필수)      : 단계 숫자
//   PerfectZoneRow     (Horizontal Box, 선택)  : 퍼펙트 존 줄 (막대 뒤에 깔린다)
//   PerfectZoneBefore / PerfectZoneBand / PerfectZoneAfter (그 가로 박스의 자식 셋, 선택)
//     존 배경색은 PerfectZoneBand의 색으로 WBP에서 정한다
//
// [네트워크] 없음. 차지 GA가 내 컴퓨터에서 보내는 신호만 쓴다 (ULOAOverlayWidgetController [차지 게이지])

#pragma once

#include "CoreMinimal.h"
#include "UI/LOAUserWidget.h"
#include "Engine/TimerHandle.h"
#include "LOAChargeGauge.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;
class ULOAOverlayWidgetController;

UCLASS(Abstract)
class LOA_API ULOAChargeGauge : public ULOAUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	/** 처음엔 숨김 */
	virtual void NativeConstruct() override;
	/** 구독 해제, 숨기기 예약 취소 */
	virtual void NativeDestruct() override;
	/** 차지 중이면 지금 모습 그리기 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	//~ Begin ULOAUserWidget Interface
	/** 오버레이 컨트롤러의 차지 알림 구독 */
	virtual void NativeOnWidgetControllerSet() override;
	//~ End ULOAUserWidget Interface

	// ---------- 위젯 (WBP에 같은 이름으로) ----------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ChargePanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ChargeBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PerfectZoneRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PerfectZoneBefore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PerfectZoneBand;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PerfectZoneAfter;

	// ---------- 설정 ----------

	/** 막대 색 (보통) */
	UPROPERTY(EditAnywhere, Category = "LOA|UI|Charge")
	FLinearColor FillColor = FLinearColor(1.f, 0.78f, 0.2f, 1.f);

	/** 막대 색 (지금 떼면 퍼펙트) */
	UPROPERTY(EditAnywhere, Category = "LOA|UI|Charge")
	FLinearColor PerfectFillColor = FLinearColor(1.f, 0.97f, 0.75f, 1.f);

	/** 발사 뒤 단계를 보여 주고 숨기기까지 (초) */
	UPROPERTY(EditAnywhere, Category = "LOA|UI|Charge", meta = (ClampMin = "0.0", Units = "Seconds"))
	float HideDelay = 0.5f;

private:
	UFUNCTION()
	void HandleChargeStarted();

	UFUNCTION()
	void HandleChargeReleased(int32 Stage, bool bPerfect);

	UFUNCTION()
	void HandleChargeCancelled();

	/** 지금 모습 그리기 (NativeTick) */
	void DrawState();

	/** 퍼펙트 존 배경: 보이기 + 가로 박스 칸 비율 (앞 | 존 | 뒤) */
	void SetPerfectZone(bool bShow, float ZoneStart, float ZoneEnd);

	/** 게이지 숨기기 (발사 뒤 Hide Delay가 지나면 / 끊김) */
	void HideGauge();

	/** 보이기 / 숨기기 (바뀔 때만) */
	void SetShown(bool bShow);

	/** 숨기기 예약 취소 */
	void ClearHideTimer();

	/** 구독 해제 */
	void UnbindController();

	TWeakObjectPtr<ULOAOverlayWidgetController> OverlayController;
	FTimerHandle HideTimer;
	bool bShown = true;
};
