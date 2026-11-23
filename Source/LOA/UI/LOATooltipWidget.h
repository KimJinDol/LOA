// LOATooltipWidget.h
// [화면] 툴팁 한 장. 스킬 슬롯과 상태 아이콘(내 버프 줄, 파티창, 보스 디버프 줄)이 같이 쓴다. (2026-11-14)
//
// ─────────────────────────────────────────────────────────────────────────────
// [모양] 이름 / (스킬만) 쿨·마나 줄 / (스킬만) 무력화 줄 / 설명
//   스킬 : 길로틴 스윙 / 재사용 대기시간 12초   마나 30 / 무력화 : 상 / 거대한 칼날을 내려찍어 2,500의 피해를 준다.
//   버프 : 공격력 증가 / (줄 숨김) / (줄 숨김) / 공격력이 10% 증가한다.
//   무력화 줄 (2026-11-18 apply121): 스킬 UI 정보의 무력화 등급(FLOASkillDisplayInfo::StaggerGrade) → 글자는 이 WBP 칸
//     Stagger Format "무력화 : {Grade}" + Stagger Grade Texts { 하, 중, 상, 최상 }. 등급이 None이거나 글자가 비면 줄을 숨김
//   (사용자 결정 11-13: 버프·디버프는 남은 시간을 넣지 않는다)
//
// [흐름]
//   아이콘 위젯 → SetSkillTooltip / SetStatusTooltip(아이콘, WBP_Tooltip 클래스, 정보)
//     ① 아이콘에 이미 붙은 툴팁이 이 클래스면 그대로 쓴다. 없으면 한 번 만들어 아이콘.SetToolTip
//     ② 내용 채우기 (이름·줄·설명)
//   그래서 아이콘마다 툴팁 위젯은 하나. 내용이 바뀔 때만 ②를 다시 한다
//
//   보여 주기·위치·지연은 엔진(Slate)이 한다 (UE 5.5 소스 확인, 11-14)
//     마우스를 올리고 0.15초 뒤 (콘솔 변수 Slate.TooltipSummonDelay) / 커서에서 (12, 8)픽셀 옆 / 화면 끝이면 안쪽으로
//     커서 아래 위젯을 히트 테스트로 찾는다 → 아이콘이 Visible이어야 한다 (HitTestInvisible이면 안 뜸)
//
// [자리표시] 숫자는 부르는 쪽이 채워서 넘긴다
//   스킬 설명 {Cooldown} {Mana} {Damage} : LOATooltipFormat::FillSkillSlotInfo 한 곳 (2026-11-14 apply113)
//     부르는 곳: ULOAOverlayWidgetController (전투 화면, 내 공격력) / ULOACharacterSelectWidget (로비, 공격력 없음 → {Damage} 빈칸)
//   상태 설명 {Value} {Duration}         : FLOAStatusInfo::MakeDescription
//   숫자 모양은 아래 LOATooltipFormat 한 곳에서 (초·% = 소수 한 자리까지, 데미지 = 정수 + 천 단위 쉼표)
//
// [WBP] WBP_Tooltip (부모 = 이 클래스)
//   TitleText       (TextBlock, 필수)
//   InfoText        (TextBlock, 선택) : 스킬 쿨·마나 줄. 쿨·마나가 둘 다 0이면 숨김
//   StaggerText     (TextBlock, 선택) : 스킬 무력화 줄 (2026-11-18). 무력화 등급이 없으면 숨김
//   DescriptionText (TextBlock, 필수) : Auto Wrap Text 켜기 (폭은 바깥 SizeBox)
//   Class Defaults : Skill Info Format (예: "재사용 대기시간 {Cooldown}초   마나 {Mana}")
//                    Stagger Format (예: "무력화 : {Grade}"), Stagger Grade Texts (Low = 하, Mid = 중, High = 상, Top = 최상)
//
// [네트워크] 없음. 각자 화면

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LOAOverlayWidgetController.h"	// FLOASkillSlotInfo (BP 함수 인자라 완전한 정의가 필요)
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"	// ELOAStaggerGrade (칸 타입)
#include "LOATooltipWidget.generated.h"

class UTextBlock;
class UWidget;
class ULOAPlayerAbility;
struct FLOAStatusInfo;

/** 툴팁 숫자 모양 (한 곳에서 정한다) */
namespace LOATooltipFormat
{
	/** 초, %처럼 소수가 있을 수 있는 값. 소수 한 자리까지, 쉼표 없음. 예) 12 / 7.5 */
	LOA_API FText Decimal(float Value);

	/** 데미지처럼 큰 정수. 반올림 + 천 단위 쉼표. 예) 2,500 */
	LOA_API FText WholeNumber(float Value);

	/**
	 * 스킬 슬롯 한 줄 채우기: InputTag, UI 정보, 쿨·마나, 설명 자리표시. (2026-11-14 apply113, 전투 화면에서 옮김)
	 *   Level           : 그 스킬 레벨 (전투 = Spec 레벨 / 로비 = 꾸러미 줄의 레벨)
	 *   AttackPowerBase : 공격력 BaseValue. 0이면 {Damage}는 빈칸 (로비, 공격력 스탯창 없음)
	 *   예) 공격력 1000, 휠윈드 DamageRate 2.5 → {Damage} = 2,500
	 * 값이 없는 자리표시는 빈칸 (넣지 않으면 "{Damage}" 글자가 그대로 남는다)
	 */
	LOA_API void FillSkillSlotInfo(FLOASkillSlotInfo& OutSlot, const FGameplayTag& InputTag, const ULOAPlayerAbility& PlayerAbility, float Level, float AttackPowerBase);
}

UCLASS(Abstract)
class LOA_API ULOATooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 스킬 슬롯의 툴팁을 붙이고 채운다. Info가 비면(빈 슬롯) 툴팁을 뗀다.
	 * WBP_SkillSlot이 슬롯 목록에서 자기 줄을 찾은 뒤 부른다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|UI|Tooltip")
	static void SetSkillTooltip(UWidget* Target, TSubclassOf<ULOATooltipWidget> TooltipClass, const FLOASkillSlotInfo& SlotInfo);

	/** 상태 아이콘의 툴팁을 붙이고 채운다. (ULOAStatusIcon::Setup이 부름) */
	static void SetStatusTooltip(UWidget* Target, TSubclassOf<ULOATooltipWidget> TooltipClass, const FLOAStatusInfo& StatusInfo);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InfoText;

	/** 무력화 줄 (2026-11-18). 없으면 무력화 줄 없이 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaggerText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionText;

	/** 스킬 쿨·마나 줄 모양. {Cooldown} {Mana}. 비어 있으면 줄을 숨긴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI|Tooltip")
	FText SkillInfoFormat;

	/** 무력화 줄 모양. {Grade} = 아래 등급 글자. 비어 있으면 줄을 숨긴다 (2026-11-18) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI|Tooltip")
	FText StaggerFormat;

	/** 무력화 등급 → 글자 (예: High = 상). 없는 등급은 줄을 숨긴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|UI|Tooltip")
	TMap<ELOAStaggerGrade, FText> StaggerGradeTexts;

private:
	/** Target에 붙은 툴팁이 이 클래스면 그것, 아니면 새로 만들어 붙인다. 실패하면 nullptr */
	static ULOATooltipWidget* FindOrCreate(UWidget* Target, TSubclassOf<ULOATooltipWidget> TooltipClass);

	/** 네 칸 채우기. Info·Stagger·Description이 비면 그 줄을 숨긴다 */
	void SetContent(const FText& Title, const FText& Info, const FText& Stagger, const FText& Description);

	/** 무력화 줄 글자. 등급이 None, 모양이 비었거나 그 등급 글자가 없으면 빈 글자 (줄 숨김) */
	FText MakeStaggerLine(ELOAStaggerGrade Grade) const;
};
