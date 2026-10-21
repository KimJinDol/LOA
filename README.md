# LOA — 로스트아크 스타일 3인 코옵 보스 레이드 (UE 5.5 C++)

로스트아크의 쿼터뷰 전투와 보스 레이드를 언리얼 엔진 5.5 C++와 GAS(Gameplay Ability System)로 따라 만드는 1인 프로젝트입니다.
3명이 리슨 서버로 함께 보스를 잡는 구조이고, **네트워크 위에서도 조작감이 어긋나지 않는 클라이언트**를 목표로 합니다.

- 시연 영상: (추가 예정)
- 개발 환경: Unreal Engine 5.5, C++, GAS, StateTree, Enhanced Input, Unreal Insights
- 개발 인원: 1명 (2026.10 ~ 진행 중)

## 핵심 구현

### 1. 스킬 모듈 시스템 — 플레이어와 보스가 같은 스킬 시스템을 쓴다
- 스킬(GA)은 "몽타주를 어떻게 진행하나"만 맡고, "무엇을 하나"는 **모듈**이 맡습니다.
  - `Apply`: 대상에게 효과(데미지 등)를 줌
  - `Spawn`: 투사체·장판 소환
  - `Move`: 돌진·점프
  - `Counter`: 보스 전용, 카운터 판정
- 대상을 찾는 방법은 `Targeting`(모양·자기 자신)으로 따로 나눴습니다.
- 한 스킬에 모듈 여러 개를 조립하고, 모듈끼리는 서로 모릅니다. 보스 패턴도 같은 모듈과 판정 모양을 그대로 씁니다.
- 스킬 폴더·GA·몽타주를 이름 규칙대로 한 번에 만드는 에디터 도구(`LOASkillFactory`)를 만들었습니다.

| 볼 파일 |
|---|
| `Source/LOA/AbilitySystem/SkillModules/` (`LOASkillModule.h`부터) |
| `Source/LOA/AbilitySystem/Abilities/Player/LOAPlayerAbility.h`, `Abilities/Monster/LOAMonsterAbility.h` |
| `Source/LOAEditor/SkillFactory/LOASkillFactory.h` |

### 2. 클라이언트 판정 + 서버 검증 — 내 화면에서 피했으면 안 맞는다
- **문제:** 서버가 아는 플레이어 위치는 지연만큼 늦습니다. 그래서 내 화면에서는 피했는데 서버 판정으로 맞는 일이 생겼습니다.
- **해결:** 원격 플레이어는 자기 컴퓨터의 위치로 판정하고(피해자 판정), 서버는 그 보고가 그럴듯한지 검증합니다. 부르는 쪽은 판정 방식을 몰라도 되도록 `JudgeAndApply` 한 곳에 모았습니다.
- **측정:** 판정 방식을 정하기 전에 Unreal Insights와 직접 만든 측정 명령으로 비용을 쟀습니다. 판정 계산(오버랩)은 약 3.5μs, RPC는 1명당 약 8μs였고, 이 숫자를 근거로 구조를 정했습니다.

| 볼 파일 |
|---|
| `Source/LOA/Combat/LOAHitJudgeSubsystem.h/.cpp` |
| `Source/LOA/Combat/LOACombatRPCComponent.h` (전투 RPC를 한곳에 모은 통로) |
| `Source/LOA/Debug/LOADebugCommands.cpp` (측정 명령) |

### 3. 보스 AI와 패턴
- 보스 AI는 StateTree로 대상 고르기 → 패턴 고르기 → 패턴 실행을 돕니다.
- 패턴은 **예고와 판정을 한 객체(장판 액터)가 갖게** 해서 둘이 어긋나지 않게 했습니다.
- 예고 장판은 미리 만들어 둔 것을 꺼내 쓰고(풀), 서버 시각을 보고 머티리얼이 스스로 차오르게 해서 매 프레임 갱신하지 않습니다.
- 카운터(파란 구간에 맞히면 스턴), 스턴, 사망, 체력바를 구현했습니다.
- 콘솔 명령과 패턴 테스터 창으로 원하는 패턴을 원하는 대상에게 바로 쓸 수 있습니다.

| 볼 파일 |
|---|
| `Source/LOA/AI/StateTree/` |
| `Source/LOA/Combat/Telegraph/` (`LOATelegraph`, `LOATelegraphSubsystem`) |
| `Source/LOA/Actors/Skill/LOAAreaActor.h` |
| `Source/LOA/AbilitySystem/SkillModules/Monster/LOASkillModule_Counter.h` |
| `Source/LOA/Debug/LOACheatManager.h` |

### 4. 클릭 이동 — 보스 몸을 자연스럽게 돌아간다
- **문제:** 보스 몸을 내비메시에 실시간으로 반영하자, 보스가 떨면서 걸었습니다. 내비메시를 Static으로 바꾸는 테스트 하나로 실시간 재생성이 원인임을 확인했습니다.
- **해결:** 내비메시는 고정 지형만 맡깁니다. 경로가 보스 몸(원)을 지나가면 클릭 이동이 접선을 따라 돌아가는 지점을 직접 끼워 넣습니다.
  - 짧은 쪽으로 돌고, 좌우가 자꾸 바뀌지 않게 이전 방향을 기억합니다.
  - 주변 몸 막이는 한 번만 찾고, 경로를 크게 벗어날 때만 다시 찾습니다.

| 볼 파일 |
|---|
| `Source/LOA/Input/LOAClickMoveComponent.h/.cpp` (`InsertBodyBlockDetours`) |
| `Source/LOA/Character/LOABodyBlockerComponent.h` |

### 5. 피격 이상(띄움·경직) — 서버와 클라이언트가 같은 궤적을 그린다
- **문제:** 물리(`LaunchCharacter`)로 띄우니 높이와 거리가 매번 달랐고, 클라이언트 화면과 어긋났습니다.
- **해결:** 정해진 궤적(포물선, 일정 시간)을 그리는 Root Motion Source를 만들었습니다.
  - GAS의 SetByCaller 값은 클라이언트로 복제되지 않습니다. 그래서 서버가 궤적 숫자를 정해 이동 컴포넌트로 복제하고, 서버와 클라이언트가 같은 숫자로 같은 궤적을 그립니다.
  - 지연 환경(`Net PktLag`)에서 덜컥거림이 없는지 확인했습니다.

| 볼 파일 |
|---|
| `Source/LOA/Character/Player/LOARootMotionSource_Arc.h/.cpp` |
| `Source/LOA/Character/Player/LOACharacterMovementComponent.h` (`CrowdControlMove`) |
| `Source/LOA/AbilitySystem/Abilities/Player/LOAGA_Airborne.h`, `LOAGA_Paralysis.h` |

### 그 밖에
- 콤보·차지 스킬: `LOAGA_Combo`, `LOAGA_Charge`
- 회피·기상기: `LOAGA_Dodge`, `LOAGA_WakeUp`
- 데미지 숫자: `UI/Combat/DamageNumber/`
- 직업별 스킬 꾸러미: `Data/LOAAbilityDataSet.h`
- 네트워크 시각: `Core/LOANetTime.h`

## 코드 읽는 법
- 각 헤더 맨 위에 **[역할 / 흐름 / 네트워크 / BP 설정]**을 한국어로 적어 두었습니다.
- 네트워크는 기능마다 "1. 서버만 / 2. 복제·전달 / 3. 각자 할 일" 세 갈래로 나눠 적었습니다.

## 폴더 구조
```
Source/LOA/
  AbilitySystem/   GAS: 스킬(GA), 효과(GE), 스탯, 스킬 모듈, 태스크
  AI/StateTree/    보스 AI
  Combat/          판정, 전투 RPC, 예고 장판
  Character/       플레이어·보스, 이동 컴포넌트, 몸 막이
  Input/           클릭 이동, 입력 설정
  Data/            직업·스킬 꾸러미 데이터
  Debug/           치트·측정 명령
  UI/              HUD, 위젯 컨트롤러, 데미지 숫자
Source/LOAEditor/  에디터 도구 (스킬 만들기)
```

## 저장소에 없는 것
- **Content (블루프린트·애니메이션·에셋):** Paragon, Mixamo 등 외부 애니메이션이 포함되어 있어 올리지 않았습니다. 그래서 이 저장소만으로는 실행되지 않으니, 동작은 영상으로 확인해 주세요.

## AI 활용
설계 검토와 코드 초안에 Claude를 활용했습니다. 다만 게임 규칙과 구조 결정, 코드 리뷰와 반영 승인, 빌드·테스트·측정은 직접 했습니다.
