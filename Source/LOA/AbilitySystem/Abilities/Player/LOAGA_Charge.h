// LOAGA_Charge.h
// [배관] 차지 GA. "누르고 있으면 모으고, 떼면 모은 만큼 세게 발사한다."
// (홀딩 - 누르는 동안 공격이 계속 나가는 스킬 - 은 이 클래스가 아니다. 나중에 LOAGA_Holding으로 따로 만든다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [디테일 패널]
//
//   ▼ LOA | Ability            ← 부모와 같음 (Montage, 쿨타임, 마나, bFaceCursor …)
//   ▼ LOA | Ability | Charge
//        Release Section    [ Release ]       ← 뗄 때 넘어갈 몽타주 섹션
//        Loop Section       [ None ]          ← 차지 중 반복할 섹션. 비우면 차지 구간 끝 자세로 멈춰서 기다림
//        Max Charge Time    [ 2.0 ]           ← 넘으면 자동 발사 (마지막 단계 시간보다 길어도 됨)
//        Base Multiplier    [ 1.0 ]           ← [단계식] 아무 단계도 못 채웠을 때
//        Min Charge Ratio   [ 0.3 ]           ← [비율식] 이 비율 아래로는 내려가지 않음
//        Stages
//           [0] Time 0.8  Multiplier 1.5
//           [1] Time 1.6  Multiplier 2.2
//        bUsePerfectZone    ☐                 ← 켜면 아래 셋이 보임
//           Perfect Zone Start / End, Perfect Multiplier
//
// ─────────────────────────────────────────────────────────────────────────────
// [차지 배율 규칙] (CalculateChargeMultiplier)
//
//   단계가 2개 이상 → 단계식. 그 단계 시간을 끝까지 채워야 그 배율.
//        0 ~ 0.79초 → Base(1.0) / 0.8 ~ 1.59초 → 1.5 / 1.6초 ~ → 2.2
//
//   단계가 1개 → 비율식. 단계 배율 × 채운 비율 (비율은 Min Charge Ratio 아래로 안 내려감)
//        Stages[0] = Time 1.5 / Multiplier 2.0, Min Charge Ratio 0.3 일 때
//        0.2초(13%) → 2.0 × 0.3 = 0.6 / 0.75초(50%) → 1.0 / 1.5초 이상 → 2.0
//
//   퍼펙트 존 (옵션, 두 방식 모두) : Start ~ End 사이에 떼면 단계와 상관없이 Perfect Multiplier
//
// ─────────────────────────────────────────────────────────────────────────────
// [몽타주] 두 가지 중 하나. (어느 쪽이든 Release 섹션에 타격 노티파이 Event.Montage.Hit, 후딜 노티파이 Event.Montage.Unlock)
//
//   ① 루프 있음 (Loop Section = Charge_Loop)
//      [Charge_Start] → [Charge_Loop ↺]      [Release]
//      - Charge_Start의 다음 섹션 = Charge_Loop, Charge_Loop의 다음 섹션 = 자기 자신 (몽타주 섹션 설정)
//      - Charge_Start 없이 Charge_Loop가 첫 섹션이어도 된다.
//
//   ② 루프 없음 (Loop Section = None) - 루프 애니메이션이 없을 때. 아무 공격 애니메이션 하나로도 만들 수 있다.
//      [Charge_Start ■]      [Release]
//      - 첫 섹션이 끝나기 직전에 몽타주를 멈추고(재생 속도 0), 그 자세로 뗄 때까지 기다린다.
//      - 첫 섹션의 다음 섹션은 비워 둔다. (Release로 이어 두면 안 뗐는데 발사됨 → 저장 시 오류)
//      - 블렌드 아웃이 시작되기 전에 멈춰야 하므로 실제로는 섹션 끝보다 (블렌드 아웃 시간 + 약간) 앞에서 멈춘다.
//
//   어느 쪽이든 차지 중에는 Release가 저절로 재생되면 안 된다. (저장 시 검사가 막는다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   키 누름 → (부모) Commit(쿨타임/마나) → 커서 쪽으로 한 번 돎 → BeginSkill(모듈 켜기)
//   StartSkill (여기)
//      ├ 몽타주 재생 (① Charge_Start → Charge_Loop 반복 / ② 첫 섹션 끝에서 멈춤 - WaitDelay Task)
//      └ WaitChargeRelease Task (누른 시간 측정)
//   키 뗌 / Max Charge Time → OnChargeReleased(누른 시간)
//      ├ 배율 계산 → ChargeMultiplier 저장 (모듈의 데미지에 곱해짐)
//      ├ 이벤트 Event.Skill.Charge.Released (+ 퍼펙트면 Event.Skill.Charge.Perfect)
//      └ (② 멈춰 있으면 재생 속도 복구) → 몽타주 Release 섹션으로 점프 (조종하는 사람 컴퓨터만. 아래 [네트워크])
//   Release 섹션의 Hit 노티파이 → Apply 모듈 → 공격력 × DamageRate × ChargeMultiplier
//
//   - 차지 중에는 돌지 않는다. (방향은 누르는 순간에만 정해짐)
//   - 회피로 끊으면 Task와 몽타주는 Ability와 함께 정리된다. 쿨타임은 이미 돌고 있다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   누른 시간은 클라이언트가 재서 서버로 보내고, 서버는 범위만 제한한다. (WaitChargeRelease 참고)
//   배율은 양쪽이 같은 누른 시간으로 각자 계산한다. 데미지는 서버만 주므로 서버 값이 최종.
//
//   [Release 점프는 조종하는 사람 컴퓨터만]
//   클라이언트가 MontageJumpToSection을 부르면 엔진(ASC)이 서버로 점프 RPC를 자동으로 보낸다.
//   서버도 스스로 점프하면 Release가 두 번 시작된다. 보통은 같은 프레임이라 티가 안 나지만,
//   렉으로 두 RPC가 다른 프레임에 처리되면 Release가 처음으로 되돌아가 Hit가 두 번 나갈 수 있다.
//   → 서버(원격 플레이어)는 점프하지 않는다. 순서: 누른 시간 TargetData(배율 확정) → 점프 RPC → Release의 Hit
//   예외: 서버 타임아웃(클라이언트 값이 끝내 안 옴). 클라이언트의 점프 RPC도 오지 않을 수 있으므로 서버가 직접 점프한다.
//   리슨 호스트 / 스탠드얼론은 자기가 조종하는 쪽이라 직접 점프한다. (보낼 RPC 없음)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "LOAGA_Charge.generated.h"

/** 차지 단계 하나: 이 시간을 채우면 이 배율 */
USTRUCT(BlueprintType)
struct FLOAChargeStage
{
	GENERATED_BODY()

	/** 이 단계가 되는 누른 시간 (초). 단계들은 시간 순서대로 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Charge", meta = (ClampMin = "0.01", Units = "Seconds"))
	float Time = 1.f;

	/** 이 단계의 차지 배율 (데미지 GE의 공격력 × DamageRate에 곱해진다) */
	UPROPERTY(EditDefaultsOnly, Category = "Charge", meta = (ClampMin = "0.0"))
	float Multiplier = 1.f;
};

UCLASS(Abstract)
class LOA_API ULOAGA_Charge : public ULOAPlayerAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Charge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin ULOAGameplayAbility Interface
	/** 이번 발동의 차지 배율. 떼기 전에는 1, 뗀 뒤에는 계산된 배율 */
	virtual float GetChargeMultiplier() const override { return ChargeMultiplier; }

#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사: 단계 순서, 최대 시간, 퍼펙트 존 범위, 몽타주 섹션 구조 (Release / Loop / 멈춤) */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End ULOAGameplayAbility Interface

	/**
	 * 누른 시간 → 차지 배율. (규칙은 헤더 상단 [차지 배율 규칙])
	 * @param bOutPerfect  퍼펙트 존 안이었으면 true
	 */
	float CalculateChargeMultiplier(float HeldTime, bool& bOutPerfect) const;

protected:
	//~ Begin ULOAPlayerAbility Interface
	/** 몽타주 재생 + 차지 대기 Task 시작 */
	virtual void StartSkill() override;
	//~ End ULOAPlayerAbility Interface

	// ---------- 데이터 (각 차지 스킬 BP에서) ----------

	/** 뗄 때 넘어갈 몽타주 섹션 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge")
	FName ReleaseSectionName = TEXT("Release");

	/**
	 * 차지 중 반복할 몽타주 섹션 이름. (다음 섹션이 자기 자신이어야 한다)
	 * 비우면(None) 루프 없이 첫 섹션 끝 자세로 멈춰서 기다린다. (헤더 상단 [몽타주] ②)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge")
	FName LoopSectionName = NAME_None;

	/** 이 시간이 되면 키를 안 떼도 자동 발사 (초). 마지막 단계 시간 이상이어야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge", meta = (ClampMin = "0.01", Units = "Seconds"))
	float MaxChargeTime = 2.f;

	/** [단계식 - 단계 2개 이상] 아무 단계도 못 채우고 뗐을 때의 배율 (가장 낮은 배율) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge", meta = (ClampMin = "0.0"))
	float BaseMultiplier = 1.f;

	/** [비율식 - 단계 1개] 채운 비율의 최솟값. 이보다 덜 채워도 이 비율로 계산 (최소 데미지 보장) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinChargeRatio = 0.3f;

	/** 차지 단계. 2개 이상이면 단계식, 1개면 비율식. 시간 순서대로 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge", meta = (TitleProperty = "Time"))
	TArray<FLOAChargeStage> Stages;

	/** 퍼펙트 존 사용 여부 (단계식/비율식 모두 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge")
	bool bUsePerfectZone = false;

	/** 퍼펙트 존 시작 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge",
		meta = (EditCondition = "bUsePerfectZone", EditConditionHides, ClampMin = "0.0", Units = "Seconds"))
	float PerfectZoneStart = 1.6f;

	/** 퍼펙트 존 끝 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge",
		meta = (EditCondition = "bUsePerfectZone", EditConditionHides, ClampMin = "0.0", Units = "Seconds"))
	float PerfectZoneEnd = 1.8f;

	/** 퍼펙트 존 안에서 뗐을 때의 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Charge",
		meta = (EditCondition = "bUsePerfectZone", EditConditionHides, ClampMin = "0.0"))
	float PerfectMultiplier = 2.6f;

private:
	/**
	 * 뗌 / 최대 시간 도착 (조종하는 사람 컴퓨터와 서버 각각)
	 * @param bServerTimedOut  서버가 클라이언트 값 없이 스스로 끝냄 → 이때만 서버도 Release로 점프
	 */
	UFUNCTION()
	void OnChargeReleased(float HeldTime, bool bServerTimedOut);

	// ---------- 루프 없는 몽타주 (② 멈춤) ----------

	/**
	 * 몽타주를 처음부터 재생할 때 몇 초 뒤에 멈춰야 하는지 (실제 시간, 재생 속도 반영).
	 * 첫 섹션 끝에서 (블렌드 아웃 시작 시간 + 여유)만큼 앞. 블렌드 아웃이 시작되면 몽타주가 끝나며 Ability도 끝나기 때문.
	 */
	float GetChargeFreezeDelay() const;

	/** 멈출 시간이 됨 → 재생 속도 0 */
	UFUNCTION()
	void OnChargeFreeze();

	/** 멈추기를 기다리는 Task. 그 전에 떼면 끝낸다. (주인 = 이 Ability → Ability가 끝나면 엔진이 정리) */
	UPROPERTY()
	TObjectPtr<class UAbilityTask_WaitDelay> FreezeTask;

	/** 지금 몽타주를 멈춰 둔 상태인지. 떼면 재생 속도를 되돌린다. */
	bool bMontageFrozen = false;

	/** 이번 발동의 차지 배율. StartSkill에서 1로 초기화, OnChargeReleased에서 확정 */
	float ChargeMultiplier = 1.f;
};
