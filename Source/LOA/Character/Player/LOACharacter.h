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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	// ---------- Components ----------
	/** 탑쿼터뷰 카메라 암. 월드 기준 고정 각도(캐릭터 회전을 따라가지 않음). 거리/각도는 BP에서 설정 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LOA|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCamera;

	/** 직업 데이터로부터 런타임 생성된 컴포넌트 (각 컴퓨터 로컬 추적용) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> ClassComponents;

	// ---------- State ----------
	/** 서버가 결정해 복제하는 직업 데이터. 에셋 참조이므로 경로 기반으로 가볍게 복제된다 */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterClassData, VisibleInstanceOnly, Category = "LOA|Character")
	TObjectPtr<const ULOACharacterClassData> CharacterClassData;

	/** PlayerState의 ASC 캐시. 소유하지 않으므로 약한 참조 */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	/** 로컬 상태 플래그 (복제하지 않음: 컴퓨터마다 도착 순서가 다르다) */
	uint8 bAbilityActorInfoReady : 1;
	uint8 bCharacterInitialized : 1;
};