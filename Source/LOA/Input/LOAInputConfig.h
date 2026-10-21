// LOAInputConfig.h
// [배관] "입력 액션(InputAction) ↔ 입력 태그(InputTag)" 연결표 데이터 에셋.
//
// ─────────────────────────────────────────────────────────────────────────────
// [이 파일이 왜 필요한가]
//
// 키 입력이 스킬까지 가는 전체 경로는 3단계로 나뉘며, 단계마다 담당이 다르다.
//
//   ① 키보드 Q  ──(IMC: 입력 매핑 컨텍스트)──▶  IA_Skill_Q (입력 액션)
//   ② IA_Skill_Q ──(이 파일: InputConfig)──▶  InputTag.Skill.Q (입력 태그)
//   ③ InputTag.Skill.Q ──(AbilitySet: 스킬 묶음)──▶  휠윈드 (스킬)
//
//   ① "어떤 물리 키가 어떤 입력 액션인가"  → IMC 에셋 (Enhanced Input 기본 기능, 키 변경 옵션도 여기서 처리)
//   ② "어떤 입력 액션이 어떤 슬롯 태그인가" → 이 파일
//   ③ "어떤 슬롯 태그에 어떤 스킬이 있나"  → 직업별 AbilitySet
//
// 단계를 나눈 이유:
//   - 키 변경(Q → T)은 ①만 바뀐다.            (코드, 스킬 데이터 수정 없음)
//   - 스킬 배치 변경(휠윈드를 R로)은 ③만 바뀐다. (코드, 입력 설정 수정 없음)
//   - PlayerController 코드는 "태그"만 알고, 구체적인 키나 스킬은 모른다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [직업마다 따로 만드나?]
//
// 아니다. 로아는 직업이 달라도 키 배치(QWER/ASDF, 스페이스, Z/X)가 같으므로
// 이 에셋은 프로젝트 전체에 "하나"만 만들고 PlayerController가 들고 있는다.
// 직업 차이는 ③(AbilitySet)에서만 생긴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [두 목록으로 나눈 이유]
//
//   NativeInputActions  : 코드가 "직접" 처리하는 입력. (예: InputTag.Move)
//                         → 조작 방식 부품(WASD 등)이 태그로 입력 액션을 찾아서 직접 바인딩한다.
//                         → 클릭 이동 부품으로 바뀌면 이 목록에 "InputTag.ClickMove" 같은 항목만 추가하면 된다.
//   AbilityInputActions : 스킬 발동용 입력. (예: InputTag.Skill.Q)
//                         → PlayerController가 전부 한꺼번에 바인딩하고,
//                           눌림/뗌이 오면 태그를 ASC로 그대로 넘긴다. (어떤 스킬인지는 ASC가 찾음)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// 이 에셋은 "내 컴퓨터의 입력 처리"에만 쓰인다. 서버는 키보드가 없으므로 사용하지 않으며, 복제 대상도 아니다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "LOAInputConfig.generated.h"

class UInputAction;

/**
 * 연결표의 한 줄: "이 입력 액션은 이 태그다"
 */
USTRUCT(BlueprintType)
struct FLOAInputAction
{
	GENERATED_BODY()

	/** Enhanced Input의 입력 액션 에셋 (예: IA_Skill_Q) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/**
	 * 이 입력 액션에 붙일 태그 (예: InputTag.Skill.Q)
	 * meta = (Categories = "InputTag") : 에디터 태그 선택창에 InputTag 하위 태그만 보이게 해서 실수를 줄인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * 입력 액션 ↔ 입력 태그 연결표.
 * 에디터에서 우클릭 → 기타 → 데이터 에셋 → LOAInputConfig 로 생성한다. (예: DA_InputConfig_Default)
 *
 * UPrimaryDataAsset이 아니라 UDataAsset인 이유:
 * 이 에셋은 PlayerController가 직접 참조해서 함께 로드된다. 에셋 매니저로 따로 찾거나 비동기 로드할 필요가 없다.
 */
UCLASS(BlueprintType, Const)
class LOA_API ULOAInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 태그로 "코드가 직접 처리하는 입력 액션"을 찾는다.
	 * 조작 방식 부품이 바인딩할 때 사용한다. (예: WASD 부품이 InputTag.Move로 IA_Move를 찾음)
	 *
	 * @param InputTag       찾을 태그
	 * @param bLogNotFound   못 찾았을 때 경고 로그를 남길지 (데이터 설정 실수를 빨리 발견하기 위함)
	 * @return               찾은 입력 액션. 없으면 nullptr
	 */
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** 스킬용 연결표 전체. PlayerController가 한꺼번에 바인딩할 때 사용한다. */
	const TArray<FLOAInputAction>& GetAbilityInputActions() const { return AbilityInputActions; }

#if WITH_EDITOR
	/**
	 * 에디터에서 에셋을 저장하거나 "데이터 검증"을 실행할 때 호출된다.
	 * 빈 칸, 태그 중복 같은 설정 실수를 게임 실행 전에 잡아낸다.
	 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** 코드가 직접 처리하는 입력 (이동 등) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Input", meta = (TitleProperty = "InputTag"))
	TArray<FLOAInputAction> NativeInputActions;

	/** 스킬 발동 입력 (QWER, ASDF, 스페이스, Z/X 등) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Input", meta = (TitleProperty = "InputTag"))
	TArray<FLOAInputAction> AbilityInputActions;
};
