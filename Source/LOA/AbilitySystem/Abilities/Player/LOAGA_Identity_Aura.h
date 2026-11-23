// LOAGA_Identity_Aura.h
// [콘텐츠] 오라형 아이덴티티. 켜진 동안 일정 간격으로 범위 안 대상에게 효과를 준다. (2026-11-09)
//   첫 사용: 세라스(서포터) "축복의 오라" = 1초마다 나 주변 아군+나 회복 (GA_sp_Identity)
//   켜기·끄기·게이지는 부모 ULOAGA_Identity
//
//   켜는 순간 1번 + AuraInterval마다 AuraTargeting 범위 안 대상에게 AuraPayloads (서버)
//   이 효과로는 게이지가 안 찬다 (켜진 동안은 State.Identity.Active로 막힘)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/LOAGA_Identity.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"
#include "LOAGA_Identity_Aura.generated.h"

class ULOASkillTargeting;

UCLASS()
class LOA_API ULOAGA_Identity_Aura : public ULOAGA_Identity
{
	GENERATED_BODY()

protected:
	//~ Begin ULOAGA_Identity Interface
	/** 효과 1번 + 간격 타이머 시작 */
	virtual void OnIdentityOn() override;
	/** 타이머 정리 */
	virtual void OnIdentityOff() override;
	//~ End ULOAGA_Identity Interface

	/** 오라 범위 (예: Shape 원 반경 800, Filter = Ally And Self). 기준은 내 캐릭터 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "LOA|Identity|Aura")
	TObjectPtr<ULOASkillTargeting> AuraTargeting;

	/** 오라가 간격마다 주는 효과 (예: GE_Heal + SetByCaller.HealRate 0.2) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Identity|Aura")
	TArray<FLOAEffectPayload> AuraPayloads;

	/** 효과를 주는 간격 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Identity|Aura", meta = (ClampMin = "0.1", Units = "Seconds"))
	float AuraInterval = 1.f;

private:
	/** [서버] 범위 안 대상에게 AuraPayloads */
	void ApplyAuraEffects();

	FTimerHandle AuraTimerHandle;
};
