// LOABossWidgetController.cpp

#include "UI/LOABossWidgetController.h"
#include "UI/Status/LOAStatusTracker.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Boss.h"
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

	// 무력화 게이지 (2026-11-18 apply120). 무력화가 없는 보스여도 알림은 건다 (값이 안 바뀌니 안 울릴 뿐)
	//   클라에서는 꾸러미로 붙는 무력화 스탯창이 아직 복제 전일 수 있다 → ASC 알림은 스탯창이 아니라 칸 이름으로 걸리므로 도착하면 울린다
	StaggerChangedHandle = BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Boss::GetStaggerAttribute())
		.AddUObject(this, &ThisClass::HandleStaggerChanged);
	MaxStaggerChangedHandle = BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Boss::GetMaxStaggerAttribute())
		.AddUObject(this, &ThisClass::HandleStaggerChanged);

	// 상태 아이콘. 남은 시간은 보스의 시간 복제 부품에서 (Minimal ASC라 클라에 GE가 없음)
	if (!StatusTracker)
	{
		StatusTracker = NewObject<ULOAStatusTracker>(this);
	}
	StatusTracker->Init(BossASC, InBoss->GetStatusReplicationComponent());
}

void ULOABossWidgetController::BroadcastInitialValues()
{
	if (const ALOABossCharacter* MyBoss = Boss.Get())
	{
		OnBossInfoChanged.Broadcast(MyBoss->GetBossDisplayName(), MyBoss->GetHealthBarLines());
	}

	BroadcastHealth();
	BroadcastStagger();
}

void ULOABossWidgetController::Shutdown()
{
	// 보스가 이 컨트롤러보다 먼저 사라지면 ASC도 없다 → 해제할 것도 없다. (약한 참조라 안전)
	if (UAbilitySystemComponent* BossASC = ASC.Get())
	{
		BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetHealthAttribute()).Remove(HealthChangedHandle);
		BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Boss::GetStaggerAttribute()).Remove(StaggerChangedHandle);
		BossASC->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Boss::GetMaxStaggerAttribute()).Remove(MaxStaggerChangedHandle);
	}

	if (StatusTracker)
	{
		StatusTracker->Shutdown();
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	StaggerChangedHandle.Reset();
	MaxStaggerChangedHandle.Reset();
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

void ULOABossWidgetController::HandleStaggerChanged(const FOnAttributeChangeData& Data)
{
	// 체력과 같은 이유로 현재/최대를 둘 다 다시 읽는다
	BroadcastStagger();
}

void ULOABossWidgetController::BroadcastStagger() const
{
	const UAbilitySystemComponent* BossASC = ASC.Get();
	if (!BossASC)
	{
		return;
	}

	// 무력화 스탯창이 없으면(무력화 없는 보스, 클라에 아직 복제 전) GetNumericAttribute가 0 → (0, 0) → 위젯이 숨긴다
	OnBossStaggerChanged.Broadcast(
		BossASC->GetNumericAttribute(ULOAAttributeSet_Boss::GetStaggerAttribute()),
		BossASC->GetNumericAttribute(ULOAAttributeSet_Boss::GetMaxStaggerAttribute()));
}
