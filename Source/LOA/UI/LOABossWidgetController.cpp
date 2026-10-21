// LOABossWidgetController.cpp

#include "UI/LOABossWidgetController.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Core/LOALog.h"

#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOABossWidgetController)

void ULOABossWidgetController::Init(ALOABossCharacter* InBoss)
{
	// 혹시 두 번 불리면 이전 알림부터 정리한다. (같은 알림이 두 번 걸리면 위젯도 두 번 갱신됨)
	Shutdown();

	Boss = InBoss;
	UAbilitySystemComponent* BossASC = InBoss ? InBoss->GetAbilitySystemComponent() : nullptr;
	ASC = BossASC;

	if (!BossASC)
	{
		LOA_LOG(InBoss, Warning, TEXT("[BossWidgetController] Boss or boss ASC is null. Boss health bar will not update."));
		return;
	}

	// 현재 값과 최대 값 중 어느 쪽이 바뀌어도 바를 다시 그린다. (Overlay 컨트롤러와 같은 이유)
	HealthChangedHandle = BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
	MaxHealthChangedHandle = BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
}

void ULOABossWidgetController::BroadcastInitialValues()
{
	if (const ALOABossCharacter* MyBoss = Boss.Get())
	{
		OnBossInfoChanged.Broadcast(MyBoss->GetBossDisplayName(), MyBoss->GetHealthBarLines());
	}

	BroadcastHealth();
}

void ULOABossWidgetController::Shutdown()
{
	// 보스가 이 컨트롤러보다 먼저 사라지면 ASC도 없다 → 해제할 것도 없다. (약한 참조라 안전)
	if (UAbilitySystemComponent* BossASC = ASC.Get())
	{
		BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetHealthAttribute()).Remove(HealthChangedHandle);
		BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	Boss.Reset();
	ASC.Reset();
}

void ULOABossWidgetController::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// 어느 스탯이 바뀌었든 현재/최대를 둘 다 다시 읽는다. (Data에는 바뀐 쪽 값만 있다)
	BroadcastHealth();
}

void ULOABossWidgetController::BroadcastHealth() const
{
	const UAbilitySystemComponent* BossASC = ASC.Get();
	if (!BossASC)
	{
		return;
	}

	OnBossHealthChanged.Broadcast(
		BossASC->GetNumericAttribute(ULOAAttributeSet_Common::GetHealthAttribute()),
		BossASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute()));
}
