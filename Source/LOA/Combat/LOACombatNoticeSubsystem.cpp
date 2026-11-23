// LOACombatNoticeSubsystem.cpp

#include "Combat/LOACombatNoticeSubsystem.h"
#include "Library/LOACombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACombatNoticeSubsystem)

ULOACombatNoticeSubsystem* ULOACombatNoticeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOACombatNoticeSubsystem>() : nullptr;
}

bool ULOACombatNoticeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool ULOACombatNoticeSubsystem::IsServerWorld() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

FLOADamageAppliedInfo ULOACombatNoticeSubsystem::MakeDamageInfo(const FGameplayEffectModCallbackData& Data, float IncomingDamage, float RemovedAmount)
{
	// 맞은 지점: 히트 판정이 있는 공격이면 Context의 HitResult.
	//   주기 GE(도트)의 틱은 처음 걸 때의 HitResult가 그대로 남아 있을 수 있다. 그 위치는 이미 지난 위치라 쓰지 않는다.
	//   → 맞은 지점 없음으로 보내면 클라가 "내 캐릭터와 가장 가까운 점"에 띄운다.
	// 백 / 헤드 어택이었나 → 숫자 옆 글자, 치명타였나 → 노랗고 크게 (2026-11-13)
	//   ApplyPayload가 Effect Context에 적은 공격 판정 기록 (데미지 계산이 보너스·치명타를 정할 때 읽은 칸과 같다)
	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
	const bool bIsPeriodicTick = Data.EffectSpec.GetPeriod() > 0.f;

	FLOADamageAppliedInfo Info;
	Info.Instigator = Context.GetInstigator();
	Info.InstigatorASC = Context.GetInstigatorAbilitySystemComponent();
	Info.Target = Data.Target.GetAvatarActor();
	Info.TargetASC = &Data.Target;
	Info.IncomingDamage = IncomingDamage;
	Info.RemovedAmount = RemovedAmount;
	Info.HitResult = bIsPeriodicTick ? nullptr : Context.GetHitResult();
	Info.AttackJudgeRecord = ULOACombatLibrary::GetAttackJudgeRecordFromContext(Context);
	return Info;
}

void ULOACombatNoticeSubsystem::NotifyDamageApplied(const FLOADamageAppliedInfo& Info)
{
	if (!IsServerWorld())
	{
		return;
	}

	// 때린 사람의 구독자에게만 (예: 버서커가 때림 → 버서커 PC의 RPC 컴포넌트)
	if (const FLOADamageAppliedNative* Listeners = DamageDealtListeners.Find(Info.Instigator))
	{
		Listeners->Broadcast(Info);
	}

	// 맞은 사람의 구독자에게만 (예: 보스가 버서커를 때림 → 버서커 PC의 RPC 컴포넌트 → 빨간 숫자, 2026-11-16 apply118)
	//   열쇠 = 맞은 쪽 ASC의 주인 (플레이어면 PlayerState). 보스가 맞으면 구독자가 없다
	const AActor* Receiver = Info.TargetASC ? Info.TargetASC->GetOwnerActor() : nullptr;
	if (const FLOADamageAppliedNative* Listeners = DamageReceivedListeners.Find(Receiver))
	{
		Listeners->Broadcast(Info);
	}

	// 전부 구독자 (예: 레이드 기록)
	OnDamageApplied.Broadcast(Info);
}

void ULOACombatNoticeSubsystem::NotifyShieldAbsorbed(const FLOAShieldAbsorbedInfo& Info)
{
	if (!IsServerWorld())
	{
		return;
	}

	// 전부 구독자 (예: 레이드 기록 → 파티 방어)
	OnShieldAbsorbed.Broadcast(Info);
}

void ULOACombatNoticeSubsystem::NotifyHealApplied(const FLOAHealAppliedInfo& Info)
{
	if (!IsServerWorld())
	{
		return;
	}

	// 받은 사람의 구독자에게만 (예: 세라스가 버서커를 회복 → 버서커 PC의 RPC 컴포넌트 → 회복 숫자, 2026-11-16)
	if (const FLOAHealAppliedNative* Listeners = HealReceivedListeners.Find(Info.TargetOwner))
	{
		Listeners->Broadcast(Info);
	}

	// 전부 구독자 (예: 레이드 기록 → 파티 회복)
	OnHealApplied.Broadcast(Info);
}

FLOADamageAppliedNative& ULOACombatNoticeSubsystem::OnDamageDealtBy(const AActor* Instigator)
{
	return DamageDealtListeners.FindOrAdd(Instigator);
}

FLOAHealAppliedNative& ULOACombatNoticeSubsystem::OnHealReceivedBy(const AActor* Receiver)
{
	return HealReceivedListeners.FindOrAdd(Receiver);
}

FLOADamageAppliedNative& ULOACombatNoticeSubsystem::OnDamageReceivedBy(const AActor* Receiver)
{
	return DamageReceivedListeners.FindOrAdd(Receiver);
}
