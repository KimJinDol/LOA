// LOAAbilityTask_WaitChargeRelease.h
// [배관] 차지 대기 Task. "키를 뗄 때(또는 최대 시간이 될 때)까지 기다렸다가, 누른 시간을 알려 준다."
// 차지 GA(ULOAGA_Charge)가 쓴다. 나중에 콤보 + 차지 같은 혼합 흐름에서도 이 Task를 그대로 쓴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   [조종하는 사람 컴퓨터] (리슨 호스트, 스탠드얼론 포함)
//      시작 시각 기록 → 키 뗌 대기 + 최대 시간 타이머
//      뗌 / 최대 시간 → 누른 시간 계산
//         → 원격 클라이언트면: 누른 시간을 서버로 보냄 (TargetData)
//         → OnReleased(누른 시간, false) → 끝  (서버 답을 기다리지 않고 바로 진행)
//
//   [서버] (원격 플레이어의 Ability)
//      시작 시각 기록 → 클라이언트가 보낸 누른 시간 대기
//      도착 → 서버가 잰 시간 기준으로 범위 제한 → OnReleased(누른 시간, false) → 끝
//      안 옴 → (최대 시간 + 여유 시간) 뒤 서버가 잰 시간으로 OnReleased(누른 시간, true) → 끝   (연결 문제로 영원히 멈추지 않게)
//              bServerTimedOut = true : 클라이언트가 발사하지 않았다는 뜻. GA는 이때만 서버가 직접 몽타주를 점프한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 서버가 직접 재지 않고 클라이언트 값을 받나]
// 서버는 발동 요청도, 키 뗌도 네트워크 지연만큼 늦게 받는다. 각자 재면 단계 경계(예: 0.8초)에서
// 내 화면은 1단, 서버는 0단처럼 갈릴 수 있다. → 클라이언트가 잰 값을 기준으로 맞추고,
// 서버는 "자기가 잰 시간 + 허용치"를 넘지 않게만 막는다. (코옵이라 이 정도 신뢰로 충분)
//
// [키 뗌 감지]
// LOA ASC가 키를 뗄 때 InvokeReplicatedEvent(InputReleased)를 부른다. (LOAAbilitySystemComponent.cpp)
// 이 Task는 그 이벤트를 듣는다. Task가 시작되기 전에 이미 뗐어도(아주 짧게 탭) 기록이 남아 있어서 바로 처리된다.
//
// [수명]
// Task는 주인 Ability 소속. Ability가 끝나거나 취소되면(회피로 끊기 등) 엔진이 정리하고, 타이머와 대기도 OnDestroy에서 해제한다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "LOAAbilityTask_WaitChargeRelease.generated.h"

/**
 * 누른 시간(초) 하나를 서버로 보내는 TargetData.
 * GAS의 TargetData 통로(ServerSetReplicatedTargetData)에 실어 보내려면 FGameplayAbilityTargetData를 상속해야 한다.
 */
USTRUCT()
struct LOA_API FLOATargetData_ChargeTime : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** 누른 시간 (초) */
	UPROPERTY()
	float HeldTime = 0.f;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	/** 네트워크로 보낼 때 이 칸만 쓴다. */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << HeldTime;
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FLOATargetData_ChargeTime> : public TStructOpsTypeTraitsBase2<FLOATargetData_ChargeTime>
{
	enum
	{
		WithNetSerializer = true,	// NetSerialize로 보낸다
	};
};

/**
 * @param HeldTime         누른 시간 (초)
 * @param bServerTimedOut  [서버] 클라이언트 값이 끝내 안 와서 서버가 스스로 끝냈다. (클라이언트도 발사하지 않았을 수 있음)
 *                         조종하는 사람 컴퓨터와, 정상적으로 값을 받은 서버는 false
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLOAChargeReleasedDelegate, float, HeldTime, bool, bServerTimedOut);

UCLASS()
class LOA_API ULOAAbilityTask_WaitChargeRelease : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 뗌 / 최대 시간 → 누른 시간과 함께 한 번 호출 (조종하는 사람 컴퓨터와 서버 각각). 서버 타임아웃이면 bServerTimedOut = true */
	UPROPERTY(BlueprintAssignable)
	FLOAChargeReleasedDelegate OnReleased;

	/**
	 * @param OwningAbility   주인 Ability
	 * @param MaxChargeTime   이 시간이 되면 키를 안 떼도 자동으로 끝난다 (초)
	 */
	static ULOAAbilityTask_WaitChargeRelease* WaitChargeRelease(UGameplayAbility* OwningAbility, float MaxChargeTime);

	virtual void Activate() override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	// ---------- 조종하는 사람 컴퓨터 ----------
	/** 키 뗌 이벤트 도착 */
	void OnInputReleased();

	/** 뗌 또는 최대 시간 → 누른 시간 확정 → (원격이면) 서버로 보냄 → 알림 */
	void FinishLocal();

	// ---------- 서버 ----------
	/** 클라이언트가 보낸 누른 시간 도착 */
	void OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag);

	/** 클라이언트 값이 끝내 안 옴 → 서버가 잰 시간으로 끝낸다 */
	void OnServerTimeout();

	// ---------- 공용 ----------
	/** 지금까지 누른 시간 (Activate 시각 기준) */
	float GetElapsed() const;

	/** 한 번만 알리고 Task 끝 */
	void BroadcastAndEnd(float HeldTime, bool bServerTimedOut);

	/** 최대 시간 (초) */
	float MaxChargeTime = 0.f;

	/** Activate 시각 (월드 시간) */
	double StartTime = 0.0;

	/** 이미 끝냈는지 (뗌과 최대 시간이 같은 프레임에 겹칠 때 두 번 알리지 않게) */
	bool bFinished = false;

	FTimerHandle TimerHandle;
	FDelegateHandle InputReleasedHandle;
	FDelegateHandle TargetDataHandle;
};
