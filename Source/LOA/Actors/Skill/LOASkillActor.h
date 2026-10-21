// LOASkillActor.h
// [배관] 스킬이 소환하는 액터의 부모. (투사체, 나중에 장판/설치물)
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
// Spawn 모듈이 "언제, 어디에, 무엇을" 소환할지 정하고, 소환된 뒤의 일(날아가기, 충돌, 데미지)은 이 액터가 한다.
// 스킬(GA)이 먼저 끝나도 액터는 혼자 계속 동작해야 하기 때문이다. (화살이 날아가는 중에 다음 스킬을 쓴다)
//
//   이 부모 : 소환할 때 받은 "누가 썼나(Context) + 무엇을 줄까(Payloads)"를 보관한다.
//   자식    : 언제 누구에게 줄지 정한다.
//             투사체(ALOAProjectile) : 부딪힌 대상에게
//             범위(ALOAAreaActor)    : 정해진 시간에(한 번 또는 여러 번) 자기 주변 범위 안 대상에게 (낫, 장판)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (서버)
//
//   Spawn 모듈 : SpawnActorDeferred (아직 BeginPlay 전)
//                → InitSkillActor(Context, Payloads)   ← 값을 "복사"해서 넘긴다. 복사본의 SourceActor만 이 액터로 바꾼다.
//                → FinishSpawning → BeginPlay
//   자식       : 맞을 때 GetSkillContext() / GetSkillPayloads()로 ULOACombatLibrary::ApplyPayloads
//
//   복사하는 이유: 화살이 맞을 때는 GA가 이미 끝났을 수 있다. GA를 가리키면 그 값이 다음 발동 값으로 바뀌거나
//   비어 있다. (GA는 InstancedPerActor라 인스턴스를 재사용한다) → 소환 순간의 값을 액터가 따로 갖고 있어야 한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - 서버만 소환한다. (Spawn 모듈이 HasSkillModuleAuthority()일 때만 부른다)
// - bReplicates = true → 서버가 만든 액터가 모든 클라이언트에 복제된다. 클라에서는 겉모습만 보인다.
// - Context / Payloads는 복제하지 않는다. 판정과 데미지는 서버만 하므로 클라에는 필요 없다.
//   → 클라의 이 액터에서 GetSkillContext()는 빈 상자다. 자식은 판정 코드를 HasAuthority()일 때만 실행한다.
// - 클라 화면에는 서버에서 생긴 뒤 복제가 도착할 때(핑만큼) 나타난다. (예측 소환은 지금 하지 않음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [복사본의 SourceActor와 Caster]
//   SourceActor = 이 액터 자신   → 범위 판정(Shape Targeting)을 이 액터 위치/방향에 놓는다. (낫이 떨어진 자리)
//   Caster      = 시전자 그대로  → 적/아군 판단은 항상 시전자 기준. (PassesFilter(시전자, 대상))
//   Caster까지 이 액터로 바꾸면 안 되는 이유: 이 액터는 PlayerState가 없어서 "몬스터 편"으로 판단된다 → 파티원을 때림.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // Context / Payload를 값으로 가지므로 include
#include "LOASkillActor.generated.h"

/**
 * 스킬 소환 액터 부모.
 *   Abstract : 이 클래스 자체는 Spawn 모듈의 Actor Class 목록에 안 나옴 (자식 BP만)
 */
UCLASS(Abstract)
class LOA_API ALOASkillActor : public AActor
{
	GENERATED_BODY()

public:
	ALOASkillActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * [서버] 소환할 때 Spawn 모듈이 부른다. SpawnActorDeferred와 FinishSpawning 사이(BeginPlay 전)에 불러야
	 * 자식의 BeginPlay에서 값을 쓸 수 있다.
	 * @param InContext   누가, 몇 레벨로, 차지 배율 (소환 순간의 값을 복사)
	 * @param InPayloads  맞은 대상에게 줄 GE들 (복사)
	 */
	void InitSkillActor(const FLOASkillModuleContext& InContext, const TArray<FLOAEffectPayload>& InPayloads);

protected:
	/** [서버] 소환할 때 받은 실행 정보. 클라에서는 빈 상자 (복제 안 함) */
	const FLOASkillModuleContext& GetSkillContext() const { return SkillContext; }

	/** [서버] 소환할 때 받은 Payloads. 클라에서는 빈 배열 (복제 안 함) */
	const TArray<FLOAEffectPayload>& GetSkillPayloads() const { return SkillPayloads; }

	/** 시전자 (적/아군 판단 기준). = Context.Caster. 서버에서만 값이 있다. */
	AActor* GetSkillCaster() const { return SkillContext.Caster; }

	/**
	 * [서버] 지금 대상들에게 Payloads를 준다. 자식이 "맞았다"고 판단했을 때 부른다.
	 * 시전자의 ASC가 사라졌으면(시전자가 나감) 아무것도 하지 않는다.
	 */
	void ApplySkillPayloads(const TArray<FLOATargetHit>& Targets) const;

private:
	/**
	 * 소환할 때 받은 값.
	 * UPROPERTY인 이유: 안의 ASC / 시전자 포인터를 GC가 알아야 한다. (시전자가 사라지면 GC가 null로 만든다)
	 * 복제는 하지 않는다. (Replicated 아님)
	 */
	UPROPERTY(Transient)
	FLOASkillModuleContext SkillContext;

	UPROPERTY(Transient)
	TArray<FLOAEffectPayload> SkillPayloads;
};
