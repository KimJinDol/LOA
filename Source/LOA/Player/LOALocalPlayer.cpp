// LOALocalPlayer.cpp

#include "Player/LOALocalPlayer.h"
#include "Data/LOACharacterClassData.h"
#include "Core/LOALog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOALocalPlayer)

void ULOALocalPlayer::SetSelectedClass(const ULOACharacterClassData* ClassData)
{
	if (!ClassData || ClassData == SelectedClassData)
	{
		return;
	}

	SelectedClassData = ClassData;
	Selection.ClassId = ClassData->GetPrimaryAssetId();

	LOA_LOG(this, Log, TEXT("[Lobby] Selected class [%s]."), *Selection.ClassId.ToString());
	OnSelectedClassChanged.Broadcast(ClassData);
}

FString ULOALocalPlayer::GetGameLoginOptions() const
{
	// 안 골랐으면 빈 글자 → 서버는 TestClasses로 (PIE 다인)
	return Selection.ToLoginOptions();
}

bool ULOALocalPlayer::SetPlayerName(const FString& InName)
{
	const FString ValidName = FLOACharacterSelection::MakeValidPlayerName(InName);
	if (ValidName.IsEmpty())
	{
		LOA_LOG(this, Warning, TEXT("[Lobby] Invalid player name '%s'."), *InName);
		return false;
	}

	Selection.PlayerName = ValidName;
	LOA_LOG(this, Log, TEXT("[Lobby] Player name [%s]."), *ValidName);
	return true;
}

FString ULOALocalPlayer::GetNickname() const
{
	return Selection.PlayerName.IsEmpty() ? Super::GetNickname() : Selection.PlayerName;
}
