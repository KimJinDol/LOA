// LOAPartyMemberController.h
// [배관] 파티원 한 명을 지켜보는 위젯 컨트롤러. 그 사람의 ASC·캐릭터를 보다가 파티 틀·버프 줄에 알림을 쏜다. (2026-11-10)
//
// ─────────────────────────────────────────────────────────────────────────────
// [알림 3가지]
//   OnInfoChanged(이름, 직업 이름, 직업 아이콘) : 캐릭터 준비가 끝났을 때 (직업 데이터 도착)
//   OnHealthBarChanged(체력 %, 보호막 끝 %, 보호막) : 체력·최대 체력·보호막 중 하나가 바뀜 (내 체력바와 같은 계산)
//   OnDeadChanged(죽었나)                           : State.Body.Dead 생김 / 사라짐
//
// [상태 아이콘] (2026-11-10) 상태 일은 ULOAStatusTracker가 한다. 이 컨트롤러는 하나를 들고 있다 → GetStatusTracker()
//   이 컨트롤러 = 플레이어 일 (이름·직업·체력·사망), Tracker = 상태 일 (버프·디버프·상태 이상, 남은 시간)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//   오버레이 컨트롤러가 파티가 바뀔 때(ALOAGameState::OnPartyChanged) 한 명당 하나 만든다 → Init(PlayerState)
//     ASC(PlayerState 소유)에 체력·보호막·사망 알림 걸기 + Tracker 만들어 같은 ASC로 Init
//     PlayerState::OnPawnSet → 캐릭터 → 준비 끝(OnCharacterInitialized)이면 직업 데이터로 OnInfoChanged
//   파티에서 빠지면 / HUD가 사라지면 Shutdown
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크] (ASC 복제 모드 = Mixed)
//   체력·최대 체력·보호막 : Common 스탯창 COND_None → 모든 컴퓨터
//   사망 태그             : GE가 주는 태그는 모든 컴퓨터에 복제 (Mixed의 태그 복제)
//   상태 아이콘·남은 시간 : ULOAStatusTracker [네트워크]
//   이름                  : PlayerState 복제 / 직업 : 캐릭터의 CharacterClassData 복제

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "UI/LOAOverlayWidgetController.h"	// FLOAHealthBarChangedSignature, FLOADeadChangedSignature 재사용
#include "LOAPartyMemberController.generated.h"

class APlayerState;
class APawn;
class ALOACharacter;
class UAbilitySystemComponent;
class UTexture2D;
class ULOAStatusTracker;

/** 이름표: 플레이어 이름, 직업 이름, 직업 아이콘 (없으면 nullptr) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLOAPartyMemberInfoSignature, FText, PlayerName, FText, ClassName, UTexture2D*, ClassIcon);

UCLASS(BlueprintType)
class LOA_API ULOAPartyMemberController : public UObject
{
	GENERATED_BODY()

public:
	/** 지켜볼 파티원을 받고 알림을 건다. 값은 아직 안 보낸다 (위젯이 묶은 뒤 BroadcastInitialValues) */
	void Init(APlayerState* InPlayerState, bool bInIsLocalPlayer);

	/** 지금 값을 한 번 보낸다. 같은 컨트롤러에 여러 위젯이 묶여 있으면 모두 다시 받는다 (그려도 같은 값이라 문제 없음) */
	UFUNCTION(BlueprintCallable, Category = "LOA|UI|Party")
	void BroadcastInitialValues();

	/** 걸어 둔 알림 해제. 두 번 불려도 안전 */
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "LOA|UI|Party")
	APlayerState* GetPlayerState() const { return PlayerState.Get(); }

	/** 이 컴퓨터의 플레이어(나)인가 */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Party")
	bool IsLocalPlayer() const { return bIsLocalPlayer; }

	/** 이 파티원의 상태(버프·디버프·상태 이상) 지켜보기. 아이콘 줄이 쓴다 */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Party")
	ULOAStatusTracker* GetStatusTracker() const { return StatusTracker; }

	// ---------- 위젯이 바인딩할 알림 ----------
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Party")
	FLOAPartyMemberInfoSignature OnInfoChanged;

	/** 체력바 (체력 %, 보호막 끝 %, 보호막). 계산은 ULOAOverlayWidgetController::ComputeHealthBar */
	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Party")
	FLOAHealthBarChangedSignature OnHealthBarChanged;

	UPROPERTY(BlueprintAssignable, Category = "LOA|UI|Party")
	FLOADeadChangedSignature OnDeadChanged;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	void BroadcastInfo() const;
	void BroadcastHealth() const;
	void BroadcastDead() const;

	/** 몸이 바뀜 (스폰, 부활해도 같은 몸이면 안 옴) */
	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	/** 캐릭터 준비 끝 (직업 데이터 도착) */
	UFUNCTION()
	void HandleCharacterInitialized(ALOACharacter* InitializedCharacter);

	/** 이 몸의 준비 끝 알림을 듣는다 (이전 몸은 해제). 이미 준비됐으면 바로 OnInfoChanged */
	void BindCharacter(APawn* Pawn);

	// ---------- 지켜보는 대상 (소유하지 않음) ----------
	TWeakObjectPtr<APlayerState> PlayerState;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<ALOACharacter> BoundCharacter;
	bool bIsLocalPlayer = false;

	/** 상태 지켜보기 (이 컨트롤러가 소유) */
	UPROPERTY(Transient)
	TObjectPtr<ULOAStatusTracker> StatusTracker;

	// ---------- 해제용 번호표 ----------
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeChangedHandles;
	FDelegateHandle DeadTagChangedHandle;
};
