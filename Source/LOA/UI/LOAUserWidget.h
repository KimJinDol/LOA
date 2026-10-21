// LOAUserWidget.h
// [배관] 우리 프로젝트 모든 위젯(WBP_)의 부모.
//
// [역할 나누기]
//   위젯(WBP)           : 그리기만 한다. 체력바 채우기, 쿨타임 원 돌리기.
//                         숫자가 어디서 오는지(GAS, ASC, AttributeSet)는 모른다.
//   위젯 컨트롤러        : GAS를 지켜보다가 "체력이 바뀌었다(현재 700 / 최대 1000)" 같은 알림을 쏜다.
//   이 클래스            : 위젯이 컨트롤러를 "받는 입구" 하나만 만든다.
//
// [로아로 치면]
// 화면 아래 체력/마나 구슬과 스킬 슬롯은 그림일 뿐이고,
// "지금 체력이 얼마인지"는 중간 관리자(컨트롤러)가 알려 준다.
//
// [사용 순서]
//   1) HUD가 위젯과 컨트롤러를 만든다.
//   2) HUD가 SetWidgetController(컨트롤러) 호출
//   3) BP의 OnWidgetControllerSet 이벤트에서 컨트롤러의 알림(OnHealthChanged 등)에 바인딩
//   4) HUD가 컨트롤러에게 "현재 값 한 번 보내" 요청 → 위젯이 처음 값으로 채워짐
//
// [자식 위젯에 자동 전달]
// SetWidgetController는 이 위젯 안에 "디자이너로 배치한" LOA 위젯(WBP_SkillSlot 등)에게 같은 컨트롤러를 자동으로 넘긴다.
// → WBP_Overlay에 슬롯을 끌어다 놓고 InputTag만 고르면 끝. BP 그래프에 Set Widget Controller를 연결하지 않는다.
//
//   WBP_Overlay.SetWidgetController(C)
//      ├─ 자식 SkillSlot_Q.SetWidgetController(C) → 그 안의 자식에게도 다시 전달 (계속 아래로)
//      ├─ 자식 SkillSlot_W.SetWidgetController(C)
//      └─ 마지막에 자기 OnWidgetControllerSet (자식들은 이미 컨트롤러를 받은 상태)
//
// - 다른 컨트롤러를 받아야 하는 자식(예: 오버레이 안에 넣은 파티창)은 bInheritWidgetController를 끈다.
//   그 자식에게는 자기 컨트롤러를 직접 넣어 준다.
// - 게임 중에 코드/BP로 새로 만든 자식(예: 버프 아이콘)은 자동 전달 대상이 아니다. 만든 쪽이 직접 넣어 준다.
// - 같은 컨트롤러가 다시 들어오면 무시한다. (알림이 두 번 바인딩되는 것 방지)
//
// [왜 컨트롤러 타입이 UObject인가]
// 위젯마다 받는 컨트롤러가 다르다. (내 화면 = Overlay, 나중에 파티창 / 보스 체력바 = 각자 다른 컨트롤러)
// 부모 클래스는 하나로 두고, BP에서 필요한 타입으로 Cast해서 쓴다.
//
// [네트워크]
// 위젯은 본인 컴퓨터에만 있다. 서버(데디케이티드)에는 만들어지지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOAUserWidget.generated.h"

UCLASS(Abstract)
class LOA_API ULOAUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이 위젯에 컨트롤러를 넣는다.
	 * ① 디자이너로 배치한 자식 LOA 위젯들에게 같은 컨트롤러를 넘기고 (bInheritWidgetController인 자식만)
	 * ② 자기 OnWidgetControllerSet을 호출한다.
	 * HUD(C++)가 최상위 위젯에 한 번 호출하면 아래로 전부 퍼진다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|UI")
	void SetWidgetController(UObject* InWidgetController);

	UFUNCTION(BlueprintPure, Category = "LOA|UI")
	UObject* GetWidgetController() const { return WidgetController; }

	/**
	 * 부모 위젯의 컨트롤러를 자동으로 물려받을지. 기본 true.
	 * 다른 컨트롤러를 써야 하는 위젯(파티창, 보스 체력바 등을 다른 위젯 안에 넣을 때)만 끈다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI")
	bool bInheritWidgetController = true;

protected:
	/**
	 * 컨트롤러가 들어왔다. BP에서 여기서 컨트롤러의 알림에 바인딩한다.
	 * (예: Cast To LOAOverlayWidgetController → Bind Event to OnHealthChanged)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|UI")
	void OnWidgetControllerSet();

private:
	/** 이 위젯이 알림을 받는 컨트롤러. 소유는 HUD가 하고, 여기서는 참조만 들고 있는다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "LOA|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UObject> WidgetController;
};