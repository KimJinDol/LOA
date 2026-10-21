// LOABossWidgetController.h
// [배관] 보스 체력바용 위젯 컨트롤러. 보스의 ASC를 지켜보다가 위젯에게 알림을 쏜다. (Overlay 컨트롤러와 같은 역할 분담)
//
// ─────────────────────────────────────────────────────────────────────────────
// [알림 2가지]
//   OnBossInfoChanged(이름, 줄 수)      : 보스가 정해졌을 때 한 번. 이름표와 "x줄" 계산용
//   OnBossHealthChanged(현재, 최대)     : 체력 또는 최대 체력이 바뀜 (FLOAVitalChangedSignature, 내 체력 바와 같은 모양)
//
// 위젯은 보스가 누구인지, ASC가 어디 있는지 모른다. 이 두 알림만 받아서 그린다.
//
// [줄 수 계산은 위젯이 한다] (로아 보스 체력바 "x123")
//   한 줄 체력 = 최대 ÷ 줄 수
//   남은 줄    = 올림(현재 ÷ 한 줄 체력)        ← "x123" 숫자
//   지금 줄 채움 = (현재 - (남은 줄 - 1) × 한 줄 체력) ÷ 한 줄 체력   ← 바 길이 0~1
//   그리기 방식(줄마다 색 바꾸기 등)은 화면 일이라 위젯 BP에 둔다. 숫자는 이 컨트롤러가 그대로 넘긴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//   HUD가 RaidSubsystem의 OnBossChanged(보스)를 받음
//     → 이 컨트롤러 생성 → Init(보스)          : ASC에 체력 알림 걸기
//     → 위젯 생성 → SetWidgetController       : BP가 OnWidgetControllerSet에서 알림에 바인딩
//     → BroadcastInitialValues                 : 이름/줄 수 + 지금 체력 한 번
//   HUD가 OnBossChanged(nullptr)를 받거나 HUD가 사라짐
//     → Shutdown                               : 걸어 둔 알림 해제
//
// [네트워크]
// - 본인 컴퓨터에만 있다. (HUD가 만든다)
// - 보스 체력은 서버가 바꾸고 복제로 도착하면 ASC가 알림을 준다. (Common의 체력은 COND_None → 모든 클라이언트가 받음)
// - 클라이언트에서는 보스가 등록된 직후 체력이 아직 0일 수 있다. (초기화 GE 결과가 복제되기 전) 도착하면 알림이 다시 온다.
//   → 위젯은 최대가 0이면 바를 비워 두거나 숨긴다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayEffectTypes.h"
#include "UI/LOAOverlayWidgetController.h"	// FLOAVitalChangedSignature (현재, 최대) 재사용
#include "LOABossWidgetController.generated.h"

class ALOABossCharacter;
class UAbilitySystemComponent;

/** 보스 이름표. 줄 수는 1 이상 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLOABossInfoSignature, FText, BossName, int32, HealthBarLines);

UCLASS(Blueprintable, BlueprintType)
class LOA_API ULOABossWidgetController : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 지켜볼 보스를 받고, 보스 ASC에 체력 알림을 건다. [본인 컴퓨터] HUD가 한 번 호출한다.
	 * 아직 위젯에 값을 보내지는 않는다. (위젯이 바인딩을 끝낸 뒤 BroadcastInitialValues로 보낸다)
	 */
	void Init(ALOABossCharacter* InBoss);

	/** 이름/줄 수 + 지금 체력을 한 번 보낸다. 위젯이 OnWidgetControllerSet에서 바인딩을 끝낸 "다음"에 HUD가 호출한다. */
	void BroadcastInitialValues();

	/** 걸어 둔 알림을 전부 해제한다. HUD가 체력바를 내릴 때 호출. 두 번 불려도 안전 */
	void Shutdown();

	/** 지켜보는 보스 (위젯이 직접 쓸 일은 거의 없다. 나중에 무력화 바 등 보스 전용 표시가 필요하면) */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Boss")
	ALOABossCharacter* GetBoss() const { return Boss.Get(); }

	// ---------- 위젯이 바인딩할 알림 ----------
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Boss")
	FLOABossInfoSignature OnBossInfoChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Boss")
	FLOAVitalChangedSignature OnBossHealthChanged;

private:
	/** Health, MaxHealth 둘 중 무엇이 바뀌어도 여기로 온다. 현재/최대를 다시 읽어서 보낸다 */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void BroadcastHealth() const;

	// ---------- 지켜보는 대상 (소유하지 않음) ----------
	TWeakObjectPtr<ALOABossCharacter> Boss;

	/** 보스 몸에 있는 ASC. 보스가 사라지면 같이 사라진다 */
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	// ---------- 해제용 번호표 ----------
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
};
