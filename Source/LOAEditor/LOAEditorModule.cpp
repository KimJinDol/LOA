// LOAEditorModule.cpp
// [배관] 에디터 전용 모듈 LOAEditor의 등록.
//
// 모듈 시작/종료 때 따로 할 일이 아직 없으므로 엔진의 기본 모듈 구현(FDefaultModuleImpl)을 쓴다.
// (나중에 메뉴 등록, 커스텀 디테일 패널 등록이 생기면 여기에 전용 모듈 클래스를 만든다)
//
// 이 모듈의 도구:
//   ULOASkillFactory (SkillFactory/LOASkillFactory.h) : "스킬 만들기" - 폴더 + GA BP + 몽타주를 한 번에
//   ULOAPlayerTesterWidget (PlayerTester/LOAPlayerTesterWidget.h) : 플레이어 테스터 에디터 창 - PIE의 플레이어 모두를 한 창에서 (자동 저가 …)

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, LOAEditor);
