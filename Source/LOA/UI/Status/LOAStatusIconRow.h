// LOAStatusIconRow.h
// [화면] 상태 아이콘 줄. 파티원 한 명의 버프·디버프를 아이콘으로 나란히. (2026-11-10)
//   파티 틀(남은 시간 없음)과 내 버프 줄(남은 시간 있음)이 같이 쓴다. 다음: 보스 체력바 밑
//   Tracker(ULOAStatusTracker)만 안다 → 주인이 플레이어든 보스든 같은 줄
//
// [흐름]
//   SetStatusTracker(Tracker, 시간 보이기?) → OnStatusTagsChanged 구독 → 지금 태그로 한 번 그리기
//   태그가 바뀔 때마다: StatusInfoData 순서대로, 붙어 있는 태그만 아이콘을 만든다 (표에 없는 태그는 건너뜀)
//   아이콘 위젯은 지우지 않고 모자라면 만들고 남으면 숨긴다 (버프가 자주 붙었다 떨어져도 만들기 반복 없음)
//   아이콘은 Visible (툴팁이 마우스를 받아야 해서, 2026-11-14. 전에는 HitTestInvisible)
//
// [WBP] WBP_StatusIconRow (부모 = 이 클래스)
//   IconBox (HorizontalBox, 필수) : 아이콘이 들어갈 곳
//   Class Defaults: IconWidgetClass = WBP_StatusIcon, StatusInfoData = DA_StatusInfo, MaxIcons
//   (2026-11-16 apply117: 옛 칸 이름 IconData, CoreRedirects로 값이 이어짐)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "LOAStatusIconRow.generated.h"

class UHorizontalBox;
class ULOAStatusIcon;
class ULOAStatusInfoData;
class ULOAStatusTracker;

UCLASS(Abstract)
class LOA_API ULOAStatusIconRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 Tracker의 상태를 그린다. 이전 구독은 푼다. nullptr이면 비운다 */
	void SetStatusTracker(ULOAStatusTracker* InTracker, bool bInShowTime);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> IconBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Status")
	TSubclassOf<ULOAStatusIcon> IconWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Status")
	TObjectPtr<ULOAStatusInfoData> StatusInfoData;

	/** 이보다 많으면 앞에서부터 이만큼만 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Status", meta = (ClampMin = "1"))
	int32 MaxIcons = 8;

private:
	UFUNCTION()
	void HandleStatusTagsChanged(const FGameplayTagContainer& StatusTags);

	void Rebuild(const FGameplayTagContainer& StatusTags);
	void Unbind();

	TWeakObjectPtr<ULOAStatusTracker> Tracker;
	bool bShowTime = false;

	/** 만들어 둔 아이콘 (다시 씀) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULOAStatusIcon>> IconWidgets;
};
