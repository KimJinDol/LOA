// LOAPlayerCheats.h
// [디버그] 플레이어 테스터. 플레이어 쪽 테스트용 콘솔 명령과 플레이어 테스터 창(WBP)이 부르는 함수를 모은다. (2026-11-04)
// 보스 쪽(패턴 발동, AI 멈춤 …)은 ULOACheatManager. 이 클래스는 그 치트 매니저에 붙는 확장(UCheatManagerExtension)이다.
// 출시 빌드(Shipping)에서는 엔진이 치트 매니저를 아예 만들지 않으므로 이 확장도 없다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 확장인가]
//   엔진이 콘솔 명령을 치트 매니저 다음에 확장들에도 넘긴다 (엔진 CheatManager.cpp ProcessConsoleExec)
//   → 여기 만든 UFUNCTION(Exec)도 그대로 콘솔 명령. 보스용과 플레이어용이 한 파일에 섞이지 않고, 상태(자동 저가 설정·타이머)를 따로 들고 있는다.
//   ULOACheatManager::InitCheatManager가 이 클래스(또는 BP 자식)를 만들어 붙인다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [명령] ` 키로 콘솔 (어느 창에서나)
//   LOAAutoGuard P2 0.3        P2: 저가 구간이 열리고 0.3초 뒤 자동으로 G (진짜 G와 같은 길)
//   LOAAutoGuard All 0.3       모든 플레이어
//   LOAAutoGuard Me Off        나: 끔 (Me = 명령을 친 플레이어)
//   LOAPlayerTester            플레이어 테스터 창 열기·닫기 (Shift+F10, DefaultInput.ini) ← 이 컴퓨터만
//
//   [보호막·회복 테스트] (2026-11-07) 진짜 GE와 계산(MMC·Execution)을 그대로 탄다. 양은 최종 숫자로 받는다
//   LOAShield 500 3 1          나: 보호막 500을 3초. 마지막 숫자 = 스킬 번호 (1 = 내 첫 번째 스킬 …, 0 = 스킬 없음)
//   LOAShield 300 4 2 P2       P2: 보호막 300을 4초, 두 번째 스킬로 건 것
//   LOAHeal 300 [대상]          300 회복
//   LOAHurt 400 [대상]          400 데미지 (보호막이 있으면 먼저 깎임)
//   LOAShieldInfo [대상]        걸린 보호막을 하나씩 로그에 (스킬, 지금 몫, 깎인 양, 남은 시간)
//
//   [낙인·공격력 버프 테스트] (2026-11-07) 시전자가 따로 있다 (낙인력·공격력을 시전자에게서 읽으므로)
//   LOAMark 5 [시전자]          보스에게 낙인 5초. 시전자 기본 Me. 받는 피해 증가 = 0.1 × (1 + 시전자 낙인력)
//   LOABuff 10 [대상] [시전자]   대상에게 공격력 버프 10초. 대상·시전자 기본 Me.
//                              (대상 공격력 + 시전자 기본 공격력 × 0.15) × 1.06  (로아 규칙, 사용자 결정 2026-11-08. 계산은 GE_AttackBuff 보정치 두 개)
//                              예) 서포터 1000, 딜러 2000 → (2000 + 150) × 1.06 = 2279
//   LOAMarkPower 0.08 [대상]    낙인력 기본값을 바꾼다 (8%). 테스트용
//   LOAAttackPower 3000 [대상]  공격력 기본값을 바꾼다. 버프 테스트용 (서포터·딜러 공격력을 다르게)
//
//   [아이덴티티 테스트] (2026-11-09)
//   LOAIdentity                 나: 아이덴티티 게이지 가득
//   LOAIdentity 40 [대상]       게이지를 40으로 (0 ~ 최대로 맞춤). 대상 Me / P1 … / All
//                               진짜 수급과 같은 GE(ULOAGE_IdentityGain)로 바꾼다 → 범위 맞추기·복제·UI가 그대로 탄다
//                               켜져 있는 동안에도 바뀐다 (수급 규칙의 "켜진 동안 안 참"은 스킬 수급에만 적용)
//
// [보호막·회복 테스트 규칙]
//   시전자 = 대상 자신 (자기에게 건다). 대상은 Me(기본) / P1, P2 … / All
//   양 → 배율: 계산식이 "시전자 공격력 × 배율"이라 배율 = 양 ÷ 대상 공격력 으로 바꿔 넣는다
//     예) 공격력 1000, LOAShield 500 → ShieldRate 0.5 → MMC가 1000 × 0.5 = 500
//   스킬 번호: 덮어쓰기(ULOAGEComponent_ShieldOverride)는 "같은 시전자 + 같은 스킬(GA)"을 본다.
//     치트에는 스킬이 없으므로 번호 → 대상의 스킬 목록(ASC GetActivatableAbilities) 순서로 짝지어 Effect Context에 적는다
//     예) LOAShield 500 3 1 을 두 번 → 같은 스킬 → 500 (1000 아님) / 1과 2 → 다른 스킬 → 쌓임
//     번호가 범위 밖이면 로그에 스킬 목록을 찍고 아무것도 안 건다
//   쓰는 GE는 BP 자식(B_LOAPlayerCheats)의 Shield / Heal / Hurt Effect Class (GE_Shield, GE_Heal, GE_Damage_Basic)
//
// [자동 저가] (그 플레이어 컴퓨터에서 돈다)
//   구간 열림 알림(ULOAJustGuardComponent::OnWindowOpened) → N초 뒤 누름 (저가 GA에 붙은 입력 태그로 AbilityInputTagPressed)
//   뗌: 단타 = 누르고 0.1초 뒤 / 홀딩 = 구간 끝(OnWindowClosed)
//   N은 "구간이 열린 뒤"다. 클라는 보스가 언제 때릴지 미리 모른다 → 구간을 첫 타 몇 초 전에 칠했는지 보고 N을 고른다
//     예) 첫 타 0.6초 전에 칠한 구간: N 0.3 = 타격 0.3초 전 (막음) / N 0.9 = 타격 뒤 (늦어서 실패)
//   캐릭터가 바뀌면(부활) PC의 빙의 변경 알림(OnPossessedPawnChanged)을 듣고 새 캐릭터에 다시 붙는다
//
// [흐름] 아무 창에서 LOAAutoGuard P2 0.3
//   [그 창]     서버 권한이 없으면 → ALOAPlayerController::ServerCheat("LOAAutoGuard P2 0.300")
//   [서버]      대상 찾기 (ULOACheatManager::ResolveTarget) → 그 플레이어의 PC
//               리슨 호스트 본인 PC면 여기서 바로 / 원격이면 ClientCheat("LOAAutoGuardLocal 0.300")
//   [P2 컴퓨터] LOAAutoGuardLocal → 자동 저가 켬
//
// [플레이어 테스터 창] WBP_LOAPlayerTester (BP에서 만든다)
//   열릴 때: Get Owning Player → Cheat Manager → Find Cheat Manager Extension(LOAPlayerCheats) → RefreshPlayerTesterLists
//            → GetPlayerTesterLabels 를 콤보 박스에
//   버튼:    SetTesterAutoGuard(콤보 번호, 초, 켜기)
//   창 위젯 클래스 = 이 클래스의 BP 자식(B_LOAPlayerCheats)의 Tester Window Class.
//   B_LOACheatManager의 Player Cheats Class = B_LOAPlayerCheats
//
// [에디터 창] (2026-11-05) 에디터에서는 LOAEditor 모듈의 ULOAPlayerTesterWidget(EUW_PlayerTester)이 PIE 월드마다 이 확장을 찾아
//   LOAAutoGuardLocal을 바로 부른다 (서버를 거치지 않음). 위 [플레이어 테스터 창]은 패키징 테스트용 인게임 창 (아직 안 만듦)
//
// [나중에 붙일 것 (예)] 무적, 마나 무한, 쿨타임 초기화, 지정한 CC를 그 플레이어에게, 자동 회피 → 이 클래스에 명령 하나 + 창에 줄 하나
//
// [네트워크]
//   1. 서버만  : 대상 찾기, 그 플레이어 컴퓨터로 보내기
//   2. 전달    : 명령 문자열 (그 창 → 서버 ServerCheat / 서버 → 그 플레이어 ClientCheat)
//   3. 각자    : 자동 저가 (그 플레이어 컴퓨터), 창

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"
#include "LOAPlayerCheats.generated.h"

struct FGameplayAttribute;
class UUserWidget;
class APawn;
class ULOACheatManager;
class ULOAJustGuardComponent;
class UGameplayEffect;
class UAbilitySystemComponent;

UCLASS(Blueprintable)
class LOA_API ULOAPlayerCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	//~ Begin UCheatManagerExtension Interface
	/** 붙을 때: 이 PC의 빙의 변경 알림을 듣는다 (자동 저가가 새 캐릭터에 다시 붙게) */
	virtual void AddedToCheatManager_Implementation() override;
	/** 떨어질 때: 자동 저가를 끄고 구독을 푼다 */
	virtual void RemovedFromCheatManager_Implementation() override;
	//~ End UCheatManagerExtension Interface

	// ---------- 콘솔 명령 ----------

	/**
	 * 자동 저가. Target: Me(기본) / P1, P2 … / All. Delay: 초(구간이 열린 뒤) 또는 Off
	 * 서버에서 대상을 찾아 그 플레이어 컴퓨터로 보낸다 (헤더 [흐름])
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAAutoGuard(const FString& Target = TEXT("Me"), const FString& Delay = TEXT(""));

	/** [이 컴퓨터만] 자동 저가 설정 (서버의 ClientCheat가 부른다. 직접 쳐도 됨). Delay < 0 = 끔 */
	UFUNCTION(Exec)
	void LOAAutoGuardLocal(float Delay);

	/** [이 컴퓨터] 자동 저가 설정. 구간이 열린 뒤 누를 때까지 (초). 음수 = 꺼짐. 에디터 창(ULOAPlayerTesterWidget)이 상태를 보여 줄 때 (2026-11-05) */
	float GetAutoGuardDelay() const { return AutoGuardDelay; }

	/** 플레이어 테스터 창 열기·닫기 (이 컴퓨터만) */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAPlayerTester();

	// ---------- 보호막·회복 테스트 (2026-11-07, 헤더 [보호막·회복 테스트 규칙]) ----------

	/** 보호막. Amount = 최종 양, Duration = 초, Skill = 스킬 번호 (0 = 없음). Target: Me / P1 … / All */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAShield(float Amount = 500.f, float Duration = 3.f, int32 Skill = 0, const FString& Target = TEXT("Me"));

	/** 회복. Amount = 최종 양 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAHeal(float Amount = 300.f, const FString& Target = TEXT("Me"));

	/** 데미지. Amount = 최종 양 (보호막이 있으면 먼저 깎임) */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAHurt(float Amount = 400.f, const FString& Target = TEXT("Me"));

	/** 걸린 보호막을 하나씩 로그에 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAShieldInfo(const FString& Target = TEXT("Me"));

	// ---------- 낙인·공격력 버프 테스트 (2026-11-07) ----------

	/** 보스에게 낙인. Duration = 초, Caster = 낙인력을 읽을 사람 (Me / P1 …) */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAMark(float Duration = 5.f, const FString& Caster = TEXT("Me"));

	/** 공격력 버프. Target = 받는 사람 (Me / P1 … / All), Caster = 공격력을 읽을 힐러 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOABuff(float Duration = 10.f, const FString& Target = TEXT("Me"), const FString& Caster = TEXT("Me"));

	/** 낙인력 기본값 바꾸기 (0.08 = 8%) */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAMarkPower(float Value = 0.08f, const FString& Target = TEXT("Me"));

	/** 공격력 기본값 바꾸기 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAAttackPower(float Value = 1000.f, const FString& Target = TEXT("Me"));

	// ---------- 아이덴티티 테스트 (2026-11-09) ----------

	/** 아이덴티티 게이지를 Value로 (음수 = 가득). 헤더 [명령] */
	UFUNCTION(Exec, BlueprintCallable, Category = "LOA|Cheat|Player")
	void LOAIdentity(float Value = -1.f, const FString& Target = TEXT("Me"));

	// ---------- 플레이어 테스터 창 (이 컴퓨터만) ----------

	/** 플레이어 목록을 지금 상태로 다시 만든다. 창이 열릴 때(Event Construct) 부른다 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Cheat|Player|Tester")
	void RefreshPlayerTesterLists();

	/** 플레이어 콤보 박스에 넣을 이름들. 맨 앞 = 모두. 예) "모두", "나 (P2 · Berserker)", "P1 (Berserker)" */
	UFUNCTION(BlueprintPure, Category = "LOA|Cheat|Player|Tester")
	TArray<FString> GetPlayerTesterLabels() const;

	/** 콤보 박스 번호로 자동 저가 → LOAAutoGuard(대상, 초 또는 Off) */
	UFUNCTION(BlueprintCallable, Category = "LOA|Cheat|Player|Tester")
	void SetTesterAutoGuard(int32 PlayerIndex, float Delay, bool bOn);

protected:
	/** 플레이어 테스터 창 위젯 (WBP_LOAPlayerTester). BP 자식(B_LOAPlayerCheats)의 Class Defaults에서 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Player|Tester")
	TSubclassOf<UUserWidget> TesterWindowClass;

	/** [보호막·회복 테스트] 쓰는 GE. BP 자식(B_LOAPlayerCheats)에서 GE_Shield / GE_Heal / GE_Damage_Basic */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Player|Effect")
	TSubclassOf<UGameplayEffect> ShieldEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Player|Effect")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Player|Effect")
	TSubclassOf<UGameplayEffect> HurtEffectClass;

	/** [낙인·공격력 버프 테스트] GE_Mark / GE_AttackBuff (2026-11-07) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Player|Effect")
	TSubclassOf<UGameplayEffect> MarkEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Player|Effect")
	TSubclassOf<UGameplayEffect> AttackBuffEffectClass;

	/** [자동 저가] 단타일 때 누르고 뗄 때까지 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Cheat|Player", meta = (ClampMin = "0.01", Units = "Seconds"))
	float AutoGuardTapTime = 0.1f;

private:
	/** 이 확장이 붙은 치트 매니저 (공용 도우미: 서버로 보내기, 대상 찾기) */
	ULOACheatManager* GetLOACheatManager() const;

	/** [서버] 그 플레이어 컴퓨터에서 이 확장의 명령을 실행 (리슨 호스트 본인이면 바로, 원격이면 ClientCheat) */
	void RunOnPlayerComputer(APawn* Player, const FString& LocalCommand) const;

	/** [서버] 대상 이름 → 캐릭터 목록. All = 모두. 못 알아들으면 빈 목록 + 경고 */
	TArray<APawn*> ResolvePlayers(const FString& Target) const;

	// ---------- 보호막·회복 테스트 (서버) ----------

	/**
	 * 대상 자신이 시전자로 GE를 건다. 배율 = Amount ÷ 대상 공격력 → RateTag에 넣는다
	 * Duration > 0 이면 SetByCaller.Duration, Skill > 0 이면 그 번호의 스킬을 Effect Context에 적는다
	 * 성공하면 true
	 */
	bool ApplyTestEffect(APawn* Player, TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& RateTag,
		float Amount, float Duration, int32 Skill, const TCHAR* What) const;

	/** 대상의 보호막 GE를 하나씩 로그에 */
	void LogShields(APawn* Player) const;

	// ---------- 낙인·공격력 버프 테스트 (서버) ----------

	/** 시전자 → 대상으로 GE를 건다 (시간만 SetByCaller.Duration). 성공하면 true */
	bool ApplyEffectFromCaster(UAbilitySystemComponent* CasterASC, UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> EffectClass, float Duration, const TCHAR* What) const;

	/** 대상들의 스탯 기본값을 바꾼다 (테스트용, GE 없이) */
	void SetBaseStat(const FString& Target, const FGameplayAttribute& Attribute, float Value, const TCHAR* What) const;

	// ---------- 자동 저가 (이 컴퓨터) ----------

	/** 지금 내 캐릭터의 저가 컴포넌트에 붙는다 (이미 붙어 있으면 그대로) */
	void BindAutoGuard();

	/** 붙은 저가 컴포넌트에서 떨어지고 걸어 둔 타이머를 지운다 */
	void UnbindAutoGuard();

	/** 이 PC의 캐릭터가 바뀜 (부활 등) → 켜져 있으면 새 캐릭터에 다시 붙는다 */
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** 구간 열림 → Delay초 뒤 누르기 예약 */
	void OnWindowOpened(int32 WindowId);

	/** 구간 끝 → 홀딩으로 누르고 있던 구간이면 뗀다 */
	void OnWindowClosed(int32 WindowId);

	/** 누름 (단타면 Tap Time 뒤 뗌 예약, 홀딩이면 구간 끝까지) */
	void PressGuard(int32 WindowId);

	/** 뗌 */
	void ReleaseGuard();

	/** 내 캐릭터의 저가 GA에 붙은 입력 태그 (꾸러미에서 준 InputTag. 예: InputTag.JustGuard). 없으면 빈 태그 */
	FGameplayTag FindJustGuardInputTag() const;

	/** 구간이 열린 뒤 누를 때까지 (초). 음수 = 꺼짐 */
	float AutoGuardDelay = -1.f;

	/** 홀딩으로 누르고 있는 구간 (0 = 없음) */
	int32 HeldWindowId = 0;

	/** 붙은 저가 컴포넌트와 구독 번호 */
	TWeakObjectPtr<ULOAJustGuardComponent> BoundJustGuardComp;
	FDelegateHandle WindowOpenedHandle;
	FDelegateHandle WindowClosedHandle;

	/** 걸어 둔 타이머 (끌 때 지운다) */
	TArray<FTimerHandle> PendingTimers;

	// ---------- 창 ----------

	/** 창 목록. 명령에 넣는 값과 보이는 글자를 따로 둔다 */
	TArray<FString> PlayerTokens;	// 예) "All", "Me", "P1"
	TArray<FString> PlayerLabels;	// 예) "모두", "나 (P2 · Berserker)", "P1 (Berserker)"

	/** 열려 있는 창 */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TesterWindow;
};
