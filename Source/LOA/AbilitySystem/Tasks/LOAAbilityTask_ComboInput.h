// LOAAbilityTask_ComboInput.h
// [배관] 콤보 입력 Task. 콤보 GA(ULOAGA_Combo)가 발동하는 동안 내내 켜 두고 "키를 다시 눌렀나 / 누르고 있나"만 알려 준다.
//   - 실행 중에 키를 다시 누름 → OnPressed
//   - 지금 누르고 있는가       → IsInputHeld()
// 조종하는 사람 컴퓨터에서만 쓴다. (키 입력은 그 컴퓨터에만 있다)
// 언제 다음 타로 넘어갈지는 GA가 정한다. 서버에 알리는 일도 이 Task가 하지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [키 누름/뗌을 어떻게 아나]
// 콤보 GA가 실행 중일 때 같은 키를 누르면, LOA ASC는 새로 발동하지 않고
// InvokeReplicatedEvent(InputPressed / InputReleased)로 "이 스킬, 이번 발동" 보관함 칸에 알린다.
// 이 Task는 그 칸을 구독한다. (WaitChargeRelease와 같은 방식. 이 신호는 서버로 가지 않는다)
//
// [수명]
// 주인 Ability 소속. 콤보가 끝나거나 끊기면 엔진이 정리하고, 구독은 OnDestroy에서 해제한다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "LOAAbilityTask_ComboInput.generated.h"

/** 실행 중에 키를 다시 누름 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLOAComboPressedDelegate);

UCLASS()
class LOA_API ULOAAbilityTask_ComboInput : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 실행 중에 키를 다시 누를 때마다 */
	UPROPERTY(BlueprintAssignable)
	FLOAComboPressedDelegate OnPressed;

	/** @param OwningAbility  주인 Ability (콤보 GA). 발동이 끝날 때까지 켜 둔다. 조종하는 사람 컴퓨터에서만 만든다. */
	static ULOAAbilityTask_ComboInput* ListenComboInput(UGameplayAbility* OwningAbility);

	virtual void Activate() override;

	/**
	 * 지금 키를 누르고 있는가.
	 * 발동 키를 누른 상태로 시작하므로 처음엔 true. 떼면 false, 다시 누르면 true.
	 */
	bool IsInputHeld() const { return bInputHeld; }

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	/** 키 누름 신호 (LOA ASC → 보관함 칸) */
	void OnInputPressed();

	/** 키 뗌 신호 (LOA ASC → 보관함 칸) */
	void OnInputReleased();

	/** 지금 키를 누르고 있는가 */
	bool bInputHeld = true;

	FDelegateHandle InputPressedHandle;
	FDelegateHandle InputReleasedHandle;
};
