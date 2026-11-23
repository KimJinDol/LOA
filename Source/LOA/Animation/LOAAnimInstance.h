// LOAAnimInstance.h
// [배관] LOA AnimBP의 공통 부모. GAS 태그를 AnimBP 변수로 자동 연결해 준다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가]
// 시체 포즈, (나중에) 그로기·기절 포즈처럼 "상태가 붙어 있는 동안 유지되는 자세"는 AnimBP 상태 머신이 맡는다.
// AnimBP가 그 상태를 알려면 ASC 태그를 봐야 한다. 매 프레임 직접 묻는 대신 엔진 기능으로 자동 연결한다.
//
//   GameplayTagPropertyMap (Class Defaults에서 설정)
//     State.Body.Dead                    →  bIsDead (AnimBP 변수)
//     State.CrowdControl.Status.Stunned  →  bIsStunned (플레이어 스턴 자세, 2026-11-10. 스턴 GA는 몽타주 없이)
//   태그가 붙으면 bIsDead = true, 떨어지면 false. 코드는 태그 이름도 변수 이름도 모른다. (데이터)
//
// 엔진의 FGameplayTagBlueprintPropertyMap을 쓴다. (Lyra의 AnimInstance와 같은 방식)
// 연결할 수 있는 변수 형식: bool (태그 있음/없음), int (태그 개수), float (태그 개수)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//   몸(보스 / 나중에 플레이어)이 자기 ASC가 준비된 직후 InitializeWithAbilitySystem(ASC)를 부른다.
//     보스      : BeginPlay에서 InitAbilityActorInfo 바로 다음
//     플레이어  : TryFinalizeInitialization (외형 적용 뒤)
//   → 이때 지금 태그 상태로 변수를 한 번 채우고, 이후 태그가 바뀔 때마다 갱신한다.
//   → 늦게 들어온 플레이어도 이미 붙은 State.Dead를 보고 바로 시체 포즈가 된다.
//
// AnimInstance가 스스로 ASC를 찾지 않는 이유: 플레이어는 ASC가 PlayerState에 있어서
// AnimBP가 초기화되는 시점에 아직 없을 수 있다. "ASC가 준비됐다"는 걸 아는 쪽(몸)이 부른다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// 컴퓨터마다 각자 연결한다. 태그는 ASC 복제로 모든 컴퓨터에 오므로 따로 복제할 것이 없다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] ABP_<이름> (부모: LOAAnimInstance)
//   ① AnimBP 변수 추가: bIsDead (bool)
//   ② Class Defaults → LOA|Anim → Gameplay Tag Property Map: [State.Body.Dead → bIsDead]
//   ③ 상태 머신: 아무 상태 → Dead (조건 bIsDead). Dead 상태 = 사망 애니 (Loop 끔 → 마지막 프레임 유지)

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "LOAAnimInstance.generated.h"

class UAbilitySystemComponent;

UCLASS()
class LOA_API ULOAAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/**
	 * 태그 → 변수 연결을 시작한다. 몸이 ASC 준비 직후 부른다. [모든 컴퓨터]
	 * 다시 부르면 이전 연결을 지우고 새 ASC로 다시 연결한다.
	 */
	void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

#if WITH_EDITOR
	/** [에디터 전용] 연결 칸 검사: 없는 변수 이름, 맞지 않는 변수 형식 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/**
	 * 태그 → AnimBP 변수 연결 목록.
	 * 예) State.Body.Dead → bIsDead
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Anim")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;
};
