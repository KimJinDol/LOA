// LOAPlayerTesterWidget.h
// [에디터 도구] 플레이어 테스터 에디터 창. PIE로 띄운 플레이어 모두를 에디터 탭 하나에서 다룬다. (2026-11-05)
// 첫 기능: 자동 저가 (ULOAPlayerCheats). 혼자서도 여러 명의 저가를 시험하려고.
// 2026-11-09: 아이덴티티 게이지 가득 / 0 (ULOAPlayerCheats::LOAIdentity) + 상태에 게이지 표시
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 에디터 창인가]
//   인게임 창(Shift+F10)은 그 플레이어 화면에만 뜨고, 다른 플레이어는 서버를 거쳐야 한다.
//   에디터는 PIE 월드(서버 1 + 클라 N)를 모두 볼 수 있다 → 창 하나로 모두 켜고 끄고, 모두의 상태를 본다.
//   조건: 에디터 설정 Play → Multiplayer Options → Run Under One Process 켜짐 (기본값).
//         꺼져 있으면 클라가 별도 프로그램으로 떠서 에디터가 닿지 못한다 (엔진 LevelEditorPlaySettings.h RunUnderOneProcess)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//   탭이 열릴 때(NativeConstruct) 버튼 연결 + 0.5초마다 Refresh
//     ※ NativeOnInitialized는 주인 플레이어가 없는 위젯(에디터 창)에서는 안 불린다 (엔진 UserWidget.cpp Initialize) → 거기 두지 않는다 (2026-11-05)
//     PIE 월드마다 그 화면의 PC(IsLocalController) → 치트 매니저 → 확장 ULOAPlayerCheats
//     PlayerId 순서로 P1, P2 … (게임 안 LOAAutoGuard P2와 같은 번호)
//     콤보 박스 목록 (바뀌었을 때만 다시 채움, 고른 줄 유지) + 상태 글자
//     진단 로그: 찾은 수가 바뀔 때와 버튼을 누를 때 한 줄 (LogLOA: [PlayerTester] PIE worlds 3, local PCs 3, with cheats 3, players 3)
//   켜기·끄기 버튼
//     고른 플레이어(또는 모두)의 ULOAPlayerCheats::LOAAutoGuardLocal(초)을 바로 부른다
//     서버를 거치지 않는다. 누르기는 원래 그 플레이어 화면(클라 월드)에서 하므로 결과는 실제와 같다
//
// ─────────────────────────────────────────────────────────────────────────────
// [쓰는 법] EUW_PlayerTester (에디터 유틸리티 위젯, 직접 만든다)
//   부모 = 이 클래스. 아래 이름으로 위젯을 놓으면 엔진이 자동으로 이어 준다 (BindWidget). BP 노드 연결 없음
//     PlayerCombo         콤보 박스 (Combo Box String)   모두 / P1 (호스트 · Berserker) / P2 (클라 · …)
//     DelaySpin           스핀 박스                       구간이 열린 뒤 몇 초에 누를지 (기본값은 디자이너에서, 예: 0.3)
//     AutoGuardOnButton   버튼                            켜기
//     AutoGuardOffButton  버튼                            끄기
//     StatusText          텍스트                          플레이어마다 상태
//     IdentityFillButton  버튼 (없어도 됨)                아이덴티티 게이지 가득 (2026-11-09)
//     IdentityEmptyButton 버튼 (없어도 됨)                아이덴티티 게이지 0
//
// [아이덴티티 버튼] 고른 플레이어의 확장에서 LOAIdentity(값, "Me")를 부른다
//   그 확장이 클라 화면 것이면 콘솔과 같은 길로 서버에 보낸다 (ForwardToServer → ServerCheat) → 서버가 GE로 바꿈
//   상태 글자에 "아덴 64/100 (켜짐)" (그 화면의 복제된 값). 게이지가 없는 직업은 안 씀
//   열기: 콘텐츠 브라우저에서 EUW_PlayerTester 우클릭 → Run Editor Utility Widget. 탭은 PIE 전에 열어 두면 된다
//
// [나중에 붙일 것] ULOAPlayerCheats에 명령 하나 → 여기에 버튼 하나 (같은 방식: 고른 플레이어의 확장 함수를 부른다)
//
// [네트워크] 없음. 에디터가 같은 프로그램 안의 월드들을 직접 본다.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Engine/TimerHandle.h"
#include "LOAPlayerTesterWidget.generated.h"

class UComboBoxString;
class USpinBox;
class UButton;
class UTextBlock;
class ULOAPlayerCheats;

UCLASS(Abstract)
class LOAEDITOR_API ULOAPlayerTesterWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	/** 탭이 열릴 때: 버튼 연결, 바로 한 번 그리고, 0.5초마다 다시 그리기 시작 */
	virtual void NativeConstruct() override;
	/** 탭이 닫힐 때: 다시 그리기 멈춤 */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ---------- 위젯 (EUW_PlayerTester에 같은 이름으로 놓는다) ----------

	/** 대상. 0번 = 모두, 1번부터 P1, P2 … */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> PlayerCombo;

	/** 구간이 열린 뒤 몇 초에 누를지 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USpinBox> DelaySpin;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AutoGuardOnButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AutoGuardOffButton;

	/** 플레이어마다 상태. 예) "P2 (클라 · Berserker): 자동 저가 켬 0.30초" */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** 아이덴티티 게이지 가득 (2026-11-09). 없어도 창은 열린다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> IdentityFillButton;

	/** 아이덴티티 게이지 0 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> IdentityEmptyButton;

	/** 다시 그리는 간격 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Tester", meta = (ClampMin = "0.1", Units = "Seconds"))
	float RefreshInterval = 0.5f;

private:
	/** PIE에서 찾은 플레이어 한 명 */
	struct FTesterPlayer
	{
		TWeakObjectPtr<ULOAPlayerCheats> Cheats;
		TWeakObjectPtr<APawn> Pawn;	// 상태 글자에 게이지 (그 화면의 캐릭터)
		int32 PlayerId = 0;
		FString Label;		// 예) "P2 (클라 · Berserker)"
		bool bHasPawn = false;
	};

	/** PIE 월드를 다시 보고 목록·상태 글자를 새로 그린다. bForceLog = 진단 로그를 숫자가 같아도 찍는다 (버튼) */
	void Refresh(bool bForceLog = false);

	/** 타이머용 (인자 없음) */
	void RefreshByTimer() { Refresh(false); }

	/**
	 * PIE 월드마다 그 화면의 플레이어를 찾아 PlayerId 순서로 (P1 = 0번)
	 * @param OutDiagnostic  단계마다 찾은 수. 예) "PIE worlds 3, local PCs 3, with cheats 3, players 3"
	 */
	TArray<FTesterPlayer> FindPlayers(FString& OutDiagnostic) const;

	/** 콤보 박스 목록이 바뀌었으면 다시 채운다 (고른 줄은 같은 글자로 유지) */
	void UpdateCombo();

	/** 상태 글자 */
	void UpdateStatus() const;

	/** 콤보 박스에서 고른 대상 (모두면 전부) */
	TArray<ULOAPlayerCheats*> GetSelectedCheats() const;

	UFUNCTION()
	void OnAutoGuardOnClicked();

	UFUNCTION()
	void OnAutoGuardOffClicked();

	/** 고른 대상의 자동 저가 설정. Delay < 0 = 끔 */
	void SetAutoGuard(float Delay);

	UFUNCTION()
	void OnIdentityFillClicked();

	UFUNCTION()
	void OnIdentityEmptyClicked();

	/** 고른 대상의 아이덴티티 게이지를 Value로 (음수 = 가득) */
	void SetIdentity(float Value);

	/** 상태 글자용: " · 아덴 64/100 (켜짐)". 게이지가 없으면 빈 글자 */
	static FString GetIdentityText(const APawn* Pawn);

	/** 마지막으로 찾은 플레이어 (P1 = 0번) */
	TArray<FTesterPlayer> Players;

	/** 지금 콤보 박스에 들어 있는 글자 (바뀌었는지 비교용) */
	TArray<FString> ComboOptions;

	/** 마지막 진단 글자 (바뀌었을 때만 로그) */
	FString LastDiagnostic;

	/** 다시 그리기 타이머 (에디터 타이머: PIE가 멈춰도 돈다) */
	FTimerHandle RefreshTimer;
};
