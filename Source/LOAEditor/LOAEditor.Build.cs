// LOAEditor.Build.cs
// 에디터 전용 모듈(LOAEditor)이 어떤 엔진 기능(모듈)을 사용할지 알려주는 파일.
//
// [이 모듈은 무엇인가]
// 게임 코드(LOA 모듈)를 돕는 에디터 도구만 둔다. (예: 스킬 만들기 도구 ULOASkillFactory, 플레이어 테스터 창 ULOAPlayerTesterWidget)
// LOA.uproject에 "Type": "Editor"로 등록되어 있어서 에디터 빌드에서만 컴파일된다. → 패키징한 게임에는 들어가지 않는다.
//
// [방향]
// LOAEditor → LOA 는 참조한다. (GA 클래스, 클래스 데이터를 알아야 한다)
// LOA → LOAEditor 는 참조하지 않는다. (게임 코드는 에디터 도구를 모른다)
//
// ※ 이 파일을 수정한 뒤에는 Live Coding이 아니라
//   "에디터 종료 → IDE에서 빌드 → 에디터 실행" 순서를 지킬 것.

using UnrealBuildTool;

public class LOAEditor : ModuleRules
{
	public LOAEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// LOA 모듈과 같은 include 규칙: #include "SkillFactory/LOASkillFactory.h" 처럼 모듈 루트 기준
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",

			// 게임 모듈: GA 클래스(ULOAPlayerAbility …), 클래스 데이터(ULOACharacterClassData)
			// 공개 헤더(LOASkillFactory.h)에 LOA 타입이 나오므로 Public
			"LOA",

			// 에디터 창(에디터 유틸리티 위젯)의 부모 UEditorUtilityWidget. 공개 헤더(LOAPlayerTesterWidget.h)의 부모라 Public (2026-11-05)
			"Blutility",
			// 위젯 부품: 콤보 박스, 스핀 박스, 버튼, 텍스트 (LOAPlayerTesterWidget.h)
			"UMG",
		});

		// 내부 구현에서만 쓰는 모듈 (헤더에 노출되지 않으므로 Private)
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// 에디터 기능: BP 생성(FKismetEditorUtilities), 몽타주 팩토리, 저장, 콘텐츠 브라우저 이동
			"UnrealEd",
			// 에셋 생성(IAssetTools::CreateAsset)
			"AssetTools",
			// 같은 이름 에셋이 이미 있는지 확인
			"AssetRegistry",
			// GA BP의 종류 = UGameplayAbilityBlueprint (콘텐츠 브라우저의 "Gameplay Ability Blueprint"와 같은 종류)
			"GameplayAbilities",
			// 결과 알림 토스트 (에디터 오른쪽 아래)
			"Slate",
			"SlateCore",
		});
	}
}
