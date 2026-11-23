// LOASkillModuleDataInfos.h
// [배관] 스킬 모듈, Targeting, 전투 라이브러리가 주고받는 데이터 묶음. (코드 없음, 구조체만)
//
// ─────────────────────────────────────────────────────────────────────────────
// [여기 있는 것]
//   FLOASkillModuleContext     : "누가, 몇 레벨로" 썼나
//   FLOATargetHit        : "누가 맞았나 + 어디를"     (Targeting의 결과)
//   FLOAEffectPayload    : "무엇을" 줄지 = GE 하나 + 그 GE에 넣을 값들
//   FLOASetByCallerValue : Payload 안의 값 하나 (태그 + 숫자)
//
// [흐름 속 위치]
//
//   Apply 모듈 : Context = 주인 Ability->MakeSkillModuleContext()      ← FLOASkillModuleContext
//   Targeting  : FindTargets(Context) → 대상 목록                ← FLOATargetHit[]
//   라이브러리 : ApplyPayloads(Context, 대상 목록, Payloads)      ← FLOAEffectPayload[]
//
// [왜 Ability가 아니라 상자로 넘기나]
// 투사체, 장판, 보스 패턴도 같은 Targeting과 같은 효과 적용 코드를 써야 한다. 그런데 투사체는 Ability가 아니라서
// GetAvatarActorFromActorInfo() 같은 함수가 없다. → 필요한 정보만 상자에 담아 넘기면, 상자를 가진 쪽은 누구나 쓸 수 있다.
//   Spawn 모듈 : 소환할 때 주인 Ability의 상자를 투사체에 복사해 준다 → 투사체가 맞힐 때 그 상자로 ApplyPayloads
//
// ─────────────────────────────────────────────────────────────────────────────
// [Payload - "무엇을"]
// 데미지, 힐, 버프는 전부 "GE 하나를 대상에게 적용"이다. 다른 건 GE와 그 GE에 넣는 값뿐이다.
//
//   데미지      : Effect = GE_Damage_Basic      Values = [ SetByCaller.DamageRate = 2.5 ]
//   힐 (나중에) : Effect = GE_Heal              Values = [ SetByCaller.HealRate   = 1.2 ]
//   버프        : Effect = GE_Buff_AttackPower  Values = [ SetByCaller.Duration = 6, … ]
//
// → 새 효과가 필요하면 GE 에셋만 만든다. C++ 수정 없음.
// → GE가 SetByCaller 값을 읽지 않으면 Values는 비워 둬도 된다. (예: 값이 GE에 고정된 기절)
//
// [나중에 늘어날 칸] (그 단계에서 추가. 지금은 쓰는 곳이 없어서 넣지 않음)
//   FLOASkillModuleContext.TargetLocation   : 커서 지점 (커서 지점 범위 공격)

#pragma once

#include "CoreMinimal.h"
#include "ScalableFloat.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "Templates/SubclassOf.h"
#include "LOASkillModuleDataInfos.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UGameplayAbility;

// =============================================================================
// 누가 썼나 / 누가 맞았나
// =============================================================================

/**
 * 스킬 실행 정보. "누가, 몇 레벨로" 쓰는가.
 * Ability는 ULOAGameplayAbility::MakeSkillModuleContext()로 만든다.
 * 함수 안에서 잠깐 만들어 넘기는 용도. Ability나 모듈은 멤버로 들고 있지 않는다. (발동마다 값이 바뀐다)
 * 예외: 소환 액터(ALOASkillActor)는 소환 순간의 값을 복사해서 끝까지 갖고 있는다. (GA가 먼저 끝나도 맞힐 때 써야 하므로)
 */
USTRUCT(BlueprintType)
struct FLOASkillModuleContext
{
	GENERATED_BODY()

	/** 스킬을 쓴 쪽의 ASC. GE의 보낸 사람이 되고, 계산이 이 ASC의 공격력을 읽는다. */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	TObjectPtr<UAbilitySystemComponent> SourceASC = nullptr;

	/**
	 * 범위 기준 액터. 판정 모양을 놓을 위치/앞 방향의 기준. (적/아군 판단은 Caster가 한다)
	 *   Ability가 만들 때  : 내 캐릭터 (Avatar)       → 캐릭터 주변 범위
	 *   소환 액터가 가질 때 : 소환 액터 자신 (낫, 장판) → 그 액터 주변 범위 (ALOASkillActor::InitSkillActor가 바꾼다)
	 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	TObjectPtr<AActor> SourceActor = nullptr;

	/**
	 * 시전자. 적/아군 판단 기준 + "나"(Self Targeting, Ally And Self의 나)
	 *   항상 스킬을 쓴 캐릭터. 소환 액터가 가질 때도 바뀌지 않는다.
	 *   SourceActor와 나눈 이유: 소환 액터를 편 판단 기준으로 쓰면 PlayerState가 없어서 "몬스터 편"으로 판단된다.
	 *   → 내 낫이 파티원을 때리고 몬스터는 안 때린다. 그래서 "어디서 찾나"(SourceActor)와 "누구 편인가"(Caster)를 나눈다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	TObjectPtr<AActor> Caster = nullptr;

	/** 레벨. GE 레벨이 되고, Payload 값의 레벨 커브를 여기서 꺼낸다. (보통 Ability 레벨) */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	float Level = 1.f;

	/**
	 * 차지 배율. 모든 Payload GE에 SetByCaller.ChargeMultiplier로 들어간다.
	 * 차지 스킬이 아니면 1. 데미지 계산식(LOADamageExecution)이 곱하고, 읽지 않는 GE(버프 등)에는 영향 없음.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	float ChargeMultiplier = 1.f;

	/**
	 * 공격 속성. GA의 Asset Tags 중 Skill.Attribute 아래 태그만 담는다. (MakeSkillModuleContext가 채운다)
	 *   예) 카운터 스킬 → { Skill.Attribute.Counter } / 일반 스킬 → 비어 있음
	 * 상자째로 소환 액터에 복사되므로 투사체·장판으로 때려도 같은 속성이 간다.
	 * 쓰는 곳: ULOACombatLibrary::ApplyPayloads가 맞은 적에게 Event.Combat.Hit의 InstigatorTags로 실어 보낸다.
	 * 공격 쪽은 속성만 알린다. 그 속성으로 무슨 일이 생기는지(카운터 등)는 맞은 쪽이 판정한다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	FGameplayTagContainer AttackTags;

	/**
	 * 이 효과를 보낸 스킬 (GA 인스턴스). ApplyPayload가 GE의 Effect Context에 적는다 (FGameplayEffectContext::SetAbility). (2026-11-07)
	 * 쓰는 곳: 보호막 덮어쓰기 (ULOAGEComponent_ShieldOverride) — "같은 힐러가 같은 스킬로 건 보호막"을 찾을 때
	 * 비어 있으면(GA가 아닌 곳에서 만든 상자) 스킬을 모르는 효과로 간다
	 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	TObjectPtr<UGameplayAbility> SourceAbility = nullptr;

	/** 상자가 쓸 수 있는 상태인가 (ASC, 범위 기준 액터, 시전자가 모두 있음) */
	bool IsValid() const { return SourceASC != nullptr && SourceActor != nullptr && Caster != nullptr; }
};

/**
 * Targeting이 찾은 대상 하나.
 * 맞은 지점이 있으면 데미지 숫자가 그 자리에 뜬다. 없으면(자기 자신, 충돌체 없는 대상) 숫자 레이어의 기본 처리.
 */
USTRUCT(BlueprintType)
struct FLOATargetHit
{
	GENERATED_BODY()

	/** 대상 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	TObjectPtr<AActor> Target = nullptr;

	/** HitResult에 맞은 지점이 들어 있는가 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	bool bHasHitPoint = false;

	/** 맞은 지점 (bHasHitPoint일 때만 의미 있음) */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	FHitResult HitResult;

	// ---------- 공격 판정 기록(FLOAAttackJudgeRecord)을 정할 자리·시각 (2026-11-02, 시각 11-03) ----------
	// 비어 있으면(false) ApplyPayload가 지금 자리(때린 사람·맞은 쪽 액터 위치, 맞은 쪽 방향)·지금 시각으로 정한다. (보스 장판, 리슨 호스트 공격 등)
	// 공격자 판정(ULOAAttackJudgeSubsystem::JudgeReport)이 채운다: 때린 사람 = 보고한 내 자리, 맞은 쪽 = 내 화면에서 본 시각으로 되감은 자리·방향, 시각 = 내 화면이 보여 주던 서버 시각
	//   예) 내 화면에서는 보스 등 뒤였는데 그사이 서버의 보스가 30° 돌았다 → 되감은 방향으로 보면 "뒤" → 백 어택
	//   예) 내 화면에서는 파란 구간이었는데 도착했을 때 서버는 구간이 닫혔다 → 내 화면의 시각으로 보면 구간 안 → 카운터

	/** 아래 칸들이 채워져 있나 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	bool bHasDirectionBasis = false;

	/** 때린 사람 위치 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	FVector AttackerLocation = FVector::ZeroVector;

	/** 맞은 쪽 위치 */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	FVector TargetLocation = FVector::ZeroVector;

	/** 맞은 쪽이 보는 방향 (도) */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	float TargetYaw = 0.f;

	/** 판정 기준 서버 시각 (초). 공격자 판정 = 보고의 내 화면 서버 시각 (ViewServerTime, 한도로 자름) (2026-11-03) */
	UPROPERTY(BlueprintReadWrite, Category = "LOA|Skill")
	double AttackJudgeServerTime = 0.0;
};

// =============================================================================
// 무엇을
// =============================================================================

/**
 * Payload 안의 값 하나. "이 SetByCaller 칸에 이 숫자를 넣어라".
 * GE 에셋의 Modifier/Execution이 같은 태그로 값을 읽는다.
 */
USTRUCT(BlueprintType)
struct FLOASetByCallerValue
{
	GENERATED_BODY()

	/** SetByCaller 태그. GE가 읽는 태그와 같아야 한다. (예: SetByCaller.DamageRate) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Payload", meta = (Categories = "SetByCaller"))
	FGameplayTag Tag;

	/** 넣을 숫자. 커브를 지정하면 Context.Level에 따라 바뀐다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Payload")
	FScalableFloat Value;
};

/**
 * "무엇을" 줄지. GE 하나 + 그 GE에 넣을 값들.
 */
USTRUCT(BlueprintType)
struct FLOAEffectPayload
{
	GENERATED_BODY()

	/** 적용할 GE. 예: GE_Damage_Basic (Instant, Executions = ULOADamageExecution) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Payload")
	TSubclassOf<UGameplayEffect> Effect;

	/** GE에 넣을 값들. GE가 SetByCaller를 읽지 않으면 비워 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOA|Payload", meta = (TitleProperty = "Tag"))
	TArray<FLOASetByCallerValue> Values;
};
