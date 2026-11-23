// LOABrawlStageSegment.h
// [화면] 대난투 UI의 단계 칸 하나. 게이지 막대 + 잠금 표시. (2026-11-18 apply126)
//
// [흐름] 대난투 UI(ULOABrawlBar)가 단계 수만큼 만들어 가로 줄에 넣고, 매 프레임 SetState를 부른다
//   지난 단계 : 가득 (100%), 잠금 없음
//   지금 단계 : 그 단계 게이지 (예: 1,500 / 3,000 → 50%). 마지막 단계는 보스 남은 체력 기준 (apply127)
//   다음 단계 : 비어 있음 (0%) + 잠금 표시 (아직 못 들어감)
//   예) 3단계, 지금 2단계 게이지 40% → [■■■■] [■■□□□] [🔒]
//
// [WBP] WBP_BrawlStageSegment (부모 = 이 클래스)
//   SegmentBar (Progress Bar, 필수) : 게이지. 색·배경은 WBP에서
//   LockIcon   (아무 위젯, 선택)     : 잠금 그림. 잠긴 칸에서만 보인다
//
// [네트워크] 없음 (대난투 UI가 넘겨주는 값을 그리기만)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOABrawlStageSegment.generated.h"

class UProgressBar;
class UWidget;

UCLASS(Abstract)
class LOA_API ULOABrawlStageSegment : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 칸 모양을 정한다 (바뀔 때만 위젯을 건드린다)
	 * @param Percent  게이지 0 ~ 1
	 * @param bLocked  아직 못 들어간 단계 → 잠금 표시
	 */
	void SetState(float Percent, bool bLocked);

protected:
	/** 게이지 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> SegmentBar;

	/** 잠금 그림 (선택) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockIcon;

private:
	/** 마지막으로 넣은 값 (같으면 다시 안 넣음). 처음엔 반드시 한 번 넣도록 범위 밖 값 */
	float AppliedPercent = -1.f;
	int8 AppliedLocked = -1;
};
