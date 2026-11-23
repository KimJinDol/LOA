// LOASkillFactory.cpp

#include "SkillFactory/LOASkillFactory.h"

// 게임 모듈 (LOA)
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "Data/LOACharacterClassData.h"

// GA BP 종류
#include "GameplayAbilityBlueprint.h"

// 에디터 기능
#include "Kismet2/KismetEditorUtilities.h"
#include "Factories/AnimMontageFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UObjectIterator.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillFactory)

DEFINE_LOG_CATEGORY_STATIC(LogLOASkillFactory, Log, All);

namespace LOASkillFactory
{
	/** 모든 스킬 폴더의 뿌리 */
	static const TCHAR* SkillsRoot = TEXT("/Game/LOA/AbilitySystem/GA/Player");

	/** 클래스 데이터 에셋 이름의 앞부분. 이걸 떼면 클래스 이름 (DA_Class_Berserker → Berserker) */
	static const TCHAR* ClassDataPrefix = TEXT("DA_Class_");
}

// =============================================================================
// 스킬 만들기
// =============================================================================

bool ULOASkillFactory::CreateSkill(const ULOACharacterClassData* ClassData, const FString& SkillName,
	const FString& ParentClassName, UAnimSequence* Animation, FString& OutMessage)
{
	// 실패 처리: 메시지 저장 + 로그 + 빨간 알림 → false
	auto Fail = [&OutMessage](const FString& Reason)
	{
		OutMessage = Reason;
		UE_LOG(LogLOASkillFactory, Warning, TEXT("CreateSkill failed: %s"), *Reason);
		ShowNotification(Reason, false);
		return false;
	};

	// ---------- ① 입력 검사 ----------

	if (!ClassData)
	{
		return Fail(TEXT("Class Data is not set. Pick a DA_Class_* asset."));
	}

	// 클래스 이름 = 에셋 이름에서 "DA_Class_"를 뗀 것
	const FString ClassDataName = ClassData->GetName();
	if (!ClassDataName.StartsWith(LOASkillFactory::ClassDataPrefix))
	{
		return Fail(FString::Printf(TEXT("Class Data '%s' must be named DA_Class_<ClassName>."), *ClassDataName));
	}
	const FString ClassName = ClassDataName.RightChop(FCString::Strlen(LOASkillFactory::ClassDataPrefix));
	if (!IsValidNamePart(ClassName))
	{
		return Fail(FString::Printf(TEXT("Class name '%s' (from %s) must be letters and digits only, starting with a letter."),
			*ClassName, *ClassDataName));
	}

	// 스킬 이름: 앞뒤 공백은 실수로 보고 지운다. 가운데 공백이나 기호는 오류.
	const FString Skill = SkillName.TrimStartAndEnd();
	if (!IsValidNamePart(Skill))
	{
		return Fail(FString::Printf(TEXT("Skill Name '%s' must be letters and digits only, starting with a letter (e.g. Whirlwind)."),
			*SkillName));
	}

	UClass* ParentClass = FindSkillParentClass(ParentClassName);
	if (!ParentClass)
	{
		return Fail(FString::Printf(TEXT("Parent class '%s' is not a skill parent class. Pick one from the list."),
			*ParentClassName));
	}

	// ---------- ② 이름 정하기 ----------

	const FString Folder = FString::Printf(TEXT("%s/%s/%s"), LOASkillFactory::SkillsRoot, *ClassName, *Skill);
	const FString GAName = FString::Printf(TEXT("GA_%s_%s"), *ClassName, *Skill);
	const FString MontageName = FString::Printf(TEXT("M_%s_%s"), *ClassName, *Skill);
	const FString GAPackageName = Folder / GAName;
	const FString MontagePackageName = Folder / MontageName;

	// ---------- ③ 이미 있나 (하나라도 있으면 아무것도 만들지 않는다) ----------

	if (DoesAssetExist(GAPackageName))
	{
		return Fail(FString::Printf(TEXT("%s already exists. Nothing was created."), *GAPackageName));
	}
	if (Animation && DoesAssetExist(MontagePackageName))
	{
		return Fail(FString::Printf(TEXT("%s already exists. Nothing was created."), *MontagePackageName));
	}

	// ---------- ④ GA BP 생성 ----------
	// 콘텐츠 브라우저의 "Gameplay Ability Blueprint"로 만드는 것과 같은 종류(UGameplayAbilityBlueprint)로 만든다.

	UPackage* GAPackage = CreatePackage(*GAPackageName);
	UBlueprint* GABlueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass, GAPackage, FName(*GAName), BPTYPE_Normal,
		UGameplayAbilityBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!GABlueprint)
	{
		return Fail(FString::Printf(TEXT("Failed to create %s."), *GAPackageName));
	}

	// 콘텐츠 브라우저에 새 에셋이 생겼다고 알리고, 저장 대상으로 표시
	FAssetRegistryModule::AssetCreated(GABlueprint);
	GAPackage->MarkPackageDirty();

	TArray<UPackage*> PackagesToSave = { GAPackage };
	TArray<UObject*> CreatedAssets = { GABlueprint };

	// ---------- ⑤ 몽타주 생성 (애니메이션을 골랐을 때만) ----------

	UAnimMontage* NewMontage = nullptr;
	if (Animation)
	{
		// 몽타주 팩토리 = 콘텐츠 브라우저에서 애니메이션 우클릭 → "Create AnimMontage"와 같다.
		// 스켈레톤은 애니메이션의 것. 슬롯은 팩토리 기본값(DefaultSlot)
		UAnimMontageFactory* MontageFactory = NewObject<UAnimMontageFactory>();
		MontageFactory->SourceAnimation = Animation;
		MontageFactory->TargetSkeleton = Animation->GetSkeleton();

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		NewMontage = Cast<UAnimMontage>(AssetTools.CreateAsset(MontageName, Folder, UAnimMontage::StaticClass(), MontageFactory));

		if (NewMontage)
		{
			PackagesToSave.Add(NewMontage->GetPackage());
			CreatedAssets.Add(NewMontage);
		}
		else
		{
			// GA는 이미 만들었으므로 계속 진행한다. (몽타주는 나중에 직접 지정)
			UE_LOG(LogLOASkillFactory, Warning, TEXT("Failed to create montage %s. GA was created without a montage."),
				*MontagePackageName);
		}
	}

	// ---------- ⑥ 연결: GA BP의 기본값(CDO) Montage = 새 몽타주 ----------
	// GA BP의 Class Defaults에서 Montage 칸을 고르는 것과 같다.

	if (NewMontage)
	{
		ULOAGameplayAbility* GADefaults = GABlueprint->GeneratedClass
			? Cast<ULOAGameplayAbility>(GABlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (GADefaults)
		{
			GADefaults->Modify();	// 되돌리기(Ctrl+Z) 기록 + 저장 대상 표시
			GADefaults->SetMontageForEditor(NewMontage);
		}
	}

	// ---------- ⑦ 저장 → 콘텐츠 브라우저 이동 → GA 편집 창 ----------

	// bOnlyDirty = false: 방금 만든 패키지를 확실히 저장
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);

	if (GEditor)
	{
		GEditor->SyncBrowserToObjects(CreatedAssets);
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(GABlueprint);
	}

	// ---------- 결과 ----------

	OutMessage = FString::Printf(TEXT("Created %s (parent: %s)"), *GAPackageName, *ParentClass->GetName());
	if (Animation)
	{
		OutMessage += NewMontage
			? FString::Printf(TEXT("\nCreated %s (linked to GA Montage)"), *MontagePackageName)
			: FString(TEXT("\nMontage creation failed. Set the GA Montage manually."));
	}
	else
	{
		OutMessage += TEXT("\nNo animation selected: no montage. Set the GA Montage manually.");
	}
	if (!bSaved)
	{
		OutMessage += TEXT("\nSave failed. Save the new assets manually (Ctrl+Shift+S).");
	}

	UE_LOG(LogLOASkillFactory, Log, TEXT("%s"), *OutMessage);
	ShowNotification(OutMessage, true);
	return true;
}

// =============================================================================
// 부모 GA 목록
// =============================================================================

TArray<FString> ULOASkillFactory::GetSkillParentClassNames()
{
	TArray<FString> Names;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (IsSkillParentClass(*It))
		{
			Names.Add(It->GetName());
		}
	}

	// 이름 순으로 정렬한 뒤, 기본 스킬(LOAPlayerAbility)을 맨 앞으로 → 콤보 박스의 첫 항목 = 기본값
	Names.Sort();
	const FString BaseName = ULOAGameplayAbility::StaticClass()->GetName();
	if (Names.Remove(BaseName) > 0)
	{
		Names.Insert(BaseName, 0);
	}
	return Names;
}

bool ULOASkillFactory::IsSkillParentClass(const UClass* Class)
{
	// ULOAPlayerAbility 자신 또는 C++ 자식만.
	//   CLASS_Native     : C++ 클래스 (BP로 만든 GA는 빼서 목록이 GA BP로 넘치지 않게)
	//   Deprecated / NewerVersionExists : 지워졌거나 핫리로드로 교체된 옛 클래스는 뺀다
	// Abstract(UCLASS(Abstract))는 뺀다는 조건이 없다. → 우리 GA 부모들은 전부 Abstract라서 빼면 목록이 빈다.
	return Class
		&& Class->IsChildOf(ULOAGameplayAbility::StaticClass())
		&& Class->HasAnyClassFlags(CLASS_Native)
		&& !Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);
}

UClass* ULOASkillFactory::FindSkillParentClass(const FString& ParentClassName)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (IsSkillParentClass(*It) && It->GetName() == ParentClassName)
		{
			return *It;
		}
	}
	return nullptr;
}

// =============================================================================
// 도우미
// =============================================================================

bool ULOASkillFactory::IsValidNamePart(const FString& Name)
{
	if (Name.IsEmpty() || !FChar::IsAlpha(Name[0]))
	{
		return false;
	}
	for (const TCHAR Char : Name)
	{
		// 영문자/숫자만 (한글 등 ASCII 밖 문자도 막는다: 에셋 경로는 영문이 안전하다)
		if (Char > 127 || !FChar::IsAlnum(Char))
		{
			return false;
		}
	}
	return true;
}

bool ULOASkillFactory::DoesAssetExist(const FString& PackageName)
{
	// 디스크에 저장된 패키지 || 이번 에디터 실행 중 만들었지만 아직 저장 안 한 패키지(메모리)
	return FPackageName::DoesPackageExist(PackageName) || FindObject<UPackage>(nullptr, *PackageName) != nullptr;
}

void ULOASkillFactory::ShowNotification(const FString& Message, bool bSuccess)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = bSuccess ? 5.f : 8.f;	// 실패는 이유를 읽을 시간을 조금 더
	Info.bUseLargeFont = false;

	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}
