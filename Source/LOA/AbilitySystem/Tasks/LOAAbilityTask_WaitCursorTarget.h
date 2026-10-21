// LOAAbilityTask_WaitCursorTarget.h
// [배관] "커서 아래 위치"를 조종하는 사람 컴퓨터에서 구해서 서버로 보내 주는 AbilityTask.
// 커서 방향으로 나가는 모든 Ability(평타, 대부분의 스킬)가 재사용한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가]
// 마우스 커서는 조종하는 사람 컴퓨터에만 있다. 서버는 커서가 어디 있는지 모른다.
// 서버가 모르면 → 서버에서 캐릭터를 돌릴 수 없고 → 다른 플레이어 화면에서는 엉뚱한 방향으로 공격한다.
// 그래서 "내 컴퓨터가 커서 위치를 구해서 서버에 알려 준다".
//
// GAS의 TargetData 기능을 쓴다. (직접 RPC를 만들지 않음)
//   - Ability 발동과 같은 예측 키로 묶여서, 서버가 "어느 발동의 커서 위치인지" 헷갈리지 않는다.
//   - 데이터가 발동 요청보다 먼저/나중에 도착해도 GAS가 순서를 맞춰 준다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   [조종하는 사람 컴퓨터]                        [서버]
//   Ability 발동 (예측)                           Ability 발동 (확인)
//      │                                             │
//   Activate()                                    Activate()
//      ├─ 커서 아래 트레이스                        ├─ "이 발동의 커서 데이터 도착하면 알려 줘" 등록
//      ├─ ServerSetReplicatedTargetData ──────────▶ │   (이미 도착해 있으면 바로 처리)
//      └─ ValidData 방송 (바로 진행)                 └─ 도착 → OnTargetDataReplicated → ValidData 방송
//
//   양쪽 모두 ValidData에서 같은 커서 위치를 받는다. → 같은 방향으로 회전
//
// ─────────────────────────────────────────────────────────────────────────────
// [커서 위치 구하는 규칙]
// ULOACursorLibrary::GetCursorHitResult를 쓴다. (클릭 이동과 같은 규칙)
//   ① 커서 아래 트레이스 → ② 실패하면 캐릭터 발밑 높이의 가상 바닥 평면
//   → 로아처럼 맵 끝 바깥을 가리켜도 그쪽을 바라본다.
//   ②로 얻은 결과는 HitResult.bBlockingHit = false. 장판처럼 "실제 바닥"이 필요한 스킬만 확인하면 된다.
//
// [결과]
//   ValidData : 커서 위치를 얻음. DataHandle 안에 HitResult가 들어 있다. (① 또는 ②)
//   Failed    : ①②가 모두 실패. (카메라가 하늘을 향함, PlayerController 없음 등 거의 없는 경우)
//               이때 어떻게 할지는 Ability가 정한다. (지금은 모두 "지금 보는 방향")
//
// [리슨 서버 호스트]
//   호스트는 "조종하는 사람 컴퓨터"이면서 서버다. 로컬 쪽 흐름만 타고, 서버 RPC는 자기 자신에게 바로 실행된다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"
#include "LOAAbilityTask_WaitCursorTarget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOAWaitCursorTargetDelegate, const FGameplayAbilityTargetDataHandle&, DataHandle);

UCLASS()
class LOA_API ULOAAbilityTask_WaitCursorTarget : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 커서 위치를 얻었다. (조종하는 사람 컴퓨터: 즉시 / 서버: 데이터 도착 시) */
	UPROPERTY(BlueprintAssignable)
	FLOAWaitCursorTargetDelegate ValidData;

	/** 커서 아래에 아무것도 없다. */
	UPROPERTY(BlueprintAssignable)
	FLOAWaitCursorTargetDelegate Failed;

	/**
	 * 커서 아래 위치를 구해서 서버와 공유한다.
	 * @param TraceChannel  커서 트레이스 채널. Ability 쪽 설정값을 넘긴다. (클릭 이동과 같은 채널 권장)
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Ability|Tasks",
		meta = (DisplayName = "Wait Cursor Target", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static ULOAAbilityTask_WaitCursorTarget* WaitCursorTarget(UGameplayAbility* OwningAbility, ECollisionChannel TraceChannel);

	/** HitResult가 담긴 DataHandle에서 위치만 꺼내는 도우미. 없으면 false */
	static bool GetLocationFromData(const FGameplayAbilityTargetDataHandle& DataHandle, FVector& OutLocation);

protected:
	//~ Begin UGameplayTask Interface
	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	//~ End UGameplayTask Interface

private:
	/** [조종하는 사람 컴퓨터] 커서 트레이스 → 서버로 전송 → 결과 방송 */
	void SendCursorData();

	/** [서버] 클라이언트가 보낸 커서 데이터 도착 */
	void OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag);

	/** 결과에 따라 ValidData / Failed 방송 후 Task 종료 */
	void BroadcastResult(const FGameplayAbilityTargetDataHandle& DataHandle);

	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 서버에서 등록한 "데이터 도착 알림" 해제용 */
	FDelegateHandle TargetDataDelegateHandle;
};