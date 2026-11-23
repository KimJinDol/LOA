// LOACharacter.h
// [배관] 플레이어 캐릭터 공통 베이스 (몸체 전용)
//
// [설계 원칙]
// - 이 클래스는 "몸체"다. 스탯/스킬 로직을 소유하지 않으며, 직업(버서커/바드 등)을 알지 못한다.
// - 직업 차이는 전부 ULOACharacterClassData(PrimaryDataAsset)로 런타임 주입된다.
// - ASC는 PlayerState가 소유하고, 이 캐릭터는 몸(Avatar)으로만 등록된다.
// - 새 직업 추가 = 데이터 에셋 추가. 이 파일은 수정하지 않는다.
//
// [입력을 모른다]
// - 이 클래스에는 키 입력 처리 코드가 없다. 모든 입력은 PlayerController가 담당한다.
//   · 이동: PlayerController의 "조작 방식 부품"이 AddMovementInput()으로 이동을 요청한다.
//     지금은 WASD 부품, 나중에 클릭 이동 부품으로 교체되어도 이 클래스는 수정하지 않는다.
//     (클릭 이동도 "경로를 따라가며 AddMovementInput"으로 변환하므로 이동 예측/동기화가 동일하게 동작)
//   · 스킬: PlayerController가 입력 태그를 ASC로 직접 전달한다.
//
// [카메라]
// - 탑쿼터뷰: 카메라는 월드 기준 고정 각도로 캐릭터를 따라간다. 캐릭터가 회전해도 카메라는 돌지 않는다.
// - 거리, 각도 등 수치는 BP 기본값에서 조정한다. (코드에 숫자를 쓰지 않음)
//
// [초기화 타이밍]
// 캐릭터가 "완전히 준비됨"이 되려면 아래 두 조건이 모두 충족되어야 한다. 도착 순서는 보장되지 않는다.
//   (A) ASC 연결 완료       - 서버: PossessedBy / 클라이언트: OnRep_PlayerState
//   (B) 직업 데이터 수신 완료 - 서버: SetCharacterClassData / 클라이언트: OnRep_CharacterClassData
// 두 경로 모두 TryFinalizeInitialization()으로 모이며, 마지막에 도착한 쪽이 초기화를 완료시킨다.
//
// [사망 / 부활 / 낙사] (2026-10-23)
//   사망 : 체력 0 → Event.Trigger.Death → GA_Death(State.Body.Dead) → 이 캐릭터의 ULOADeathReactionComponent가 충돌·이동을 끔 [모든 컴퓨터]
//   부활 : ReviveFromDeath() [서버]. 죽은 자리에서 일어난다.
//          ① 돌고 있는 GA_Death 취소 ② 체력 가득 ③ State.Body.Invulnerable N초 ④ State.Body.Dead 제거 → 몸이 되돌아옴
//          목숨 확인은 부르는 쪽(ALOAPlayerController::ServerRequestRevive)이 한다.
//   낙사 : 엔진이 KillZ 아래로 떨어진 액터에 FellOutOfWorld를 부른다. 엔진 기본은 액터를 지운다 → 덮어쓴다.
//          서버: 마지막으로 밟은 땅으로 옮김 → 체력 0 (보통 사망 경로) → 부활하면 그 자리에서 일어남
//          클라: 아무것도 안 함 (엔진도 클라에서는 안 지움). 서버가 ClientSnapToLocation으로 같은 자리로 옮겨 준다.
//   전멸 : 서버에서 죽으면 ALOAGameState::NotifyPlayerDied → 전멸 확인. 전멸 3초 뒤 RespawnAt(부활 자리) [서버]
//   "마지막으로 밟은 땅" : 서버가 0.25초마다, 땅을 걷는 중이고 몸 중심 바로 아래에 바닥이 있으면 저장
//          (낭떠러지 끝에 걸쳐 선 위치는 저장하지 않음 → 옮겨도 다시 떨어지지 않게)
//   무너진 지형 (2026-11-14, ALOATerrainZone) : 사용자 규칙 "시체는 언제나 부활할 수 있는 곳에"
//          마지막으로 밟은 땅이 무너진 구역 안이면 → 가운데 원(안 무너짐) 안쪽으로 (ULOARaidSubsystem::FindRecoverLocation)
//          예) 왼쪽 반쪽 고리 위에서 예고를 못 피함 → 떨어짐 → KillZ → 마지막으로 밟은 땅 = 구역 안 → 가운데 원 가장자리 안쪽에서 죽음
//          무너진 구역 위 허공에서 죽어도 같은 자리로 (HandleDiedOnServer). 이미 시체인 채로 구역이 무너지면 상황판이 옮긴다
//
// [BP에서 설정할 것] BP_LOAHero
//   LOA|Revive : Revive Invulnerable Duration (기본 3초)
//   레벨의 World Settings → Kill Z : 떨어지면 죽을 높이 (바닥보다 충분히 아래)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "LOACharacter.generated.h"

class UAbilitySystemComponent;
class ULOACharacterClassData;
class USpringArmComponent;
class UCameraComponent;
class UActorComponent;
class ULOADeathReactionComponent;
class ULOAJustGuardComponent;
class ULOACrowdControlPredictionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLOACharacterInitializedSignature, ALOACharacter*, InitializedCharacter);

UCLASS(Abstract)
class LOA_API ALOACharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALOACharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	//~ Begin AActor Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** KillZ 아래로 떨어짐. 엔진 기본(액터 삭제) 대신 낙사 처리 (헤더 [사망 / 부활 / 낙사]) */
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	virtual void PossessedBy(AController* NewController) override;   // 서버 (리슨 서버 호스트 포함)
	virtual void UnPossessed() override;                              // 서버
	virtual void OnRep_PlayerState() override;                        // 클라이언트
	//~ End APawn Interface

	/**
	 * 직업 데이터 주입 (서버 전용).
	 * GameMode 등 외부에서 호출한다. 캐릭터 스스로 직업을 결정하지 않는다.
	 * SpawnActorDeferred 이후 FinishSpawning 이전에 호출하면 첫 복제 패킷에 포함된다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LOA|Character")
	void SetCharacterClassData(const ULOACharacterClassData* NewClassData);

	UFUNCTION(BlueprintPure, Category = "LOA|Character")
	const ULOACharacterClassData* GetCharacterClassData() const { return CharacterClassData; }

	/** (A)와 (B)가 모두 끝나 사용 가능한 상태인지. PlayerController는 이 값이 true일 때만 입력을 전달한다. */
	UFUNCTION(BlueprintPure, Category = "LOA|Character")
	bool IsCharacterInitialized() const { return bCharacterInitialized; }

	/**
	 * 초기화 완료 알림. 외부 시스템(PlayerController, UI, 직업 전용 컴포넌트 등)은
	 * 캐릭터 코드를 수정하지 않고 이 델리게이트를 구독해 확장한다.
	 * 서버/클라이언트 각 컴퓨터에서 로컬로 1회 브로드캐스트된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "LOA|Character")
	FLOACharacterInitializedSignature OnCharacterInitialized;

	// ---------- 사망 / 부활 ----------
	/** 죽어 있나 (State.Body.Dead). 모든 컴퓨터 */
	UFUNCTION(BlueprintPure, Category = "LOA|Revive")
	bool IsDead() const;

	/**
	 * [서버] 죽어 있으면 살린다. 죽은 자리에서 체력 가득 + 무적 N초. 살렸으면 true.
	 * 공용 목숨 확인·차감은 부르는 쪽이 한다. (ALOAPlayerController::ServerRequestRevive)
	 */
	bool ReviveFromDeath(bool bGrantInvulnerability = true);

	/**
	 * [서버] 전멸 뒤 부활: 죽어 있으면 살리고(무적 없음), 이 자리로 옮긴다. 살아 있어도 옮긴다.
	 * 주인 클라도 같은 자리로 바로 옮긴다 (ClientSnapToLocation).
	 */
	void RespawnAt(const FVector& Location);

	/**
	 * [서버] 살리지 않고 몸만 이 자리로 옮긴다 (시체도 옮겨짐). 주인 클라도 바로 옮기고, 낙사 복귀 자리도 여기로.
	 * 쓰는 곳: 낙사, 전멸 뒤 부활(RespawnAt), 무너진 구역 안 시체 (ULOARaidSubsystem), 대난투 차원 이동 (ALOABrawlArena)
	 */
	void RelocateBody(const FVector& Location);

	/**
	 * [본인 컴퓨터] 카메라 거리 배율 (2026-11-18 apply122). 1 = BP에서 정한 거리, 0.6 = 60% 거리 (줌인)
	 * 부르는 곳: ALOAPlayerController가 매 프레임 (대난투 연출). 기준 거리는 BeginPlay 때 CameraBoom의 TargetArmLength
	 */
	void SetCameraZoomScale(float Scale);

	UFUNCTION(BlueprintPure, Category = "LOA|Character")
	ULOADeathReactionComponent* GetDeathReactionComponent() const { return DeathReactionComponent; }

protected:
	/** 파생 클래스 확장 지점. 오버라이드 시 부모 호출 필수 */
	virtual void OnCharacterInitializedInternal();

	/** 블루프린트 확장 지점 (연출 용도 권장) */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|Character", meta = (DisplayName = "On Character Initialized"))
	void K2_OnCharacterInitialized();

private:
	// ---------- Replication ----------
	UFUNCTION()
	void OnRep_CharacterClassData();

	// ---------- Initialization ----------
	/** (A) PlayerState의 ASC에 이 캐릭터를 몸(Avatar)으로 등록. 서버/클라이언트 공용 */
	void InitAbilityActorInfo();

	/** (A), (B) 도착 경로의 합류 지점. 둘 다 준비됐을 때만 실제 초기화를 수행 */
	void TryFinalizeInitialization();

	/**
	 * [서버 전용] 직업 데이터의 스킬/이펙트/스탯창 부여를 요청한다.
	 * ASC가 PlayerState 소유이므로, 부활 시 중복 부여 방지도 부여하는 쪽이 판단한다.
	 */
	void RequestGrantClassGameplayData();

	/** [모든 컴퓨터] 메시, 애님 등 외형 적용. 데디케이티드 서버도 판정용 본 위치가 필요하므로 실행한다 */
	void ApplyClassAvatarData();

	/** [모든 컴퓨터] 데이터에 정의된 직업 전용 컴포넌트 생성/제거 */
	void CreateClassComponents();
	void DestroyClassComponents();

	/** 초기화 상태 및 ASC 연결 해제 (UnPossessed / EndPlay) */
	void UninitializeCharacter();

	// ---------- 낙사 ----------
	/** [서버] 땅을 걷는 중이고 몸 중심 아래에 바닥이 있으면 지금 위치를 "마지막으로 밟은 땅"으로 저장 */
	void RecordLastGroundLocation();

	/**
	 * [서버 → 이 캐릭터 주인 컴퓨터] 서버가 옮긴 자리로 같이 옮긴다. (주인 컴퓨터는 이동 보정을 기다리지 않음)
	 * 낙사(곧 이동이 꺼져 보정이 안 옴), 전멸 뒤 부활(멀리 순간이동)에서 쓴다.
	 */
	UFUNCTION(Client, Reliable)
	void ClientSnapToLocation(FVector_NetQuantize Location);

	/** [서버] 사망 반응 부품이 "죽었다"고 알릴 때 → GameState에 알림 (전멸 확인). AddDynamic이라 UFUNCTION */
	UFUNCTION()
	void HandleDiedOnServer(AActor* DeadActor);

	// ---------- Components ----------
	/** 탑쿼터뷰 카메라 암. 월드 기준 고정 각도(캐릭터 회전을 따라가지 않음). 거리/각도는 BP에서 설정 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCamera;

	/** BP에서 정한 카메라 거리 (BeginPlay 때 기억). 줌 배율의 기준 */
	float DefaultCameraArmLength = 0.f;

	/** State.Body.Dead → 충돌·이동 끄기/되돌리기 [모든 컴퓨터]. 보스와 같은 부품 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Death", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULOADeathReactionComponent> DeathReactionComponent;

	/** 저스트 가드 장부 (누른 시각·뗀 시각). 조종하는 사람 = 원본, 서버 = 사본. 판정 담당이 맞는 순간 본다 (2026-10-26) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULOAJustGuardComponent> JustGuardComponent;

	/** 예측 피격 반응: 피해자 판정 "맞음"이면 서버 CC 전에 먼저 멈춤·CC 몽타주 [조종하는 사람 컴퓨터] (2026-10-31) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULOACrowdControlPredictionComponent> CrowdControlPredictionComp;

	/** 직업 데이터로부터 런타임 생성된 컴포넌트 (각 컴퓨터 로컬 추적용) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> ClassComponents;

	// ---------- State ----------
	/** 서버가 결정해 복제하는 직업 데이터. 에셋 참조이므로 경로 기반으로 가볍게 복제된다 */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterClassData, VisibleInstanceOnly, Category = "LOA|Character")
	TObjectPtr<const ULOACharacterClassData> CharacterClassData;

	// ---------- 부활 / 낙사 설정 ----------
	/** 부활 직후 무적 시간 (초). 0이면 무적 없음 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Revive", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ReviveInvulnerableDuration = 3.f;

	/** "마지막으로 밟은 땅"을 저장하는 간격 (초) [서버] */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Revive", meta = (ClampMin = "0.05", Units = "Seconds"))
	float LastGroundRecordInterval = 0.25f;

	/** [서버] 마지막으로 밟은 땅. 처음엔 스폰 위치 */
	FVector LastGroundLocation = FVector::ZeroVector;

	FTimerHandle LastGroundTimerHandle;

	/** PlayerState의 ASC 캐시. 소유하지 않으므로 약한 참조 */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	/** 로컬 상태 플래그 (복제하지 않음: 컴퓨터마다 도착 순서가 다르다) */
	uint8 bAbilityActorInfoReady : 1;
	uint8 bCharacterInitialized : 1;
};