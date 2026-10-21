// LOAAttributeSet_Mana.cpp

#include "AbilitySystem/Attributes/LOAAttributeSet_Mana.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAttributeSet_Mana)

ULOAAttributeSet_Mana::ULOAAttributeSet_Mana()
{
	// 초기값을 여기서 넣지 않는다. (하드코딩 금지) 직업 초기화 GE가 채운다.
}

void ULOAAttributeSet_Mana::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 셋 다 본인에게만. (다른 사람 마나는 화면에 안 보임 → 네트워크 절약)
	// REPNOTIFY_Always : 값이 같아도 OnRep 호출. 클라가 예측으로 먼저 깎아 둔 뒤 서버 값이 "같게" 도착해도 알림이 필요하다.
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Mana, Mana,      COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Mana, MaxMana,   COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Mana, ManaRegen, COND_OwnerOnly, REPNOTIFY_Always);
}

void ULOAAttributeSet_Mana::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetManaAttribute())
	{
		// 최대 마나가 아직 0이면(초기화 도중) 위쪽 제한을 걸지 않는다.
		// 초기화 GE의 순서가 "마나 → 최대 마나"여도 마나가 0으로 잘리지 않게. (Common의 체력과 같은 이유)
		const float Max = GetMaxMana();
		NewValue = (Max > 0.f) ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxManaAttribute() || Attribute == GetManaRegenAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void ULOAAttributeSet_Mana::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// [서버에서만 호출됨] PreAttributeChange는 보여지는 값만 맞춘다. 여기서는 저장된 기본값까지 맞춘다.
	if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		const float Max = GetMaxMana();
		SetMana((Max > 0.f) ? FMath::Clamp(GetMana(), 0.f, Max) : FMath::Max(GetMana(), 0.f));
	}
}

// ---------- 복제 도착 알림 ----------
// GAMEPLAYATTRIBUTE_REPNOTIFY : GAS에게 "서버 값이 도착했다"고 알린다. (UI 알림, 예측 값 보정)

void ULOAAttributeSet_Mana::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Mana, Mana, OldValue);
}

void ULOAAttributeSet_Mana::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Mana, MaxMana, OldValue);
}

void ULOAAttributeSet_Mana::OnRep_ManaRegen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Mana, ManaRegen, OldValue);
}
