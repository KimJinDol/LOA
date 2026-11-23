// LOACharacterSelection.cpp

#include "Data/LOACharacterSelection.h"
#include "Data/LOACharacterClassData.h"
#include "Party/LOALobbySettings.h"

#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACharacterSelection)

namespace LOALoginOption
{
	/** 접속 주소의 직업 칸 이름. ?Class=DA_Class_Berserker */
	static const TCHAR* Class = TEXT("Class");

	/** 이름에 못 쓰는 글자. 접속 주소(?Name=)와 콘솔 open 명령을 깬다 */
	static const TCHAR* ForbiddenNameChars = TEXT("?=#/\\:\" ");
}

FString FLOACharacterSelection::ToLoginOptions() const
{
	if (!IsValid())
	{
		return FString();
	}

	// 이름 부분만. 종류(CharacterClass)는 받는 쪽이 안다
	// 엔진이 앞에 '?'를 붙여 주소 뒤에 단다 (FURL::AddOption)
	return FString::Printf(TEXT("%s=%s"), LOALoginOption::Class, *ClassId.PrimaryAssetName.ToString());
}

FLOACharacterSelection FLOACharacterSelection::FromLoginOptions(const FString& Options)
{
	FLOACharacterSelection Selection;

	const FString ClassName = UGameplayStatics::ParseOption(Options, LOALoginOption::Class);
	if (!ClassName.IsEmpty())
	{
		Selection.ClassId = FPrimaryAssetId(ULOACharacterClassData::GetPrimaryAssetTypeName(), FName(*ClassName));
	}
	return Selection;
}

FString FLOACharacterSelection::MakeValidPlayerName(const FString& InName)
{
	FString Result;
	const FString Trimmed = InName.TrimStartAndEnd();
	for (const TCHAR Char : Trimmed)
	{
		// 탭·줄바꿈 같은 공백 글자와 못 쓰는 글자는 뺀다
		if (FChar::IsWhitespace(Char) || FCString::Strchr(LOALoginOption::ForbiddenNameChars, Char) != nullptr)
		{
			continue;
		}
		Result.AppendChar(Char);
	}
	return Result.Left(GetDefault<ULOALobbySettings>()->MaxPlayerNameLength);
}
