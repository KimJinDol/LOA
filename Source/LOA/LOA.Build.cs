// LOA.Build.cs
// 이 모듈(LOA)이 어떤 엔진 기능(모듈)을 사용할지 알려주는 파일.
// 여기 이름이 없으면 해당 기능의 헤더를 include해도 링크 에러가 난다.
//
// ※ 이 파일을 수정한 뒤에는 Live Coding이 아니라
//   "에디터 종료 → IDE에서 빌드 → 에디터 실행" 순서를 지킬 것.

using UnrealBuildTool;

public class LOA : ModuleRules
{
	public LOA(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 모듈 루트 폴더를 include 기준점으로 등록.
		// 하위 폴더에 있는 파일도 #include "Core/LOALog.h" 처럼 일관된 경로로 포함할 수 있다.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			// 기본 (템플릿에 원래 있던 것)
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",

			// GAS (Gameplay Ability System): 스킬, 스탯, 버프, 네트워크 예측
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",

			// 네트워크 압축 직렬화: FVector_NetQuantize 등 (콤보 다음 타 요청 TargetData)
			"NetCore",
			
			// 클릭 이동: 내비메시 길찾기
			"NavigationSystem",

			// UI: UUserWidget(위젯 부모 클래스)이 공개 헤더(LOAUserWidget.h)에 나오므로 Public
			"UMG",

			// Project Settings 칸: 예고 설정(ULOATelegraphSettings)이 UDeveloperSettings를 상속. 엔진 모듈이라 플러그인 켤 것 없음
			"DeveloperSettings",

			// 보스·몬스터 AI: AIController(AIModule) + StateTree 실행기·노드(StateTreeModule, GameplayStateTreeModule)
			// ※ 에디터 Plugins 창에서 StateTree, GameplayStateTree 플러그인이 켜져 있어야 빌드된다.
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
		});

		// UI 내부 구현에서만 쓰는 모듈 (헤더에 노출되지 않으므로 Private)
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",

			// 레이드 결과 "나가기": Game Default Map(로비)을 읽는다 (UGameMapsSettings, 2026-11-15). 엔진 모듈이라 플러그인 켤 것 없음
			"EngineSettings",

			// 파티 찾기 (2026-11-19 apply131): 목록 = LAN 세션(OnlineSubsystem), 신청~입장 = Beacon(OnlineSubsystemUtils)
			// 플러그인이라 .uproject에서도 켠다 (OnlineSubsystem / OnlineSubsystemUtils / OnlineSubsystemNull)
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
		});

		// 이후 단계에서 필요해지면 여기에 추가한다. (지금은 넣지 않음)
		// - "ModelViewViewModel" : UI를 MVVM 방식으로 바꿀 때
	}
}