// LOASkillSlotWidget.h
// [화면] 스킬 슬롯 한 칸의 "보여 주기". 전투 화면 슬롯과 로비 스킬 목록이 같은 WBP_SkillSlot을 쓴다. (2026-11-15 apply114)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   ShowSkill(줄) : 아이콘 + 툴팁 (줄 = FLOASkillSlotInfo, 아이콘·이름·설명·쿨·마나)
//   ShowEmpty()   : 아이콘 숨김 + 툴팁 떼기 (빈 칸)
//   보여 주는 모양은 여기 한 곳. 줄을 어디서 받는지는 부르는 쪽이 정한다
//
// [부르는 곳]
//   전투 화면 : WBP_SkillSlot BP (CE_UpdateIcon) — Overlay 컨트롤러가 목록을 보내면 내 InputTag 줄을 찾아 ShowSkill, 없으면 ShowEmpty
//              InputTag(슬롯마다 Q·W…)·쿨타임 표시·컨트롤러 묶기는 BP에 그대로 (전투 화면에만 필요)
//   로비      : ULOACharacterSelectWidget (C++) — 직업 꾸러미에서 줄을 만들어 ShowSkill. 컨트롤러가 없어 묶기·쿨타임은 일어나지 않음
//
// [WBP] WBP_SkillSlot (부모 = 이 클래스)
//   IconImage (Image, 필수) : 스킬 아이콘
//   Class Defaults: Tooltip Widget Class = WBP_Tooltip (비우면 툴팁 없음)
//
// [네트워크] 없음. 각자 화면

#pragma once

#include "CoreMinimal.h"
#include "UI/LOAUserWidget.h"
#include "LOASkillSlotWidget.generated.h"

class UImage;
class ULOATooltipWidget;
struct FLOASkillSlotInfo;

UCLASS(Abstract)
class LOA_API ULOASkillSlotWidget : public ULOAUserWidget
{
	GENERATED_BODY()

public:
	/** 스킬 한 줄 보여 주기: 아이콘 + 툴팁 */
	UFUNCTION(BlueprintCallable, Category = "LOA|UI|SkillSlot")
	void ShowSkill(const FLOASkillSlotInfo& SlotInfo);

	/** 빈 칸: 아이콘 숨김 + 툴팁 떼기 */
	UFUNCTION(BlueprintCallable, Category = "LOA|UI|SkillSlot")
	void ShowEmpty();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "LOA|UI|SkillSlot", meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	/** 툴팁 위젯 (WBP_Tooltip). 비우면 툴팁 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI|SkillSlot")
	TSubclassOf<ULOATooltipWidget> TooltipWidgetClass;
};
