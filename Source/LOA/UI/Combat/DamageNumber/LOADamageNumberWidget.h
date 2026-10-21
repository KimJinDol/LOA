// LOADamageNumberWidget.h
// [배관] 데미지 숫자 위젯의 부모. 숫자를 받는 입구 하나만 만든다.
//
// 모양(글자 크기, 색, 외곽선)과 움직임(떠오르기, 흐려지기)은 전부 WBP에서 만든다.
//   WBP_DamageNumber (부모: LOADamageNumberWidget)
//     - TextBlock 하나
//     - UMG 애니메이션: 위로 떠오르면서 흐려짐
//     - On Damage Set 이벤트: 받은 숫자를 TextBlock에 넣고 애니메이션 재생
//
// 컨트롤러를 받지 않는다. (데미지 숫자 레이어가 풀에서 꺼내 숫자를 직접 넣어 준다)
// 그래서 ULOAUserWidget이 아니라 UUserWidget을 상속한다.
//
// [재사용 주의] 풀에서 돌려쓰는 위젯이다. On Damage Set이 올 때마다 애니메이션을 "처음부터" 재생해야 한다.
// (Play Animation의 Start at Time = 0) 이전 숫자의 상태가 남지 않게.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LOADamageNumberWidget.generated.h"

UCLASS(Abstract)
class LOA_API ULOADamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 데미지를 넣는다. 데미지 숫자 레이어(ULOADamageNumberLayer)가 풀에서 꺼낼 때마다 호출한다. */
	void SetDamage(float Damage);

protected:
	/** 숫자가 들어왔다. WBP에서 글자 설정 + 애니메이션 재생 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|UI", meta = (DisplayName = "On Damage Set"))
	void OnDamageSet(float Damage);
};
