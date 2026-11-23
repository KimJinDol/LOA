// LOACharacterSelectWidget.h
// [화면] 로비 캐릭터 선택 화면. ① 직업 카드 목록 ③ 고른 직업 이름·설명·스킬 목록 + 이름 칸·파티 찾기 버튼. (2026-11-14 apply113, 11-20 apply132 파티 찾기)
//        ② 3D 미리보기는 레벨의 ALOACharacterPreview가 따로 그린다 (이 위젯은 모름)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//   열림(NativeConstruct)
//     ① 에셋 매니저에서 CharacterClass 목록 → 이름순 → 비동기로 불러옴 (새 직업 에셋을 만들면 카드가 저절로 늘어남)
//     ② 다 오면 카드를 만든다. 이미 고른 직업이 있으면 그것, 없으면 첫 카드를 고른다 (대신 정함)
//   카드 클릭 → ULOALocalPlayer::SetSelectedClass (고르는 곳은 LocalPlayer 하나)
//   LocalPlayer 알림 OnSelectedClassChanged → 이름·설명·카드 표시·스킬 목록 다시
//   파티 찾기 → 이름 칸의 글로 ULOALocalPlayer::SetPlayerName → 규칙에 맞는 이름이 없으면 "이름을 먼저 정하세요"
//     → 있으면 파티 찾기 창(PartyFinder, 이 WBP 안의 자식)을 연다. 레이드 입장은 그 창의 입장하기 (ULOAPartySubsystem)
//     고른 직업·이름은 엔진이 접속 주소에 붙인다 (ULOALocalPlayer::GetGameLoginOptions / GetNickname)
//
// [스킬 목록] ASC 없이 직업 데이터에서 바로
//   DefaultAbilityDataSets → 꾸러미 줄마다 { 스킬, 레벨, InputTag } → 키가 있고 플레이어 GA이고 아이콘이 있는 것만 (전투 화면 규칙과 같음)
//   순서 = 꾸러미에 적힌 순서 (대신 정함). 숫자 = LOATooltipFormat::FillSkillSlotInfo (공격력 0 → {Damage} 빈칸)
//   칸 = 전투 화면과 같은 WBP_SkillSlot (ULOASkillSlotWidget::ShowSkill, 2026-11-15 apply114). 키 글자는 안 보임 (사용자 A)
//
// [WBP] WBP_CharacterSelect (부모 = 이 클래스)
//   ClassCardBox        (가로·세로 상자 등 Panel, 필수) : 카드가 들어갈 곳
//   SkillIconBox        (Panel, 필수)                   : 스킬 아이콘이 들어갈 곳
//   ClassNameText       (TextBlock, 필수)
//   ClassDescriptionText(TextBlock, 선택)
//   PlayerNameInput     (EditableTextBox, 필수)         : 이름 칸 (파티 찾기 버튼 위)
//   NameErrorText       (TextBlock, 선택)               : "이름을 먼저 정하세요" (평소 접힘)
//   PartyFinderButton   (Button, 필수)                  : 파티 찾기 (11-20 apply132, 옛 CreateRoomButton)
//   PartyFinder         (WBP_PartyFinder, 필수)         : 파티 찾기 창. 화면 위에 덮이게 두고 처음엔 Collapsed
//   Class Defaults: Class Card Class = WBP_ClassCard / Skill Slot Class = WBP_SkillSlot / Name Required Text
//
// [네트워크] 없음. 로비는 혼자 쓰는 맵

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UObject/PrimaryAssetId.h"
#include "LOACharacterSelectWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class ULOACharacterClassData;
class ULOAClassCardWidget;
class ULOAPartyFinderWidget;
class UEditableTextBox;
class ULOASkillSlotWidget;
class ULOALocalPlayer;
struct FStreamableHandle;

UCLASS(Abstract)
class LOA_API ULOACharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ClassCardBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SkillIconBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClassNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClassDescriptionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> PlayerNameInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameErrorText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PartyFinderButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULOAPartyFinderWidget> PartyFinder;

	/** 카드 위젯 (WBP_ClassCard) */
	UPROPERTY(EditAnywhere, Category = "LOA|Lobby")
	TSubclassOf<ULOAClassCardWidget> ClassCardClass;

	/** 스킬 칸 위젯 (WBP_SkillSlot, 전투 화면과 같은 것) */
	UPROPERTY(EditAnywhere, Category = "LOA|Lobby")
	TSubclassOf<ULOASkillSlotWidget> SkillSlotClass;

	/** 이름 없이 파티 찾기를 누르면 보이는 글. 예) "이름을 먼저 정하세요." (화면 글은 WBP에) */
	UPROPERTY(EditAnywhere, Category = "LOA|Lobby")
	FText NameRequiredText;

private:
	/** ① 직업 목록을 불러온다 */
	void LoadClassList();

	/** ② 다 옴 → 카드 만들기 + 처음 고르기 */
	void HandleClassListLoaded();

	/** 카드 클릭 → LocalPlayer에 고르기 */
	void HandleCardClicked(const ULOACharacterClassData* ClassData);

	/** LocalPlayer 알림 → 화면 다시 */
	void HandleSelectedClassChanged(const ULOACharacterClassData* ClassData);

	/** 고른 직업의 스킬 칸을 다시 만든다 */
	void RefreshSkillIcons(const ULOACharacterClassData* ClassData);

	/** 이름 칸 글로 이름 정하기 → 되면 파티 찾기 창 */
	UFUNCTION()
	void HandlePartyFinderClicked();

	/** 이 화면을 보는 사람 */
	TWeakObjectPtr<ULOALocalPlayer> LobbyPlayer;

	/** 이름순 직업 ID */
	TArray<FPrimaryAssetId> ClassIds;

	/** 직업 목록 불러오기 핸들 (닫힐 때 놓기만, 취소 안 함) */
	TSharedPtr<FStreamableHandle> ClassListHandle;

	/** 만든 카드 (골라짐 표시용) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULOAClassCardWidget>> ClassCards;
};
