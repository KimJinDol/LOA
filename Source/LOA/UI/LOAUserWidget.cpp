// LOAUserWidget.cpp

#include "UI/LOAUserWidget.h"

#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAUserWidget)

void ULOAUserWidget::SetWidgetController(UObject* InWidgetController)
{
	// 같은 컨트롤러가 다시 들어오면 무시. (BP에 예전 Set Widget Controller 연결이 남아 있어도 알림이 두 번 걸리지 않게)
	if (WidgetController == InWidgetController)
	{
		return;
	}

	WidgetController = InWidgetController;

	// ① 디자이너로 배치한 자식 LOA 위젯에게 같은 컨트롤러 전달
	//    ForEachWidget은 "이 위젯의 트리"만 돈다. 자식 위젯 내부 트리는 자식의 SetWidgetController가 다시 돈다. (중복 없음)
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([InWidgetController](UWidget* Widget)
		{
			ULOAUserWidget* ChildWidget = Cast<ULOAUserWidget>(Widget);
			if (ChildWidget && ChildWidget->bInheritWidgetController)
			{
				ChildWidget->SetWidgetController(InWidgetController);
			}
		});
	}

	// ② 자기 C++ → BP 이벤트. 이 시점에 자식들은 이미 컨트롤러를 받았다.
	NativeOnWidgetControllerSet();
	OnWidgetControllerSet();
}