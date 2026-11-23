// LOACombatTextWidget.h
// [배관] 전투 글자 위젯의 부모. 글자 하나(데미지 숫자 / 회복 숫자 / 버프·디버프·상태 이상 이름)를 받는 입구만 만든다.
// (2026-11-16 apply116: 옛 ULOADamageNumberWidget. 클래스 이름은 CoreRedirects로 이어져 WBP_DamageNumber가 그대로 붙는다)
//
// 모양(글자 크기, 색, 외곽선)과 움직임(떠오르기, 흐려지기)은 전부 WBP에서 만든다.
//   WBP (부모: LOACombatTextWidget)
//     - On Text Set 이벤트 (2026-11-16): Kind를 보고 모양을 정한다. 풀에서 돌려쓰므로 매번 모든 칸을 다시 정한다
//         Damage : Amount 숫자, Critical이면 노랗고 크게, Directional Attack이면 "BACK ATTACK" / "HEAD ATTACK"
//                  Weak Point가 켜져 있으면 그 대신 "약점 공격" (2026-11-18 apply124)
//                  Brawl Damage가 켜져 있으면 색: Critical이면 Brawl Crit Color(빨강), 아니면 Brawl Color(분홍) (2026-11-18 apply125)
//         Heal   : Amount 숫자, 초록
//         Buff   : Status Name, 상아색 (사용자)
//         Debuff : Status Name, 진한 분홍색 (사용자)
//         Status : Status Name, 상태 이상 색 (2026-11-16 apply117)
//         DamageTaken : Amount 숫자, 빨강 (DamageTakenColor, 2026-11-16 apply118)
//       아이콘은 띄우지 않는다. 글자만 (사용자 2026-11-16)
//       색은 WBP 변수로 (하드코딩 금지)
//     - UMG 애니메이션: 위로 떠오르면서 흐려짐 (Start at Time = 0으로 처음부터)
//
// [옛 이벤트] (2026-11-16 apply117) On Damage Set / On Directional Attack Set / On Critical Set은 지웠다.
//   WBP_DamageNumber는 apply116 BP 단계에서 On Text Set으로 옮겨 옛 노드가 없다
//
// 컨트롤러를 받지 않는다. (전투 글자 레이어가 풀에서 꺼내 직접 넣어 준다)
// 그래서 ULOAUserWidget이 아니라 UUserWidget을 상속한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Combat/CombatText/LOACombatTextTypes.h"
#include "LOACombatTextWidget.generated.h"

struct FLOAStatusInfo;

UCLASS(Abstract)
class LOA_API ULOACombatTextWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 글자 하나를 넣는다. 전투 글자 레이어(ULOACombatTextLayer)가 풀에서 꺼낼 때마다 호출한다.
	 * @param StatusRow  Buff·Debuff·Status면 상태 정보 표의 그 줄 (이름을 읽음). 숫자면 nullptr
	 */
	void Setup(const FLOACombatTextInfo& Info, const FLOAStatusInfo* StatusRow);

protected:
	/**
	 * 글자가 들어왔다 (2026-11-16). WBP에서 Kind를 보고 모양 설정 + 애니메이션 재생. 헤더 위 표
	 * @param StatusName  Buff·Debuff·Status의 이름 (상태 정보 표 Display Name). 숫자면 빈 글자
	 * (2026-11-16 apply117: 아이콘 인자 StatusIcon을 뺐다. 글자만 띄운다, 사용자)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|UI", meta = (DisplayName = "On Text Set"))
	void OnTextSet(const FLOACombatTextInfo& Info, const FText& StatusName);
};
