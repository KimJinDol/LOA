// LOACharacterSelectWidget.cpp

#include "UI/Lobby/LOACharacterSelectWidget.h"
#include "UI/Lobby/LOAClassCardWidget.h"
#include "UI/Lobby/LOAPartyFinderWidget.h"
#include "UI/Combat/LOASkillSlotWidget.h"
#include "UI/LOATooltipWidget.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "Data/LOAAbilityDataSet.h"
#include "Data/LOACharacterClassData.h"
#include "Player/LOALocalPlayer.h"
#include "Party/LOAPartySubsystem.h"
#include "Core/LOALog.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACharacterSelectWidget)

void ULOACharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (PartyFinderButton)
	{
		PartyFinderButton->OnClicked.AddDynamic(this, &ThisClass::HandlePartyFinderClicked);
	}
}

void ULOACharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ULOALocalPlayer* LocalPlayer = GetOwningLocalPlayer<ULOALocalPlayer>();
	if (!LocalPlayer)
	{
		LOA_LOG(this, Warning, TEXT("[Lobby] No ULOALocalPlayer. Check LocalPlayerClassName in DefaultEngine.ini."));
		return;
	}

	LobbyPlayer = LocalPlayer;
	LocalPlayer->OnSelectedClassChanged.AddUObject(this, &ThisClass::HandleSelectedClassChanged);

	// 이름은 LocalPlayer가 기억한다 (레이드에서 돌아오면 그대로 채워 둠)
	if (PlayerNameInput)
	{
		PlayerNameInput->SetText(FText::FromString(LocalPlayer->GetSelection().PlayerName));
	}
	if (NameErrorText)
	{
		NameErrorText->SetVisibility(ESlateVisibility::Collapsed);
	}

	LoadClassList();
}

void ULOACharacterSelectWidget::NativeDestruct()
{
	// LocalPlayer는 맵이 바뀌어도 남는다 → 듣기를 끊는다
	if (ULOALocalPlayer* LocalPlayer = LobbyPlayer.Get())
	{
		LocalPlayer->OnSelectedClassChanged.RemoveAll(this);
	}
	// 핸들은 놓기만 한다. 에셋 매니저가 같은 핸들로 직업 데이터를 들고 있어서 취소하면 그 상태가 꼬일 수 있다
	// (알림은 CreateUObject라 이 위젯이 사라지면 오지 않는다)
	ClassListHandle.Reset();

	Super::NativeDestruct();
}

void ULOACharacterSelectWidget::LoadClassList()
{
	ClassIds.Reset();
	UAssetManager::Get().GetPrimaryAssetIdList(ULOACharacterClassData::GetPrimaryAssetTypeName(), ClassIds);
	if (ClassIds.IsEmpty())
	{
		LOA_LOG(this, Warning, TEXT("[Lobby] No CharacterClass assets. Check PrimaryAssetTypesToScan in DefaultGame.ini."));
		return;
	}

	// 에셋 이름순 (DA_Class_Berserker, DA_Class_Ranged, DA_Class_Supporter)
	ClassIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName);
	});

	// 이미 메모리에 있으면 엔진이 이 안에서 바로 부른다 (AssetManager.cpp ChangeBundleStateForPrimaryAssets)
	ClassListHandle = UAssetManager::Get().LoadPrimaryAssets(ClassIds, TArray<FName>(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleClassListLoaded));
}

void ULOACharacterSelectWidget::HandleClassListLoaded()
{
	ULOALocalPlayer* LocalPlayer = LobbyPlayer.Get();
	if (!LocalPlayer || !ClassCardBox || !ClassCardClass)
	{
		LOA_LOG(this, Warning, TEXT("[Lobby] ClassCardBox or ClassCardClass is not set."));
		return;
	}

	ClassCardBox->ClearChildren();
	ClassCards.Reset();

	const ULOACharacterClassData* FirstClassData = nullptr;
	for (const FPrimaryAssetId& ClassId : ClassIds)
	{
		const ULOACharacterClassData* ClassData = UAssetManager::Get().GetPrimaryAssetObject<ULOACharacterClassData>(ClassId);
		if (!ClassData)
		{
			continue;
		}

		ULOAClassCardWidget* Card = CreateWidget<ULOAClassCardWidget>(this, ClassCardClass);
		if (!Card)
		{
			continue;
		}
		Card->Setup(ClassData);
		Card->OnClicked.BindUObject(this, &ThisClass::HandleCardClicked);
		ClassCardBox->AddChild(Card);
		ClassCards.Add(Card);

		if (!FirstClassData)
		{
			FirstClassData = ClassData;
		}
	}

	// 처음 고르기: 이미 고른 것이 있으면(레이드에서 돌아옴 등) 그대로 화면만 맞추고, 없으면 첫 카드 (대신 정함)
	if (const ULOACharacterClassData* AlreadySelected = LocalPlayer->GetSelectedClassData())
	{
		HandleSelectedClassChanged(AlreadySelected);
	}
	else
	{
		LocalPlayer->SetSelectedClass(FirstClassData);	// → 알림 → HandleSelectedClassChanged
	}
}

void ULOACharacterSelectWidget::HandleCardClicked(const ULOACharacterClassData* ClassData)
{
	if (ULOALocalPlayer* LocalPlayer = LobbyPlayer.Get())
	{
		LocalPlayer->SetSelectedClass(ClassData);
	}
}

void ULOACharacterSelectWidget::HandleSelectedClassChanged(const ULOACharacterClassData* ClassData)
{
	if (!ClassData)
	{
		return;
	}

	if (ClassNameText)
	{
		ClassNameText->SetText(ClassData->DisplayName.IsEmpty() ? FText::FromName(ClassData->GetFName()) : ClassData->DisplayName);
	}
	if (ClassDescriptionText)
	{
		ClassDescriptionText->SetText(ClassData->Description);
	}

	for (ULOAClassCardWidget* Card : ClassCards)
	{
		if (Card)
		{
			Card->SetSelected(Card->GetClassData() == ClassData);
		}
	}

	RefreshSkillIcons(ClassData);
}

void ULOACharacterSelectWidget::RefreshSkillIcons(const ULOACharacterClassData* ClassData)
{
	if (!SkillIconBox)
	{
		return;
	}
	SkillIconBox->ClearChildren();

	if (!ClassData || !SkillSlotClass)
	{
		return;
	}

	for (const ULOAPlayerAbilityDataSet* DataSet : ClassData->DefaultAbilityDataSets)
	{
		if (!DataSet)
		{
			continue;
		}

		for (const FLOAPlayerAbilityDataSet_Ability& Row : DataSet->GetGrantedAbilities())
		{
			// 전투 화면과 같은 규칙: 키가 있고, 플레이어 GA이고, 아이콘이 있는 스킬만 (회피·평타·자동 스킬은 뺀다)
			const ULOAPlayerAbility* PlayerAbility = Row.Ability ? Cast<ULOAPlayerAbility>(Row.Ability->GetDefaultObject()) : nullptr;
			if (!Row.InputTag.IsValid() || !PlayerAbility || PlayerAbility->GetSkillDisplayInfo().Icon.IsNull())
			{
				continue;
			}

			// 로비에는 공격력이 없다 → 0 → {Damage} 빈칸 (사용자 11-14)
			FLOASkillSlotInfo SlotInfo;
			LOATooltipFormat::FillSkillSlotInfo(SlotInfo, Row.InputTag, *PlayerAbility, static_cast<float>(Row.AbilityLevel), 0.f);

			// 컨트롤러를 넣지 않는다 → 전투용 묶기·쿨타임은 일어나지 않고 보여 주기만
			if (ULOASkillSlotWidget* SkillSlot = CreateWidget<ULOASkillSlotWidget>(this, SkillSlotClass))
			{
				SkillIconBox->AddChild(SkillSlot);
				SkillSlot->ShowSkill(SlotInfo);
			}
		}
	}
}

void ULOACharacterSelectWidget::HandlePartyFinderClicked()
{
	ULOALocalPlayer* LocalPlayer = LobbyPlayer.Get();
	if (!LocalPlayer || !PlayerNameInput || !PartyFinder)
	{
		return;
	}

	// 파티(신청 포함) 중에는 이름을 바꾸지 않는다: 파티장에게 보낸 이름과 레이드 접속 이름이 달라진다 → 창만 연다
	const UGameInstance* GameInstance = GetGameInstance();
	const ULOAPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<ULOAPartySubsystem>() : nullptr;
	if (Party && Party->GetRole() != ELOAPartyRole::None)
	{
		PlayerNameInput->SetText(FText::FromString(LocalPlayer->GetSelection().PlayerName));
		PartyFinder->Open();
		return;
	}

	// 이름 규칙은 한 곳 (FLOACharacterSelection::MakeValidPlayerName). 고친 결과가 비면 실패
	if (!LocalPlayer->SetPlayerName(PlayerNameInput->GetText().ToString()))
	{
		if (NameErrorText)
		{
			NameErrorText->SetText(NameRequiredText);
			NameErrorText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		return;
	}

	// 고쳐진 이름(못 쓰는 글자 빠짐·길이 자름)을 칸에 다시 보여 준다
	PlayerNameInput->SetText(FText::FromString(LocalPlayer->GetSelection().PlayerName));
	if (NameErrorText)
	{
		NameErrorText->SetVisibility(ESlateVisibility::Collapsed);
	}

	PartyFinder->Open();
}
