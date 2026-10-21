// LOAAbilityDataSet.cpp

#include "Data/LOAAbilityDataSet.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
// 언리얼 엔진 5.1부터 도입된 매크로로, 한 줄로 요약하면 "C++ 프로젝트의 전체 빌드(컴파일) 시간을 획기적으로 줄이기 위해 사용하는 컴파일 최적화 치트키"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAbilityDataSet)

#define LOCTEXT_NAMESPACE "LOAAbilityDataSet"

// =============================================================================
// 영수증
// =============================================================================

void FLOAAbilityDataSet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	// 유효하지 않은 핸들(부여 실패)은 기록하지 않는다.
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FLOAAbilityDataSet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	// 즉시형(Instant) 효과는 "적용되고 바로 끝나서" 유효한 핸들이 나오지 않는다. → 자연스럽게 기록에서 빠진다.
	// 지속형(Infinite/Duration) 효과만 기록되어 나중에 회수된다.
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FLOAAbilityDataSet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	if (Set)
	{
		GrantedAttributeSets.Add(Set);
	}
}

void FLOAAbilityDataSet_GrantedHandles::TakeFromAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	// 회수도 서버만 할 수 있다. (클라이언트가 스킬을 지워도 서버 기준으로는 그대로라 의미가 없고, 상태만 어긋난다)
	if (!ASC->IsOwnerActorAuthoritative())
	{
		LOA_LOG(ASC, Warning, TEXT("TakeFromAbilitySystem called without authority. Ignored."));
		return;
	}

	// ① 스킬 회수
	//    실행 중인 스킬이면 GAS가 스킬을 종료시키고 제거한다.
	//    (아이덴티티 교체는 "스킬 사용 중이 아닐 때만 허용"하는 규칙을 아이덴티티 관리 부품에서 따로 검사할 예정)
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	// ② 지속형 효과 제거 (패시브 등)
	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	// ③ 스탯창 제거
	//    RemoveSpawnedAttribute: 런타임에 추가한 스탯창을 ASC 목록에서 빼고, 클라이언트에도 제거가 복제된다.
	for (UAttributeSet* Set : GrantedAttributeSets)
	{
		if (Set)
		{
			ASC->RemoveSpawnedAttribute(Set);
		}
	}

	// 영수증 비우기 (같은 영수증으로 두 번 회수하는 사고 방지)
	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

bool FLOAAbilityDataSet_GrantedHandles::IsEmpty() const
{
	return AbilitySpecHandles.IsEmpty() && GameplayEffectHandles.IsEmpty() && GrantedAttributeSets.IsEmpty();
}

// =============================================================================
// 꾸러미 부여
// =============================================================================

void ULOAAbilityDataSetBase::GiveToAbilitySystem(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	if (!ASC)
	{
		LOA_LOG(this, Error, TEXT("GiveToAbilitySystem: ASC is null."));
		return;
	}

	// [서버 전용]
	// IsOwnerActorAuthoritative: ASC의 소유자(플레이어는 PlayerState)가 서버 쪽 객체인지 확인한다.
	// 리슨 서버 호스트 = 서버 → 통과 / 원격 클라이언트 → 차단
	if (!ASC->IsOwnerActorAuthoritative())
	{
		LOA_LOG(ASC, Warning, TEXT("GiveToAbilitySystem(%s) called without authority. Ignored."), *GetNameSafe(this));
		return;
	}

	// -------------------------------------------------------------------------
	// ① 스탯창 추가 (효과보다 먼저: 초기화 효과가 값을 넣을 그릇이 있어야 함)
	// -------------------------------------------------------------------------
	for (int32 Index = 0; Index < GrantedAttributeSets.Num(); ++Index)
	{
		const FLOAAbilityDataSet_AttributeSet& Entry = GrantedAttributeSets[Index];

		if (!Entry.AttributeSet)
		{
			LOA_LOG(this, Error, TEXT("GrantedAttributeSets[%d] is empty. Skipped."), Index);
			continue;
		}

		// 같은 종류의 스탯창이 이미 있으면 건너뛴다.
		// (예: 공통 스탯창을 실수로 꾸러미에 넣은 경우. 같은 스탯창이 두 개면 어느 쪽 값이 쓰일지 모호해진다)
		if (ASC->GetAttributeSet(Entry.AttributeSet))
		{
			LOA_LOG(this, Warning, TEXT("AttributeSet [%s] already exists on ASC. Skipped."), *GetNameSafe(Entry.AttributeSet));
			continue;
		}

		// 스탯창 객체는 ASC의 소유자(PlayerState)를 Outer로 만든다.
		// 소유자가 사라지면 스탯창도 함께 정리되고, 복제도 소유자를 통해 이루어진다.
		UAttributeSet* NewSet = NewObject<UAttributeSet>(ASC->GetOwner(), Entry.AttributeSet);

		// ASC에 등록 → 이후 효과/스킬이 이 스탯창의 값을 읽고 쓸 수 있다. 클라이언트에도 복제된다.
		ASC->AddAttributeSetSubobject(NewSet);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAttributeSet(NewSet);
		}
	}

	// -------------------------------------------------------------------------
	// ② 스킬 부여 (자식이 자기 줄로. 플레이어 = InputTag 포함, 몬스터 = 없음)
	// -------------------------------------------------------------------------
	GiveAbilities(ASC, OutGrantedHandles, SourceObject);

	// -------------------------------------------------------------------------
	// ③ 효과 적용 (초기 스탯 세팅, 패시브 등)
	// -------------------------------------------------------------------------
	for (int32 Index = 0; Index < GrantedEffects.Num(); ++Index)
	{
		const FLOAAbilityDataSet_GameplayEffect& Entry = GrantedEffects[Index];

		if (!Entry.GameplayEffect)
		{
			LOA_LOG(this, Error, TEXT("GrantedEffects[%d] is empty. Skipped."), Index);
			continue;
		}

		// 효과 컨텍스트: "이 효과가 어디서 왔는지" 정보를 담는 봉투.
		// 나중에 데미지 계산 등에서 출처를 확인할 때 쓰인다.
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(SourceObject);

		// 효과 클래스의 기본 객체(CDO)를 자기 자신에게 적용한다.
		const UGameplayEffect* EffectCDO = Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(EffectCDO, Entry.EffectLevel, Context);

		if (OutGrantedHandles)
		{
			// 즉시형 효과는 유효한 핸들이 없으므로 자동으로 기록에서 빠진다. (AddGameplayEffectHandle 참고)
			OutGrantedHandles->AddGameplayEffectHandle(Handle);
		}
	}
}

void ULOAAbilityDataSetBase::GiveOneAbility(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject,
	TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel, const FGameplayTag& InputTag, int32 Index) const
{
	if (!AbilityClass)
	{
		LOA_LOG(this, Error, TEXT("GrantedAbilities[%d] is empty. Skipped."), Index);
		return;
	}

	// 스킬 카드(Spec) 만들기: "어떤 스킬을, 몇 레벨로, 누가 줬는지"
	// InputID(세 번째 인자)는 사용하지 않는다. 우리는 숫자 ID 대신 입력 태그로 스킬을 찾는다.
	FGameplayAbilitySpec Spec(AbilityClass, AbilityLevel, INDEX_NONE, SourceObject);

	// 카드의 메모지에 "이 스킬은 어느 슬롯인지" 붙인다. (플레이어 꾸러미만 넘긴다)
	// 서버에서 붙인 메모지는 카드와 함께 본인 클라이언트로 복제되므로,
	// 클라이언트에서 Q를 눌렀을 때 ASC가 이 메모지로 스킬을 찾을 수 있다.
	if (InputTag.IsValid())
	{
		Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
	}

	// ASC에 카드 넣기. 반환되는 핸들 = 카드 번호표 (회수할 때 사용)
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);

	if (OutGrantedHandles)
	{
		OutGrantedHandles->AddAbilitySpecHandle(Handle);
	}
}

void ULOAPlayerAbilityDataSet::GiveAbilities(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
	{
		const FLOAPlayerAbilityDataSet_Ability& Entry = GrantedAbilities[Index];
		GiveOneAbility(ASC, OutGrantedHandles, SourceObject, Entry.Ability, Entry.AbilityLevel, Entry.InputTag, Index);
	}
}

void ULOAMonsterAbilityDataSet::GiveAbilities(UAbilitySystemComponent* ASC, FLOAAbilityDataSet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
	{
		const FLOAMonsterAbilityDataSet_Ability& Entry = GrantedAbilities[Index];
		GiveOneAbility(ASC, OutGrantedHandles, SourceObject, Entry.Ability, Entry.AbilityLevel, FGameplayTag(), Index);   // 몬스터는 키가 없다
	}
}

// =============================================================================
// 에디터 데이터 검증
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOAAbilityDataSetBase::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// --- 효과 ---
	for (int32 Index = 0; Index < GrantedEffects.Num(); ++Index)
	{
		if (!GrantedEffects[Index].GameplayEffect)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyEffect", "GrantedEffects[{0}]: GameplayEffect is empty."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// --- 스탯창 ---
	for (int32 Index = 0; Index < GrantedAttributeSets.Num(); ++Index)
	{
		if (!GrantedAttributeSets[Index].AttributeSet)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyAttributeSet", "GrantedAttributeSets[{0}]: AttributeSet is empty."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

EDataValidationResult ULOAPlayerAbilityDataSet::IsDataValid(FDataValidationContext& Context) const
{
	// 부모: 효과·스탯창
	EDataValidationResult Result = Super::IsDataValid(Context);

	// --- 스킬 ---
	TSet<FGameplayTag> SeenInputTags;
	for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
	{
		const FLOAPlayerAbilityDataSet_Ability& Entry = GrantedAbilities[Index];

		if (!Entry.Ability)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyAbility", "GrantedAbilities[{0}]: Ability is empty."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}

		// 입력 태그는 비어 있어도 된다 (키로 발동하지 않는 스킬). 비어 있지 않을 때만 중복 검사.
		if (Entry.InputTag.IsValid())
		{
			if (SeenInputTags.Contains(Entry.InputTag))
			{
				Context.AddError(FText::Format(
					LOCTEXT("DuplicateInputTag", "GrantedAbilities[{0}]: InputTag [{1}] is used more than once in this set."),
					FText::AsNumber(Index), FText::FromString(Entry.InputTag.ToString())));
				Result = EDataValidationResult::Invalid;
			}
			SeenInputTags.Add(Entry.InputTag);
		}
	}

	return Result;
}

EDataValidationResult ULOAMonsterAbilityDataSet::IsDataValid(FDataValidationContext& Context) const
{
	// 부모: 효과·스탯창
	EDataValidationResult Result = Super::IsDataValid(Context);

	// --- 스킬 ---
	for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
	{
		if (!GrantedAbilities[Index].Ability)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyMonsterAbility", "GrantedAbilities[{0}]: Ability is empty."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
