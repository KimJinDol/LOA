// LOAPartyMemberWidget.h
// [화면] 파티 틀의 한 명. 직업 아이콘 | 이름 / 체력바(보호막) / 버프 아이콘. 죽으면 회색 + 사망 표시. (2026-11-10)
//
// [흐름]
//   파티 틀(ULOAPartyFrameWidget)이 SetMemberController(파티원 컨트롤러) → 알림 구독 → 지금 값 한 번 받기
//
// [WBP] WBP_PartyMember (부모 = 이 클래스)
//   NameText     (TextBlock, 필수)          : 플레이어 이름
//   HealthBar    (ProgressBar, 필수)        : 체력 (앞)
//   ShieldBar    (ProgressBar, 선택)        : 보호막 끝 (뒤, 흰색). 체력바와 겹쳐 둔다 (Overlay)
//   ClassIcon    (Image, 선택)              : 직업 아이콘. 없으면 숨김
//   ClassText    (TextBlock, 선택)          : 직업 이름
//   DeadMarker   (아무 위젯, 선택)          : 죽었을 때만 보임 (해골·"사망" 글자 등)
//   StatusRow    (WBP_StatusIconRow, 선택)  : 버프 아이콘 (시간 없음. 나도 여기선 시간 없이)
//   Class Defaults: DeadTint = 죽었을 때 전체 색 (회색)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOAPartyMemberWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class UWidget;
class UTexture2D;
class ULOAStatusIconRow;
class ULOAPartyMemberController;

UCLASS(Abstract)
class LOA_API ULOAPartyMemberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 파티원을 그린다. 이전 구독은 푼다 */
	void SetMemberController(ULOAPartyMemberController* InController);

	UFUNCTION(BlueprintPure, Category = "LOA|UI|Party")
	ULOAPartyMemberController* GetMemberController() const { return Controller.Get(); }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ShieldBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ClassIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClassText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DeadMarker;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULOAStatusIconRow> StatusRow;

	/** 죽었을 때 위젯 전체에 곱하는 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI|Party")
	FLinearColor DeadTint = FLinearColor(0.45f, 0.45f, 0.45f, 1.f);

private:
	UFUNCTION()
	void HandleInfoChanged(FText PlayerName, FText ClassName, UTexture2D* InClassIcon);

	UFUNCTION()
	void HandleHealthBarChanged(float HealthPercent, float ShieldEndPercent, float Shield);

	UFUNCTION()
	void HandleDeadChanged(bool bIsDead);

	void Unbind();

	TWeakObjectPtr<ULOAPartyMemberController> Controller;
};
