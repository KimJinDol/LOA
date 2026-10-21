// LOAPlayerController.h
// [배관] 플레이어의 "입력 담당". 키보드/마우스 입력을 받아 알맞은 곳으로 넘긴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [하는 일 3가지]
//
//   1) 스킬 입력  : InputConfig의 AbilityInputActions를 전부 바인딩
//                   눌림/뗌 → InputTag → PlayerState의 ASC로 전달 (어떤 스킬인지는 ASC가 찾음)
//   2) 이동 입력  : 클릭 이동 부품(ULOAClickMoveComponent)을 켜 준다. 이동 처리는 부품이 전부 담당
//   3) 마우스 커서: 탑쿼터뷰 게임이므로 커서를 항상 보이게 하고, 우클릭 중에도 숨기지 않는다
//   4) HUD 초기화 : "캐릭터 준비 완료"와 "HUD 생성" 둘 다 확인되면 HUD에게 화면을 만들라고 한다
//   5) 전투 RPC 컴포넌트(ULOACombatRPCComponent)를 붙여 둔다. 서버 ↔ 이 플레이어 사이의 전투 RPC는 컴포넌트가 전부 담당
//
// [HUD 초기화 - 왜 여기서 하나]
// PlayerController = "이 컴퓨터 앞에 앉은 사람". HUD도 이 사람 것이다. 그래서 둘을 잇는 일을 여기서 한다.
// 캐릭터는 몸이라 UI를 모른다. 캐릭터 코드는 수정하지 않고, 캐릭터의 OnCharacterInitialized 알림만 구독한다.
//
// 아래 두 가지는 도착 순서가 보장되지 않는다. (클라이언트는 네트워크 상황에 따라 매번 다름)
//   (가) 캐릭터 준비 완료  - SetPawn에서 확인. 아직이면 OnCharacterInitialized를 구독해서 기다림
//   (나) HUD 생성         - ClientSetHUD에서 확인
// 두 경로 모두 TryInitHUD()로 모이고, 늦게 도착한 쪽이 실제 초기화를 한다. (캐릭터의 택배 A/B와 같은 방식)
//
// [하지 않는 일]
//   - 어떤 스킬이 나가는지 결정 (ASC + AbilityDataSet)
//   - 이동 경로 계산 (클릭 이동 부품)
//   - 캐릭터 몸 제어 (캐릭터)
//
// ─────────────────────────────────────────────────────────────────────────────
// [직업마다 다른가?]
// 아니다. 모든 직업이 같은 키 배치를 쓰므로 PlayerController는 하나, InputConfig도 하나다.
// 직업 차이는 "어떤 InputTag에 어떤 Ability가 있나"(AbilityDataSet)에서만 생긴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - PlayerController는 서버와 "그 플레이어 본인 컴퓨터"에만 존재한다. (다른 플레이어 컴퓨터에는 없음)
// - 입력 처리는 본인 컴퓨터에서만 일어난다. 서버의 원격 플레이어용 PlayerController는 입력을 받지 않는다.
// - 스킬 발동 요청을 서버로 보내는 일은 ASC(GAS)가, 이동 동기화는 CharacterMovementComponent가 담당한다.
// - 이 클래스에 직접 만든 RPC는 디버그용 ServerCheat 하나뿐이다. (치트 매니저의 명령을 서버로 보냄, 개발 빌드만)
//   전투 RPC(데미지 숫자, 예고, 피해자 판정 보고 등)는 전투 RPC 컴포넌트가 가진다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "LOAPlayerController.generated.h"

class UInputMappingContext;
class ULOAInputConfig;
class ULOAClickMoveComponent;
class ULOACombatRPCComponent;
class ULOAAbilitySystemComponent;
class ALOACharacter;

UCLASS()
class LOA_API ALOAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALOAPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "LOA|PlayerController")
	ULOAClickMoveComponent* GetClickMoveComponent() const { return ClickMoveComponent; }

	UFUNCTION(BlueprintPure, Category = "LOA|PlayerController")
	ULOACombatRPCComponent* GetCombatRPCComponent() const { return CombatRPCComponent; }

	//~ Begin AController Interface
	/**
	 * 조종하는 캐릭터가 바뀔 때 엔진이 호출한다. [서버 + 본인 컴퓨터]
	 * 서버: Possess / UnPossess 때, 클라이언트: 캐릭터 정보가 복제로 도착했을 때.
	 * HUD 경로 (가): 새 캐릭터가 준비됐는지 보고, 아직이면 준비 완료 알림을 구독한다.
	 * (엔진에서 public이므로 public 유지)
	 */
	virtual void SetPawn(APawn* InPawn) override;
	//~ End AController Interface

	//~ Begin APlayerController Interface
	/**
	 * 치트 매니저(ULOACheatManager)를 만든다. 엔진 기본은 "서버 게임 모드가 허락할 때만"이라 클라 PC에는 없다.
	 * 개발 빌드에서는 클라도 갖게 한다. (클라 창에서 LOABossPattern 등을 칠 수 있게. Lyra와 같은 방식) 출시 빌드는 엔진이 아예 안 만든다.
	 */
	virtual void AddCheats(bool bForce) override;
	//~ End APlayerController Interface

	// ---------- 치트 (개발 빌드) ----------
	/**
	 * [이 플레이어 컴퓨터 → 서버] 콘솔 명령을 서버에 있는 "이 PC"의 치트 매니저로 실행한다.
	 * ULOACheatManager가 서버 권한이 없을 때 부른다. 서버에서 이 PC로 실행되므로 "누가 쳤나"(Me)가 그대로 유지된다.
	 * 예) 클라: LOABossPattern Downward → ServerCheat("LOABossPattern Downward Me") → 서버의 ULOACheatManager::LOABossPattern
	 * 출시 빌드에서는 아무것도 안 한다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerCheat(const FString& Command);

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	//~ Begin APlayerController Interface
	/**
	 * 입력 컴포넌트가 준비되면 엔진이 호출한다. [본인 컴퓨터에서만]
	 * 리슨 서버 호스트 본인, 각 클라이언트 본인의 PlayerController에서만 불린다.
	 */
	virtual void SetupInputComponent() override;

	/**
	 * 서버가 "이 HUD를 써라"라고 알려 줄 때 호출된다. Super 안에서 HUD가 생성된다. [본인 컴퓨터]
	 * HUD 경로 (나): HUD가 생겼으니 초기화를 시도한다.
	 */
	virtual void ClientSetHUD_Implementation(TSubclassOf<AHUD> NewHUDClass) override;
	//~ End APlayerController Interface

private:
	// ---------- HUD ----------
	/**
	 * HUD 초기화 합류 지점. 아래가 전부 갖춰졌을 때만 HUD->InitOverlay를 부른다. [본인 컴퓨터]
	 *   - 이 컴퓨터의 PlayerController인가
	 *   - HUD가 ALOAHUD로 생성됐는가
	 *   - 조종 중인 캐릭터가 초기화를 끝냈는가 (= ASC 연결 완료)
	 * 몇 번 불러도 안전하다. (HUD가 이미 만들었으면 무시)
	 */
	void TryInitHUD();

	/**
	 * 기다리던 캐릭터가 준비됐다(캐릭터의 OnCharacterInitialized 알림). → 기다리기를 끝내고 TryInitHUD
	 * SetPawn 시점에 캐릭터가 아직 준비 전이었을 때만 불린다.
	 */
	UFUNCTION()
	void OnCharacterReadyForHUD(ALOACharacter* ReadyCharacter);

	/** 캐릭터 준비 기다리기를 그만둔다. (PendingCharacter의 알림 구독 해제) */
	void StopWaitingForCharacter();

	/**
	 * 준비되기를 기다리는 캐릭터. (OnCharacterInitialized를 구독 중)
	 * 기다리는 도중 캐릭터가 바뀌면(사망 등) 이전 캐릭터의 구독을 끊기 위해 기억한다. 기다릴 게 없으면 비어 있다.
	 */
	TWeakObjectPtr<ALOACharacter> PendingCharacter;

	// ---------- 스킬 입력 ----------
	/** 스킬 키를 누른 순간. 태그를 ASC로 넘긴다. */
	void Input_AbilityTagPressed(FGameplayTag InputTag);

	/** 스킬 키를 뗀 순간. 태그를 ASC로 넘긴다. (차지/홀딩 스킬용) */
	void Input_AbilityTagReleased(FGameplayTag InputTag);

	/**
	 * 지금 입력을 넘길 ASC. PlayerState에서 가져온다.
	 * 아직 캐릭터와 연결이 끝나지 않았으면(몸이 등록 전이면) nullptr → 입력을 무시한다.
	 */
	ULOAAbilitySystemComponent* GetReadyAbilitySystemComponent() const;

	// ---------- 컴포넌트 ----------
	/** 우클릭 이동 부품. 설정값(IMC, 도착 반경 등)은 BP_LOAPlayerController의 컴포넌트 디테일에서 조정 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULOAClickMoveComponent> ClickMoveComponent;

	/** 전투 RPC 컴포넌트. 서버 ↔ 이 플레이어 사이의 전투 RPC 통로 (데미지 숫자, 예고, 피해자 판정 보고) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|CombatRPC", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULOACombatRPCComponent> CombatRPCComponent;

	// ---------- 데이터 (BP 기본값에서 지정) ----------
	/** 입력 액션 ↔ InputTag 연결표 (프로젝트에 하나: DA_InputConfig_Default) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Input")
	TObjectPtr<const ULOAInputConfig> InputConfig;

	/** 스킬 키 설정 (예: IMC_Ability = Q/W/E/R/A/S/D/F, 스페이스, Z/X, 좌클릭) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Input")
	TObjectPtr<UInputMappingContext> AbilityIMC;

	/** 스킬 IMC 우선순위 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Input")
	int32 AbilityIMCPriority = 0;
};
