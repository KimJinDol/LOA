// LOAGEComponent_ShieldOverride.h
// [배관] GE 컴포넌트 "Shield Override". 같은 힐러가 같은 스킬로 보호막을 다시 걸면 예전 보호막을 지운다 = 덮어쓰기. (2026-11-07)
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙] (사용자 결정 2026-11-07)
//   보호막은 스킬마다 따로 (각자 양·시간). 다른 스킬이면 쌓이고, 같은 스킬이면 덮어쓴다 (더하지 않음)
//   예) A 500(3초) → 1초에 A 다시 → 예전 A를 지우고 새 A 500, 시간도 다시 3초 (1000이 아님)
//       A 500 + B 300 → 800 (다른 스킬이라 둘 다 남음)
//
// [같은 스킬인가] Effect Context로 판단한다. 스킬 데이터에 따로 적지 않는다
//   보낸 사람 (GetInstigator) + 보낸 스킬 (GetAbility, ApplyPayload가 FLOASkillModuleContext::SourceAbility로 적음)
//   보낸 스킬을 모르는 보호막(GA가 아닌 곳에서 걸림)은 덮어쓰지 않는다 (쌓인다)
//
// [언제] OnGameplayEffectApplied: 처음 걸릴 때와 다시 걸릴 때 매번 서버에서 불린다 (복제로는 안 불림, GameplayEffectComponent.h)
//   이때 새 보호막 GE는 이미 붙어 있고, 넘어오는 Spec이 바로 그 GE의 Spec이다 (AbilitySystemComponent.cpp ApplyGameplayEffectSpecToSelf)
//   → 같은 GE 에셋의 보호막 중 "Spec 주소가 다른 것" = 예전 것만 지운다
//   엔진의 URemoveOtherGameplayEffectComponent(걸릴 때 다른 GE 지우기)와 같은 자리
//
// [쓰는 법] GE_Shield
//   Duration    : Has Duration, SetByCaller.Duration (스킬 데이터)
//   Modifiers   : Shield / Add / Custom Calculation Class = LOAShieldMagnitude
//   Components  : Target Tags (State.Buff.Shield) + 이 부품 (Shield Override (LOA))
//   Stacking    : 없음 (걸 때마다 따로 붙어야 스킬마다 따로 셈)
//   저장 시 검사: Instant면 경고 (붙어 있지 않아 덮어쓸 게 없음)
//
// [네트워크] 서버만

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "LOAGEComponent_ShieldOverride.generated.h"

UCLASS(DisplayName = "Shield Override (LOA)")
class LOA_API ULOAGEComponent_ShieldOverride : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	//~ Begin UGameplayEffectComponent Interface
	/** [서버] 걸릴 때마다: 같은 힐러 + 같은 스킬의 예전 보호막 GE를 지운다 */
	virtual void OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const override;
	//~ End UGameplayEffectComponent Interface

#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사: 주인 GE가 Instant면 경고 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
