// LOACheatManager.h
// [디버그] 치트 매니저. 보스 패턴 테스트용 콘솔 명령과 패턴 테스터 창(WBP)이 부르는 함수를 모은다.
// 치트 매니저 = 엔진이 PlayerController마다 하나씩 만들어 주는 디버그 전용 객체. UFUNCTION(Exec) 함수가 곧 콘솔 명령이다.
// 출시 빌드(Shipping)에서는 엔진이 치트 매니저를 아예 만들지 않는다. (UE_WITH_CHEAT_MANAGER)
//
// ─────────────────────────────────────────────────────────────────────────────
// [명령] ` 키로 콘솔 → (클라 창에서도 됨)
//   LOABossPattern                    보스 패턴 목록을 Output Log에 출력
//   LOABossPattern Downward           Downward 발동, 대상 = 나 (태그 끝부분 또는 전체 태그, 대소문자 무시)
//   LOABossPattern Downward P1        대상 = P1 (들어온 순서 1번. 리슨이면 호스트) / None = 대상 없음 / Me = 나
//   LOABossAI Pause | Resume          보스 AI 멈춤(하던 패턴도 취소) / 재개
//   LOABossHeal                       보스 체력을 최대로 (죽은 보스는 안 살림)
//   LOASlomo 0.25                     게임 전체 배속 (0.1 ~ 2, 1 = 원래대로)
//   LOAHitShape 1 | 0                 판정 모양 그리기 켜기·끄기 (= LOA.Debug.HitShape) ← 이 컴퓨터만
//   LOATester                         패턴 테스터 창 열기·닫기 (F10, DefaultInput.ini) ← 이 컴퓨터만
//
// [흐름] 클라 창에서 LOABossPattern Downward
//   [클라] LOABossPattern: 이 PC에 서버 권한이 없음 → ALOAPlayerController::ServerCheat("LOABossPattern Downward Me")
//   [서버] ServerCheat → 그 PC의 ConsoleCommand → 서버 쪽 이 클래스의 LOABossPattern
//          "Me" = 이 치트 매니저의 PC = 요청한 플레이어의 캐릭터 → ULOAMonsterAbility::TryActivatePattern
//   스탠드얼론·리슨 호스트는 권한이 있어서 바로 실행한다.
//
// [대상 이름] P1, P2 … = PlayerId 순서(들어온 순서). 클라 창 목록과 서버가 같은 순서로 센다. (PlayerId는 복제됨)
//
// [패턴 테스터 창] WBP_LOAPatternTester (BP에서 만든다)
//   열릴 때: Get Owning Player → Cheat Manager → Cast to LOACheatManager → RefreshTesterLists
//            → GetTesterPatternLabels / GetTesterTargetLabels 를 콤보 박스에 넣는다.
//   버튼:    ActivateTesterPattern(패턴 콤보 번호, 대상 콤보 번호), LOABossAI, LOASlomo, LOAHitShape, LOABossHeal
//   창 위젯 클래스 = 이 클래스의 BP 자식(B_LOACheatManager)의 Tester Window Class. BP_LOAPlayerController의 Cheat Class = B_LOACheatManager
//
// [네트워크]
//   1. 서버만  : 패턴 발동, AI 멈춤, 배속, 보스 체력 회복
//   2. 전달    : 클라의 명령 문자열 → 서버 (ALOAPlayerController::ServerCheat). 배속은 엔진이 World Settings로 복제
//   3. 각자    : 창 열기, 패턴·대상 목록 만들기, 판정 표시

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "LOACheatManager.generated.h"

class UUserWidget;
class APawn;

UCLASS(Blueprintable)
class LOA_API ULOACheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	// ---------- 콘솔 명령 (서버에서 실행. 클라면 서버로 보냄) ----------

	/** 보스 패턴 발동. 이름이 비어 있으면 목록 출력. Target: Me(기본) / None / P1, P2 … */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat")
	void LOABossPattern(const FString& PatternName = TEXT(""), const FString& Target = TEXT("Me"));

	/** 보스 AI 멈춤(Pause) / 재개(Resume) */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat")
	void LOABossAI(const FString& Mode = TEXT(""));

	/** 보스 체력을 최대로 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat")
	void LOABossHeal();

	/** 게임 전체 배속. 0.1 ~ 2로 자른다. */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat")
	void LOASlomo(float TimeDilation = 1.f);

	// ---------- 콘솔 명령 (이 컴퓨터만) ----------

	/** 판정 모양 그리기 켜기·끄기 (= LOA.Debug.HitShape) */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat")
	void LOAHitShape(bool bShow = true);

	/** 패턴 테스터 창 열기·닫기 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat")
	void LOATester();

	// ---------- 패턴 테스터 창 (이 컴퓨터만) ----------

	/** 패턴·대상 목록을 지금 상태로 다시 만든다. 창이 열릴 때(Event Construct) 부른다. */
	UFUNCTION(BlueprintCallable, Category = "LOA|Cheat|Tester")
	void RefreshTesterLists();

	/** 패턴 콤보 박스에 넣을 이름들. 예) "Downward", "Charge (대상 필수)" */
	UFUNCTION(BlueprintPure, Category = "LOA|Cheat|Tester")
	TArray<FString> GetTesterPatternLabels() const;

	/** 대상 콤보 박스에 넣을 이름들. 맨 앞 = 나, 맨 끝 = 없음. 예) "나 (P2 · Berserker)", "P1 (Berserker)", "없음" */
	UFUNCTION(BlueprintPure, Category = "LOA|Cheat|Tester")
	TArray<FString> GetTesterTargetLabels() const;

	/** 콤보 박스 번호로 발동 → LOABossPattern(이름, 대상) */
	UFUNCTION(BlueprintCallable, Category = "LOA|Cheat|Tester")
	void ActivateTesterPattern(int32 PatternIndex, int32 TargetIndex);

	/** 판정 표시가 켜져 있나 (체크박스 처음 값) */
	UFUNCTION(BlueprintPure, Category = "LOA|Cheat|Tester")
	bool IsHitShapeVisible() const;

protected:
	/** 패턴 테스터 창 위젯 (WBP_LOAPatternTester). BP 자식(B_LOACheatManager)의 Class Defaults에서 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Tester")
	TSubclassOf<UUserWidget> TesterWindowClass;

private:
	/** 이 PC에 서버 권한이 없으면 명령을 서버로 보내고 true. (권한이 있으면 false → 여기서 실행) */
	bool ForwardToServer(const FString& Command) const;

	/** [서버] 대상 이름 → 캐릭터. Me / None / P1 … 못 알아들으면 false */
	bool ResolveTarget(const FString& Target, APawn*& OutPawn) const;

	/** PlayerId 순서로 정렬한 플레이어 캐릭터 목록 (P1 = 0번) */
	TArray<APawn*> GetPlayersInJoinOrder() const;

	/** 창 목록. 이름(명령에 넣는 값)과 보이는 글자를 따로 둔다. */
	TArray<FString> PatternNames;	// 예) "Downward"
	TArray<FString> PatternLabels;	// 예) "Downward (대상 필수)"
	TArray<FString> TargetTokens;	// 예) "Me", "P1", "None"
	TArray<FString> TargetLabels;	// 예) "나 (P2 · Berserker)", "P1 (Berserker)", "없음"

	/** 열려 있는 창 (닫으면 화면에서 떼고 다시 열 때 다시 붙인다 → 열릴 때마다 Event Construct) */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TesterWindow;
};
