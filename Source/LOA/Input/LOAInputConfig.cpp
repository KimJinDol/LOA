// LOAInputConfig.cpp

#include "Input/LOAInputConfig.h"
#include "Core/LOALog.h"
#include "InputAction.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAInputConfig)

#define LOCTEXT_NAMESPACE "LOAInputConfig"

const UInputAction* ULOAInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	// 목록을 처음부터 훑어서 태그가 "정확히" 같은 항목을 찾는다.
	// (MatchesTagExact: InputTag.Move 를 찾을 때 InputTag.Move.Something 같은 하위 태그는 제외)
	for (const FLOAInputAction& Entry : NativeInputActions)
	{
		if (Entry.InputAction && Entry.InputTag.MatchesTagExact(InputTag))
		{
			return Entry.InputAction;
		}
	}

	// 못 찾은 경우: 대부분 "에디터에서 연결표에 한 줄 추가하는 걸 깜빡한" 데이터 실수다.
	// 크래시 대신 경고 로그만 남기고 nullptr을 반환한다. (호출한 쪽은 nullptr이면 바인딩을 건너뛴다)
	if (bLogNotFound)
	{
		LOA_LOG(this, Warning, TEXT("Native InputAction not found for tag [%s]. Check %s."),
			*InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

#if WITH_EDITOR
EDataValidationResult ULOAInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// 두 목록에 같은 검사를 적용하기 위한 작은 도우미 함수
	auto ValidateList = [&Context, &Result](const TArray<FLOAInputAction>& List, const TCHAR* ListName)
	{
		TSet<FGameplayTag> SeenTags;

		for (int32 Index = 0; Index < List.Num(); ++Index)
		{
			const FLOAInputAction& Entry = List[Index];

			// 1) 입력 액션 칸이 비어 있음
			if (!Entry.InputAction)
			{
				Context.AddError(FText::Format(
					LOCTEXT("MissingAction", "{0}[{1}]: InputAction is empty."),
					FText::FromString(ListName), FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
			}

			// 2) 태그 칸이 비어 있음
			if (!Entry.InputTag.IsValid())
			{
				Context.AddError(FText::Format(
					LOCTEXT("MissingTag", "{0}[{1}]: InputTag is empty."),
					FText::FromString(ListName), FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			// 3) 같은 태그가 두 번 등록됨 → 한 키에 두 입력이 묶이는 실수
			if (SeenTags.Contains(Entry.InputTag))
			{
				Context.AddError(FText::Format(
					LOCTEXT("DuplicateTag", "{0}[{1}]: InputTag [{2}] is duplicated."),
					FText::FromString(ListName), FText::AsNumber(Index), FText::FromString(Entry.InputTag.ToString())));
				Result = EDataValidationResult::Invalid;
			}
			SeenTags.Add(Entry.InputTag);
		}
	};

	ValidateList(NativeInputActions, TEXT("NativeInputActions"));
	ValidateList(AbilityInputActions, TEXT("AbilityInputActions"));

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
