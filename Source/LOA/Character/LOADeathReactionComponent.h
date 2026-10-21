// LOADeathReactionComponent.h
// [배관] 몸의 사망 반응 부품. State.Dead 태그가 붙으면 몸을 "시체"로 만들고, 떨어지면 되돌린다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할 분담 - 사망]
//   Common 스탯창   : 체력이 처음 0이 됨 → Event.Death              (알리기만)
//   GA_Death        : 다른 Ability 취소, State.Dead 부여, 사망 몽타주   (규칙, 서버)
//   AnimInstance    : State.Dead → bIsDead → 시체 포즈                 (모습, 모든 컴퓨터)
//   이 부품         : State.Dead → 충돌 끄기, 이동 멈춤, 알림            (몸, 모든 컴퓨터)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 모든 컴퓨터에서 하나]
// 충돌 설정은 복제되지 않는다. 서버에서만 캡슐을 끄면 클라이언트 캐릭터는 자기 이동 예측에서
// 시체 캡슐에 계속 막히고, 서버는 통과시켜서 위치 보정(끌려가는 느낌)이 생긴다.
// → 복제되는 태그(State.Dead)를 각 컴퓨터가 보고 각자 끈다.
//
// [흐름]
//   몸이 ASC 준비 직후 InitializeWithAbilitySystem(ASC) [모든 컴퓨터]
//     ├─ State.Dead 태그 변화 구독
//     └─ 이미 붙어 있으면(늦게 들어온 플레이어) 바로 사망 반응
//   State.Dead 붙음  → 루트 충돌체 충돌 끄기, 이동 멈춤 → OnDied
//   State.Dead 떨어짐 → 충돌·이동 되돌리기              → OnRevived   (부활이 생기면 쓰임)
//   EndPlay           → 구독 해제
//
// [다른 곳이 이 부품을 쓰는 법]
//   RaidSubsystem : 보스의 OnDied → OnBossDefeated (토벌 완료)
//   (나중에) AI   : OnDied → StateTree 정지
//   BP            : OnDied에 연출(소리, 이펙트)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// 이 부품은 복제하지 않는다. 컴퓨터마다 자기 ASC의 태그를 보고 각자 반응한다. (태그는 ASC가 복제)
// OnDied / OnRevived는 그 컴퓨터 안에서만 알린다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것]
//   몸 BP의 이 부품 → LOA|Death : Disable Collision On Death, Stop Movement On Death (기본 둘 다 켬)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "LOADeathReactionComponent.generated.h"

class UAbilitySystemComponent;

/** 몸이 죽었다 / 살아났다. (이 컴퓨터 안에서만) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOADeathReactionSignature, AActor*, OwnerActor);

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOADeathReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOADeathReactionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * State.Dead 구독을 시작한다. 몸이 ASC 준비 직후 부른다. [모든 컴퓨터]
	 * 이미 죽어 있으면 바로 사망 반응을 하고 OnDied를 알린다.
	 * 다른 ASC로 다시 부르면 이전 구독을 지우고 새로 구독한다.
	 */
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	/** 구독을 끊는다. EndPlay에서 자동으로 부른다. */
	void UninitializeFromAbilitySystem();

	/** 지금 죽어 있나 (이 컴퓨터가 State.Dead를 받은 상태) */
	UFUNCTION(BlueprintPure, Category = "LOA|Death")
	bool IsDead() const { return bIsDead; }

	/** 죽었다. 충돌·이동 처리가 끝난 뒤 알린다. */
	UPROPERTY(BlueprintAssignable, Category = "LOA|Death")
	FLOADeathReactionSignature OnDied;

	/** 살아났다. (부활이 생기면 쓰임) 충돌·이동을 되돌린 뒤 알린다. */
	UPROPERTY(BlueprintAssignable, Category = "LOA|Death")
	FLOADeathReactionSignature OnRevived;

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ---------- 데이터 (몸 BP에서 조정) ----------

	/** 죽으면 몸의 루트 충돌체(캐릭터면 캡슐)를 끈다. → 다른 캐릭터가 통과, 판정 후보 검색에도 안 잡힘 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Death")
	bool bDisableCollisionOnDeath = true;

	/** 죽으면 이동을 멈추고 막는다. (캐릭터일 때만. 걷던 중이면 그 자리에 선다) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Death")
	bool bStopMovementOnDeath = true;

private:
	/** State.Dead 개수가 0 ↔ 1 이상으로 바뀔 때 ASC가 부른다. */
	void OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 시체로 만들기: 충돌 끄기, 이동 멈춤 → OnDied */
	void HandleDied();

	/** 되돌리기: 충돌·이동 복구 → OnRevived */
	void HandleRevived();

	/** 구독 중인 ASC (몸이 사라지면 자동으로 비는 약한 참조) */
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	/** State.Dead 구독 번호표 (해제할 때 씀) */
	FDelegateHandle DeadTagChangedHandle;

	/** 지금 죽어 있나. 같은 반응을 두 번 하지 않도록 기억한다. */
	bool bIsDead = false;

	/** 죽기 전 루트 충돌 설정. 되살아날 때 이 값으로 되돌린다. */
	TEnumAsByte<ECollisionEnabled::Type> CollisionBeforeDeath = ECollisionEnabled::NoCollision;
};
