// LOAAttributeSet_Boss.cpp

#include "AbilitySystem/Attributes/LOAAttributeSet_Boss.h"
#include "Library/LOACombatLibrary.h"
#include "Combat/LOACombatNoticeSubsystem.h"
#include "Core/LOAGameplayTags.h"
#include "Core/LOALog.h"
#include "GameplayEffectExtension.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAttributeSet_Boss)

ULOAAttributeSet_Boss::ULOAAttributeSet_Boss()
{
	// 초기값을 여기서 넣지 않는다. (하드코딩 금지)
	// 보스 초기화 GE가 MaxStagger, Stagger를 채운다.
}

void ULOAAttributeSet_Boss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 무력화 게이지: 모두에게 복제 (보스 체력바 아래 게이지). 체력과 같은 이유로 REPNOTIFY_Always
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Boss, Stagger,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULOAAttributeSet_Boss, MaxStagger, COND_None, REPNOTIFY_Always);
}

void ULOAAttributeSet_Boss::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetStaggerAttribute())
	{
		// 최대가 아직 0이면(초기화 도중) 위쪽 제한을 걸지 않는다 (Common의 체력과 같은 이유: 초기화 순서가 Stagger → MaxStagger여도 안 잘리게)
		const float Max = GetMaxStagger();
		NewValue = (Max > 0.f) ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxStaggerAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

bool ULOAAttributeSet_Boss::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	// [서버] 적용 직전 값. Post에서 "0보다 컸는데 0이 됨"을 판단하는 기준
	StaggerBeforeExecute = GetStagger();
	return Super::PreGameplayEffectExecute(Data);
}

void ULOAAttributeSet_Boss::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// [서버] 대난투 입구 (메타, 2026-11-18 apply125). 체력은 그대로, 알림만 (헤더 [칸])
	if (Data.EvaluatedData.Attribute == GetBrawlDamageAttribute())
	{
		const float IncomingDamage = GetBrawlDamage();
		SetBrawlDamage(0.f);

		// 맞을 수 없는 대상이면 버린다 (Common의 데미지 입구와 같은 규칙)
		if (IncomingDamage <= 0.f || !ULOACombatLibrary::IsTargetable(Data.Target.GetAvatarActor()))
		{
			return;
		}

		// 깎인 양 = 들어온 피해 전체 (체력은 돌아온 뒤 경기장이 누적만큼 깎는다. 마지막 한 방이 남은 체력보다 커도 그대로)
		// 알림 정보는 알림판이 만들고, 이 입구의 사정(대난투 피해)만 덧붙인다
		if (ULOACombatNoticeSubsystem* CombatNotice = ULOACombatNoticeSubsystem::Get(&Data.Target))
		{
			FLOADamageAppliedInfo Info = ULOACombatNoticeSubsystem::MakeDamageInfo(Data, IncomingDamage, IncomingDamage);
			Info.bBrawlDamage = true;
			CombatNotice->NotifyDamageApplied(Info);
		}
		return;
	}

	// [서버] 무력화 입구 (메타). ULOAGE_Stagger가 넣어 준 값을 꺼내서 Stagger를 깎는다
	if (Data.EvaluatedData.Attribute != GetStaggerDamageAttribute())
	{
		return;
	}

	// ① 꺼내고 즉시 비운다 (입구일 뿐 보관하는 값이 아니다)
	const float IncomingStagger = GetStaggerDamage();
	SetStaggerDamage(0.f);

	// ② 버리는 경우
	//    맞을 수 없는 대상(죽음, 숨음): 판정이 대상을 고를 때와 같은 규칙 (ULOACombatLibrary::IsTargetable)
	//    이미 0 (다 깎인 뒤 다시 채우기 전): 예) 대난투 중 → 게이지는 0 그대로, 이벤트도 다시 안 감
	if (IncomingStagger <= 0.f || StaggerBeforeExecute <= 0.f || !ULOACombatLibrary::IsTargetable(Data.Target.GetAvatarActor()))
	{
		return;
	}

	// ③ 깎기. 0 아래로 안 내려간다
	SetStagger(FMath::Max(StaggerBeforeExecute - IncomingStagger, 0.f));

	LOA_LOG(Data.Target.GetAvatarActor(), Log, TEXT("[Stagger] -%.1f (%.1f -> %.1f / %.1f) by %s"),
		IncomingStagger, StaggerBeforeExecute, GetStagger(), GetMaxStagger(), *GetNameSafe(Data.EffectSpec.GetContext().GetInstigator()));

	// ④ 처음 0이 됐으면 알린다 (누가 듣는지는 모른다)
	SendStaggerBrokenEventIfJustBroken(Data);
}

void ULOAAttributeSet_Boss::SendStaggerBrokenEventIfJustBroken(const FGameplayEffectModCallbackData& Data) const
{
	// "처음 0이 됨"만. 예) 20 → 0 이면 보냄 / 0 → 0 은 위 ②에서 이미 버림
	if (StaggerBeforeExecute <= 0.f || GetStagger() > 0.f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// 받는 쪽(대난투 GA 예정)이 TriggerEventData로 읽는다
	//   Instigator : 마지막으로 무력화를 깎은 쪽 / Target : 보스
	FGameplayEventData Payload;
	Payload.EventTag = LOAGameplayTags::Event_Trigger_StaggerBroken;
	Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
	Payload.Target = Data.Target.GetAvatarActor();
	Payload.ContextHandle = Data.EffectSpec.GetContext();

	LOA_LOG(Payload.Target.Get(), Log, TEXT("[Stagger] Broken. Sending Event.Trigger.StaggerBroken (last: %s)"),
		*GetNameSafe(Payload.Instigator.Get()));

	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
}

void ULOAAttributeSet_Boss::OnRep_Stagger(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Boss, Stagger, OldValue);
}

void ULOAAttributeSet_Boss::OnRep_MaxStagger(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULOAAttributeSet_Boss, MaxStagger, OldValue);
}
