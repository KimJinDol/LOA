// LOAGA_Identity_Aura.cpp

#include "AbilitySystem/Abilities/Player/LOAGA_Identity_Aura.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "Library/LOACombatLibrary.h"

#include "TimerManager.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAGA_Identity_Aura)

void ULOAGA_Identity_Aura::OnIdentityOn()
{
	// 켜는 순간 1번 + 간격마다
	ApplyAuraEffects();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AuraTimerHandle, this, &ThisClass::ApplyAuraEffects, AuraInterval, /*bLoop*/ true);
	}
}

void ULOAGA_Identity_Aura::OnIdentityOff()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraTimerHandle);
	}
}

void ULOAGA_Identity_Aura::ApplyAuraEffects()
{
	if (!IsActive() || !AuraTargeting)
	{
		return;
	}

	// 범위 기준·시전자 = 내 캐릭터
	const FLOASkillModuleContext Context = MakeSkillModuleContext();
	TArray<FLOATargetHit> Hits;
	AuraTargeting->FindTargets(Context, Hits);
	ULOACombatLibrary::ApplyPayloads(Context, Hits, AuraPayloads);
}
