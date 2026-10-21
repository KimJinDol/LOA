// LOAAbilitySystemComponent.h
// 스킬 엔진. GAS 기본 ASC에 "입력 태그로 스킬 발동" 기능만 얹은 클래스.
//
// [이 클래스가 하는 일]
// - 캐릭터가 전달한 입력 태그(예: InputTag.Skill.Q)를 받아, 그 태그가 붙은 스킬을 찾아 발동한다.
// - 캐릭터는 어떤 스킬이 발동되는지 모른다. 이 클래스도 "어떤 직업인지"는 모른다.
//
// - 스킬 목록이 바뀌면 OnAbilitiesChanged로 알린다. (내 화면의 스킬 아이콘 슬롯이 듣는다)
//
// [스킬 목록 바뀜 알림 - OnAbilitiesChanged]
//   언제 : 서버에서 부여/회수할 때 (OnGiveAbility / OnRemoveAbility)
//          클라이언트에 스킬 목록이 복제로 도착하거나 바뀔 때 (OnRep_ActivateAbilities. 나중에 슬롯 이동 Q → W도 여기)
//   한 프레임에 한 번만 : 꾸러미 하나가 스킬 10개를 주면 알림이 10번이 아니라 다음 프레임에 1번.
//          그리고 회수는 목록에서 빠지기 "전에" 불리므로, 다음 프레임에 알려야 빠진 뒤의 목록이 보인다.
//   듣는 사람이 없으면(보스, 다른 플레이어의 ASC) 아무것도 하지 않는다.
//
// [이 클래스가 하지 않는 일]
// - 복제 모드 설정: 소유자가 정한다. (플레이어 = PlayerState에서 Mixed, 보스 = 보스 액터에서 Minimal)
// - 스킬 목록 결정: 데이터 에셋이 정한다.
//
// [네트워크]
// - 입력 함수들은 "조종하는 플레이어의 컴퓨터"에서 호출된다.
// - 스킬 발동 요청을 서버로 보내는 일은 GAS가 알아서 한다. (클라이언트 → 서버 RPC, 로컬 예측 포함)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "LOAAbilitySystemComponent.generated.h"

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOAAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	ULOAAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 입력 키가 눌렸을 때 캐릭터가 호출한다. [조종하는 플레이어의 컴퓨터]
	 * InputTag를 가진 스킬이 꺼져 있으면 발동을 시도하고, 이미 실행 중이면 "눌림" 신호를 전달한다.
	 * (예: 차지 스킬이 "키를 누르고 있는지" 알 수 있도록)
	 */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/**
	 * 입력 키를 뗐을 때 캐릭터가 호출한다. [조종하는 플레이어의 컴퓨터]
	 * 실행 중인 해당 스킬에 "뗌" 신호를 전달한다. (예: 차지 스킬 발사 시점)
	 */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** 스킬 목록이 바뀜 (부여, 회수, 복제 도착). 한 프레임에 한 번. 헤더 [스킬 목록 바뀜 알림] */
	FSimpleMulticastDelegate OnAbilitiesChanged;

protected:
	//~ Begin UAbilitySystemComponent Interface
	/**
	 * 실행 중인 스킬에 "눌림/뗌"을 알린다. [조종하는 플레이어의 컴퓨터에서만 호출됨]
	 *
	 * 하는 일: InvokeReplicatedEvent로 이 컴퓨터의 ASC 보관함 칸 (스킬 Handle + 발동 예측키)에
	 *          "눌림/뗌"을 기록하고, 그 칸을 구독한 Task(예: WaitChargeRelease)를 부른다.
	 *
	 * 하지 않는 일: 서버로 보내지 않는다. InvokeReplicatedEvent는 이름과 달리 이 컴퓨터 안에서만 동작한다.
	 *   서버가 알아야 하면 그 신호를 쓰는 Task가 직접 보낸다.
	 *   예) WaitChargeRelease: "뗌" 대신 누른 시간을 TargetData로 보냄 (ServerSetReplicatedTargetData)
	 *   예) 엔진 WaitInputRelease: ServerSetReplicatedEvent RPC를 직접 부름
	 *
	 * GAS 기본 구현(bReplicateInputDirectly)을 쓰지 않는 이유: 매 입력마다 RPC를 보내고, 엔진도 권장하지 않음.
	 */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	/** [서버] 스킬이 부여됨 → 스킬 목록 바뀜 알림 예약 */
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	/** [서버] 스킬이 회수됨 (아직 목록에 남아 있는 순간) → 스킬 목록 바뀜 알림 예약 */
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	/** [클라이언트] 스킬 목록이 복제로 도착하거나 바뀜 → 스킬 목록 바뀜 알림 예약 */
	virtual void OnRep_ActivateAbilities() override;
	//~ End UAbilitySystemComponent Interface

private:
	/** 다음 프레임에 OnAbilitiesChanged를 한 번 보낸다. 이미 예약했으면 아무것도 안 함 */
	void RequestAbilitiesChangedBroadcast();

	/** 다음 프레임 알림을 예약해 둔 상태인가 (한 프레임에 여러 번 바뀌어도 알림은 1번) */
	bool bAbilitiesChangedPending = false;
};
