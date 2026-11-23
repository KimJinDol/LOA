// LOATrainingDummy.h
// [배관] 데미지 확인용 허수아비. 맞으면 체력이 줄고, 로그로 보여 주고, 잠시 뒤 체력이 다시 찬다. 죽지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [플레이어와 다른 점 - ASC 위치]
//
//   플레이어 : ASC가 PlayerState에 있다 (Owner = PlayerState, Avatar = 캐릭터)
//              → 캐릭터가 죽고 부활해도 쿨타임/스탯이 남아야 하므로 + Spec을 다시 부여하지 않기 위해
//   허수아비 : ASC가 자기 자신에 있다 (Owner = Avatar = 허수아비)
//              → 부활도, PlayerState도 없다. 보스/몬스터도 이 방식을 쓸 예정
//
// ─────────────────────────────────────────────────────────────────────────────
// [초기 체력 - 하드코딩 없음]
// 플레이어와 같은 방식으로 AbilityDataSet의 초기화 GE로 넣는다.
//   DA_AbilityDataSet_Dummy → Effects: GE_Init_Dummy (MaxHealth, Health Override)
// 레벨에 배치한 허수아비의 디테일 패널 → AbilityDataSets에 지정한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   BeginPlay (서버/클라이언트 모두)
//     ├─ InitAbilityActorInfo(this, this)
//     ├─ 체력 변경 알림 등록
//     └─ [서버] AbilityDataSets 부여 → 초기화 GE → 체력 채움
//
//   피격 (서버에서 데미지 GE 적용)
//     ├─ [서버] 체력 감소 → 알림 → 로그 + 회복 타이머 재시작
//     └─ [클라이언트] 체력 복제 도착 → 알림 → 로그 (복제 확인용)
//
//   마지막 피격 후 ResetDelay초 → [서버] 체력을 최대치로 → 복제
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - 레벨에 배치된 액터도 bReplicates = true면 모든 클라이언트에 복제된다.
// - 체력은 COND_None이라 모든 클라이언트가 받는다. (마나 스탯창은 플레이어만 가진다. 허수아비는 없음)
// - ASC 복제 모드 Minimal: 허수아비의 버프 상세 정보는 아무에게도 필요 없다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "LOATrainingDummy.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class ULOABodyBlockerComponent;
class ULOAAbilitySystemComponent;
class ULOAAttributeSet_Common;
class ULOAAttributeSet_Combat;
class ULOAMonsterAbilityDataSet;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class LOA_API ALOATrainingDummy : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALOATrainingDummy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin IAbilitySystemInterface
	/** 데미지 GE를 적용하는 쪽이 이 함수로 허수아비의 ASC를 찾는다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

private:
	/** 체력이 바뀔 때마다 호출. (서버: 즉시 / 클라이언트: 복제 도착 시) */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** [서버] 체력을 최대치로 되돌린다. */
	void ResetHealth();

	// ---------- 컴포넌트 ----------
	/** 피격 판정용 몸통. 콜리전 프로필은 Pawn (캐릭터처럼 판정되도록) */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Dummy")
	TObjectPtr<UCapsuleComponent> Capsule;

	/** 겉모습. 메시는 BP나 레벨에서 지정. 판정은 Capsule이 하므로 콜리전 없음 */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Dummy")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 몸 막이. 플레이어가 허수아비를 통과하지 않게 (보스와 같은 방식) */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Dummy")
	TObjectPtr<ULOABodyBlockerComponent> BodyBlocker;

	UPROPERTY(VisibleAnywhere, Category = "LOA|AbilitySystem")
	TObjectPtr<ULOAAbilitySystemComponent> ASC;

	/** 체력 칸. 플레이어와 같은 AttributeSet을 쓴다. (ASC가 초기화될 때 자동 등록) */
	UPROPERTY()
	TObjectPtr<ULOAAttributeSet_Common> CommonAttributeSet;

	/**
	 * 전투 스탯창 (2026-11-13). 허수아비는 공격하지 않지만, Combat에는 맞는 쪽 칸(치명타 저항, 낙인 받는 피해 증가)도 있다.
	 * 데미지 계산이 맞는 쪽의 낙인 칸을 캡처하는데, 맞는 쪽에 없는 스탯창 칸을 캡처하면 엔진이 멈춘다 → 허수아비도 가진다.
	 * 초기값 없음 (전부 0). 낙인·약점 노출 테스트도 허수아비로 할 수 있다
	 */
	UPROPERTY()
	TObjectPtr<ULOAAttributeSet_Combat> CombatAttributeSet;

	// ---------- 데이터 (BP 기본값 또는 레벨 배치 인스턴스에서 지정) ----------

	/** 초기화 GE 등을 담은 몬스터 꾸러미 목록. (키로 발동하는 스킬이 없으므로 몬스터 꾸러미) [서버] BeginPlay에서 부여 */
	UPROPERTY(EditAnywhere, Category = "LOA|Dummy")
	TArray<TObjectPtr<const ULOAMonsterAbilityDataSet>> AbilityDataSets;

	/** 마지막 피격 후 몇 초 뒤 체력을 최대치로 되돌릴지. 0 이하면 되돌리지 않음 */
	UPROPERTY(EditAnywhere, Category = "LOA|Dummy")
	float ResetDelay = 3.f;

	// ---------- 런타임 ----------
	FTimerHandle ResetTimerHandle;
	FDelegateHandle HealthChangedHandle;
};
