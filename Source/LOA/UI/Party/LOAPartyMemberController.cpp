// LOAPartyMemberController.cpp

#include "UI/Party/LOAPartyMemberController.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Character/Player/LOACharacter.h"
#include "Core/LOAGameplayTags.h"
#include "Data/LOACharacterClassData.h"
#include "UI/Status/LOAStatusTracker.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAPartyMemberController)

// =============================================================================
// 시작 / 끝
// =============================================================================

void ULOAPartyMemberController::Init(APlayerState* InPlayerState, bool bInIsLocalPlayer)
{
	Shutdown();

	PlayerState = InPlayerState;
	bIsLocalPlayer = bInIsLocalPlayer;
	if (!InPlayerState)
	{
		return;
	}

	// ASC는 PlayerState 소유 (모든 컴퓨터에 있다)
	if (UAbilitySystemComponent* InASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InPlayerState))
	{
		ASC = InASC;

		// 체력·최대 체력·보호막 중 무엇이 바뀌어도 체력바 분모가 바뀔 수 있다 (내 체력바와 같은 이유)
		const FGameplayAttribute HealthAttributes[] = {
			ULOAAttributeSet_Common::GetHealthAttribute(),
			ULOAAttributeSet_Common::GetMaxHealthAttribute(),
			ULOAAttributeSet_Common::GetShieldAttribute() };
		for (const FGameplayAttribute& Attribute : HealthAttributes)
		{
			AttributeChangedHandles.Emplace(Attribute,
				InASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleHealthChanged));
		}

		DeadTagChangedHandle = InASC->RegisterGameplayTagEvent(LOAGameplayTags::State_Body_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleDeadTagChanged);

		// 상태 아이콘은 Tracker가 (같은 ASC)
		if (!StatusTracker)
		{
			StatusTracker = NewObject<ULOAStatusTracker>(this);
		}
		StatusTracker->Init(InASC);
	}

	// 몸: 클라에서는 PlayerState보다 늦게 올 수 있다 → OnPawnSet (엔진: APawn::OnRep_PlayerState에서도 불림)
	InPlayerState->OnPawnSet.AddUniqueDynamic(this, &ThisClass::HandlePawnSet);
	BindCharacter(InPlayerState->GetPawn());
}

void ULOAPartyMemberController::BroadcastInitialValues()
{
	BroadcastInfo();
	BroadcastHealth();
	BroadcastDead();

	// 상태 아이콘: 아이콘 줄이 Tracker를 받을 때 지금 태그로 직접 그린다 (여기서 따로 보내지 않음)
}

void ULOAPartyMemberController::Shutdown()
{
	if (UAbilitySystemComponent* MyASC = ASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : AttributeChangedHandles)
		{
			MyASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
		MyASC->UnregisterGameplayTagEvent(DeadTagChangedHandle, LOAGameplayTags::State_Body_Dead, EGameplayTagEventType::NewOrRemoved);
	}

	if (StatusTracker)
	{
		StatusTracker->Shutdown();
	}

	if (APlayerState* MyPlayerState = PlayerState.Get())
	{
		MyPlayerState->OnPawnSet.RemoveDynamic(this, &ThisClass::HandlePawnSet);
	}

	if (ALOACharacter* Character = BoundCharacter.Get())
	{
		Character->OnCharacterInitialized.RemoveDynamic(this, &ThisClass::HandleCharacterInitialized);
	}

	AttributeChangedHandles.Reset();
	DeadTagChangedHandle.Reset();
	BoundCharacter.Reset();
	PlayerState.Reset();
	ASC.Reset();
}

// =============================================================================
// 이름표 (이름, 직업)
// =============================================================================

void ULOAPartyMemberController::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	BindCharacter(NewPawn);
}

void ULOAPartyMemberController::BindCharacter(APawn* Pawn)
{
	ALOACharacter* NewCharacter = Cast<ALOACharacter>(Pawn);
	if (BoundCharacter.Get() == NewCharacter)
	{
		return;
	}

	if (ALOACharacter* OldCharacter = BoundCharacter.Get())
	{
		OldCharacter->OnCharacterInitialized.RemoveDynamic(this, &ThisClass::HandleCharacterInitialized);
	}
	BoundCharacter = NewCharacter;

	if (!NewCharacter)
	{
		return;
	}

	if (NewCharacter->IsCharacterInitialized())
	{
		BroadcastInfo();
	}
	else
	{
		NewCharacter->OnCharacterInitialized.AddUniqueDynamic(this, &ThisClass::HandleCharacterInitialized);
	}
}

void ULOAPartyMemberController::HandleCharacterInitialized(ALOACharacter* InitializedCharacter)
{
	BroadcastInfo();
}

void ULOAPartyMemberController::BroadcastInfo() const
{
	const APlayerState* MyPlayerState = PlayerState.Get();
	if (!MyPlayerState)
	{
		return;
	}

	FText ClassName;
	UTexture2D* ClassIcon = nullptr;
	const ALOACharacter* Character = BoundCharacter.Get();
	if (const ULOACharacterClassData* ClassData = Character ? Character->GetCharacterClassData() : nullptr)
	{
		ClassName = ClassData->DisplayName;
		// 작은 텍스처 하나라 바로 읽는다 (직업 데이터는 이미 읽혀 있고, 아이콘은 Soft 참조)
		ClassIcon = ClassData->Icon.LoadSynchronous();
	}

	OnInfoChanged.Broadcast(FText::FromString(MyPlayerState->GetPlayerName()), ClassName, ClassIcon);
}

// =============================================================================
// 체력 · 사망
// =============================================================================

void ULOAPartyMemberController::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	BroadcastHealth();
}

void ULOAPartyMemberController::BroadcastHealth() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	if (!MyASC)
	{
		return;
	}

	const float Shield = MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetShieldAttribute());
	float HealthPercent = 0.f;
	float ShieldEndPercent = 0.f;
	ULOAOverlayWidgetController::ComputeHealthBar(
		MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute()),
		MyASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute()),
		Shield, HealthPercent, ShieldEndPercent);
	OnHealthBarChanged.Broadcast(HealthPercent, ShieldEndPercent, Shield);
}

void ULOAPartyMemberController::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	BroadcastDead();
}

void ULOAPartyMemberController::BroadcastDead() const
{
	const UAbilitySystemComponent* MyASC = ASC.Get();
	OnDeadChanged.Broadcast(MyASC && MyASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead));
}
