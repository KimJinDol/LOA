// LOAIdentityGauge.h
// [화면] 아이덴티티 게이지. 원형 문장(엠블럼) 안의 유리 원이 아래에서 위로 차오른다. (2026-11-09)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (내 화면만)
//   컨트롤러가 들어옴 (NativeOnWidgetControllerSet) → 오버레이 컨트롤러의 알림 구독
//     OnIdentityGaugeChanged(현재, 최대) → 채움 = 현재 ÷ 최대. 최대가 0이면 숨김 (게이지 없는 직업)
//                                          게이지가 0이어도 최대가 있으면 보인다 (사용자 규칙)
//     OnIdentityActiveChanged(켜짐?)     → 켜진 색
//   그리기는 머티리얼(M_IdentityGauge)이 한다. 이 클래스는 파라미터 3개만 넣는다
//     Percent (0 ~ 1) / Active (0, 1) / Full (0, 1: 가득이고 꺼져 있음 → 빛남)
//
// [화면 규칙] (2026-11-09 사용자 결정: 원형, 숫자 없음)
//   차는 중 : 채운 부분만 금빛, 나머지는 어둡게
//   가득    : 전체가 금빛 + 깜빡이는 빛 (켤 수 있음)
//   켜짐    : 푸른빛으로 바뀌고 줄어든다
//
// [WBP] WBP_IdentityGauge (부모 = 이 클래스)
//   GaugeImage (Image, 필수) : 브러시 = M_IdentityGauge (텍스처 T_IdentityEmblem)
//
// [네트워크] 없음. 게이지·태그는 복제로 온 값

#pragma once

#include "CoreMinimal.h"
#include "UI/LOAUserWidget.h"
#include "LOAIdentityGauge.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class ULOAOverlayWidgetController;

UCLASS(Abstract)
class LOA_API ULOAIdentityGauge : public ULOAUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	/**
	 * 처음엔 숨김 (최대가 오면 보인다), 머티리얼 인스턴스 만들기 (2026-11-09 수정: NativeConstruct에서 옮김)
	 * CreateWidget 때 불린다 → HUD가 처음 값을 보내기(BroadcastInitialValues) "전"
	 * NativeConstruct는 AddToViewport 때라 처음 값을 받은 "뒤"에 불려서, 거기서 숨기면 게이지 0인 동안 계속 숨어 있었다
	 */
	virtual void NativeOnInitialized() override;
	/** 구독 해제 */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin ULOAUserWidget Interface
	/** 오버레이 컨트롤러의 아이덴티티 알림 구독 */
	virtual void NativeOnWidgetControllerSet() override;
	//~ End ULOAUserWidget Interface

	// ---------- 위젯 (WBP에 같은 이름으로) ----------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GaugeImage;

private:
	UFUNCTION()
	void HandleGaugeChanged(float CurrentValue, float MaxValue);

	UFUNCTION()
	void HandleActiveChanged(bool bActive);

	/** 지금 값으로 머티리얼 파라미터 넣기 */
	void ApplyToMaterial();

	/** 구독 해제 */
	void UnbindController();

	TWeakObjectPtr<ULOAOverlayWidgetController> OverlayController;

	/** GaugeImage의 머티리얼 인스턴스 (파라미터를 바꿀 수 있는 것) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GaugeMaterial;

	float Percent = 0.f;
	bool bActive = false;
};
