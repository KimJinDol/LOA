// LOASkillFactory.h
// [에디터 도구] 스킬 만들기. 버튼 한 번으로 "스킬 폴더 + GA BP + 몽타주"를 이름 규칙대로 만든다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 있나]
// 스킬을 손으로 만들면 매번 같은 일을 반복하고, 실수가 생긴다.
//   - GA는 GA 폴더, 몽타주는 Resources 폴더 → 한 스킬의 에셋이 흩어진다.
//   - 이름을 손으로 짓는다 → 오타 (예: 실제로 있던 GA_Berkerser_PowerBrake)
//   - 몽타주를 만들고 GA의 Montage 칸에 연결하는 걸 깜빡한다.
// → 입력 3~4개를 받아 정해진 규칙대로 한 번에 만든다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙] 스킬 하나 = 폴더 하나
//
//   /Game/LOA/Skills/<클래스>/<스킬>/
//      GA_<클래스>_<스킬>   ← 고른 부모 GA 클래스로 만든 Gameplay Ability Blueprint
//      M_<클래스>_<스킬>    ← 애니메이션을 골랐을 때만. GA의 Montage 칸에 연결된다.
//      (나중에 이 스킬의 투사체 BP, GE, 이펙트도 이 폴더에 둔다)
//
//   <클래스> : 고른 클래스 데이터 에셋 이름에서 "DA_Class_"를 뗀 것 (DA_Class_Berserker → Berserker)
//              → 글자로 입력하지 않으므로 오타가 날 수 없다.
//   <스킬>   : 입력한 이름. 영문자와 숫자만, 첫 글자는 영문자. (예: Whirlwind, PowerBrake)
//              밑줄(_)을 막는 이유: GA_<클래스>_<스킬>이 항상 세 조각으로 나뉘도록.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] CreateSkill
//
//   ① 입력 검사          클래스 데이터 / 스킬 이름 / 부모 GA 클래스
//   ② 이름 정하기        폴더, GA 이름, 몽타주 이름
//   ③ 이미 있나          GA나 몽타주가 하나라도 있으면 아무것도 만들지 않고 실패 (덮어쓰기 방지)
//   ④ GA BP 생성         UGameplayAbilityBlueprint (콘텐츠 브라우저의 "Gameplay Ability Blueprint"와 같은 종류)
//   ⑤ 몽타주 생성        애니메이션을 골랐을 때만. 스켈레톤은 애니메이션의 것
//   ⑥ 연결               GA BP의 기본값(CDO) Montage = 새 몽타주
//   ⑦ 저장 → 콘텐츠 브라우저를 새 폴더로 이동 → GA BP 편집 창 열기
//   결과는 에디터 오른쪽 아래 알림 + OutMessage(EUW에 표시용)로 알린다.
//
// [만들지 않는 것] (직접 한다)
//   - 몽타주의 노티파이(Hit, Unlock, ComboOpen …)와 섹션(콤보 단계, 차지 Release)
//     → GA를 저장하면 저장 시 검사가 "노티파이 없음" 등을 알려 준다.
//   - 모듈(Apply, Spawn …) 세팅, AbilityDataSet 등록 (입력 태그가 필요해서 나중에)
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 법]
// 에디터 유틸리티 위젯(EUW_SkillFactory)의 버튼이 CreateSkill을 부른다.
// 부모 GA 목록(콤보 박스)은 GetSkillParentClassNames로 채운다.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LOASkillFactory.generated.h"

class ULOACharacterClassData;
class UAnimSequence;

UCLASS()
class LOAEDITOR_API ULOASkillFactory : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 스킬 폴더 + GA BP (+ 몽타주)를 만든다. 위 [흐름] 참고.
	 *
	 * @param ClassData        어느 클래스의 스킬인가 (DA_Class_*). 이름에서 클래스 이름을 꺼낸다.
	 * @param SkillName        스킬 이름. 영문자와 숫자만, 첫 글자는 영문자 (예: Whirlwind)
	 * @param ParentClassName  부모 GA 클래스 이름. GetSkillParentClassNames가 돌려준 값 중 하나 (예: LOAGA_Charge)
	 * @param Animation        몽타주를 만들 애니메이션. 비워 두면 몽타주를 만들지 않는다. (나중에 직접 지정)
	 * @param OutMessage       결과 설명 (성공: 만든 에셋 경로 / 실패: 이유). EUW에 그대로 보여 주면 된다.
	 * @return 만들었으면 true. 하나도 만들지 않았으면(입력 오류, 이미 있음) false
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Skill Factory")
	static bool CreateSkill(const ULOACharacterClassData* ClassData, const FString& SkillName,
		const FString& ParentClassName, UAnimSequence* Animation, FString& OutMessage);

	/**
	 * 고를 수 있는 부모 GA 클래스 이름들. EUW의 콤보 박스에 넣는다.
	 * = ULOAPlayerAbility와 그 C++ 자식들 (LOAPlayerAbility, LOAGA_Charge, LOAGA_Combo …). BP 클래스는 뺀다.
	 * 맨 앞은 항상 LOAPlayerAbility (기본 스킬). 나머지는 이름 순.
	 * 새 GA 종류(예: Holding)를 C++로 추가하면 코드 수정 없이 자동으로 목록에 나온다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Skill Factory")
	static TArray<FString> GetSkillParentClassNames();

private:
	/** GetSkillParentClassNames의 목록에 들어가는 클래스인가 */
	static bool IsSkillParentClass(const UClass* Class);

	/** 이름으로 부모 GA 클래스 찾기. 목록에 없는 이름이면 nullptr */
	static UClass* FindSkillParentClass(const FString& ParentClassName);

	/** 영문자와 숫자만, 첫 글자는 영문자인가 (스킬 이름, 클래스 이름 공용) */
	static bool IsValidNamePart(const FString& Name);

	/** 에셋(패키지)이 이미 있나. 디스크에 저장된 것 + 아직 저장 안 한 것(메모리) 둘 다 본다. */
	static bool DoesAssetExist(const FString& PackageName);

	/** 결과 알림 (에디터 오른쪽 아래). 성공이면 초록, 실패면 빨강 */
	static void ShowNotification(const FString& Message, bool bSuccess);
};
