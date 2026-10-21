// LOACharacter.cpp

#include "Character/Player/LOACharacter.h"
#include "Core/LOALog.h"
#include "Data/LOACharacterClassData.h"
#include "Player/LOAPlayerState.h"
#include "Core/LOACollisionChannels.h"
#include "Character/Player/LOACharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACharacter)

// =============================================================================
// 생성자
// =============================================================================

ALOACharacter::ALOACharacter(const FObjectInitializer& ObjectInitializer)
	// 이동 컴포넌트를 LOA 것으로 바꿔 만든다. (보스 몸 막이에 겹쳤을 때 빠져나오기. 이름은 엔진 기본 그대로라 BP 설정값 유지)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<ULOACharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
	, bAbilityActorInfoReady(false)
	, bCharacterInitialized(false)
{
	// 캐릭터 자체는 매 프레임 할 일이 없다. (이동은 CharacterMovementComponent가 알아서 Tick)
	PrimaryActorTick.bCanEverTick = false;

	// ---------- 회전 규칙 (탑쿼터뷰) ----------
	// 카메라(컨트롤러) 방향으로 캐릭터를 돌리지 않는다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 캐릭터는 "이동하는 방향"을 바라본다.
	// WASD든 나중의 클릭 이동이든 AddMovementInput으로 들어오므로 둘 다 똑같이 동작한다.
	// 회전 속도 같은 수치는 BP 기본값에서 조정한다.
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->bConstrainToPlane = true;       // 탑쿼터뷰: 바닥 평면 위에서만 이동
	MoveComp->bSnapToPlaneAtStart = true;

	// ---------- 탑쿼터뷰 카메라 ----------
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);

	// 월드 기준 고정 각도: 캐릭터가 회전해도 카메라는 돌지 않는다.
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bUsePawnControlRotation = false;

	// 탑쿼터뷰에서는 벽 뒤로 가도 카메라를 당기지 않는다. (당기면 화면이 튐. 가림 처리는 나중에 반투명으로)
	CameraBoom->bDoCollisionTest = false;

	// 거리(TargetArmLength), 각도(Rotation)는 BP_LOAHero의 CameraBoom에서 설정한다. (코드에 숫자 없음)

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	// ---------- 몸 충돌 (로아 규칙) ----------
	// 플레이어끼리 통과: Pawn 무시. (보스 캡슐도 Pawn이라 통과 → 보스를 막는 건 몸 막이)
	// 보스·허수아비의 몸 막이(BodyBlock)에는 막힌다.
	// 겹쳤을 때 빠져나오기와 관통 스킬(그 순간만 무시)은 ULOACharacterMovementComponent가 이 응답을 바꾼다. (한 곳에서만)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(LOACollision::BodyBlock, ECR_Block);
}

// =============================================================================
// IAbilitySystemInterface
// =============================================================================

UAbilitySystemComponent* ALOACharacter::GetAbilitySystemComponent() const
{
	// 캐릭터는 ASC를 소유하지 않는다. PlayerState의 ASC를 빌려서 돌려줄 뿐이다.
	// 아직 연결 전이면 nullptr.
	return CachedAbilitySystemComponent.Get();
}

// =============================================================================
// Replication
// =============================================================================

void ALOACharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 직업 데이터는 모든 컴퓨터가 알아야 한다. (파티원 외형도 내 화면에 그려야 하므로)
	// 에셋 자체가 아니라 "에셋 경로"만 전송되므로 가볍다.
	DOREPLIFETIME(ALOACharacter, CharacterClassData);
}

void ALOACharacter::OnRep_CharacterClassData()
{
	// [클라이언트] 택배 B 도착 (직업 데이터)
	// 서버에서는 이 함수가 호출되지 않는다 → 서버는 SetCharacterClassData에서 직접 합류 지점을 부른다.
	TryFinalizeInitialization();
}

// =============================================================================
// 빙의 / PlayerState (택배 A 경로)
// =============================================================================

void ALOACharacter::PossessedBy(AController* NewController)
{
	// [서버] 컨트롤러가 이 캐릭터를 조종하기 시작함. 리슨 서버 호스트 본인 캐릭터도 여기로 온다.
	// Super 안에서 PlayerState가 연결되므로 Super를 먼저 호출한다.
	Super::PossessedBy(NewController);

	// 택배 A: ASC 연결
	InitAbilityActorInfo();
	TryFinalizeInitialization();
}

void ALOACharacter::OnRep_PlayerState()
{
	// [클라이언트] PlayerState가 네트워크로 도착함. (내 캐릭터 + 파티원 캐릭터 모두)
	Super::OnRep_PlayerState();

	// 택배 A: ASC 연결
	InitAbilityActorInfo();
	TryFinalizeInitialization();
}

void ALOACharacter::UnPossessed()
{
	// [서버] 조종이 끝남 (사망 후 교체 등). Super가 PlayerState 연결을 끊기 전에 먼저 정리한다.
	UninitializeCharacter();

	Super::UnPossessed();
}

void ALOACharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// [모든 컴퓨터] 캐릭터가 사라짐. 클라이언트는 UnPossessed가 없으므로 여기서 정리된다.
	UninitializeCharacter();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// 직업 데이터 (택배 B 경로)
// =============================================================================

void ALOACharacter::SetCharacterClassData(const ULOACharacterClassData* NewClassData)
{
	// BlueprintAuthorityOnly로 BP에서는 막혀 있지만, C++ 호출도 한 번 더 막는다.
	if (!HasAuthority())
	{
		LOA_LOG(this, Warning, TEXT("SetCharacterClassData called on client. Ignored."));
		return;
	}

	if (!NewClassData || CharacterClassData == NewClassData)
	{
		return;
	}

	// 현재 설계: 캐릭터 하나의 직업은 한 번만 정해진다. (직업 변경 = 새 캐릭터 스폰)
	if (CharacterClassData)
	{
		LOA_LOG(this, Warning, TEXT("Class already set to [%s]. Changing class on the same character is not supported."),
			*GetNameSafe(CharacterClassData));
		return;
	}

	CharacterClassData = NewClassData;

	// 바뀐 값을 가능한 빨리 클라이언트로 보내도록 요청
	ForceNetUpdate();

	// 택배 B: 서버는 OnRep이 오지 않으므로 직접 합류 지점을 부른다.
	TryFinalizeInitialization();
}

// =============================================================================
// 초기화
// =============================================================================

void ALOACharacter::InitAbilityActorInfo()
{
	// PlayerState에서 ASC를 찾는다.
	// 구체 타입(ALOAPlayerState)이 아니라 인터페이스로 찾아서 PlayerState 클래스에 덜 묶이게 한다.
	IAbilitySystemInterface* ASCOwner = Cast<IAbilitySystemInterface>(GetPlayerState());
	UAbilitySystemComponent* ASC = ASCOwner ? ASCOwner->GetAbilitySystemComponent() : nullptr;

	if (!ASC)
	{
		// 클라이언트에서 PlayerState가 아직 안 온 경우 등. 나중에 OnRep_PlayerState로 다시 들어온다.
		return;
	}

	// ASC에 "주인(Owner) = PlayerState, 몸(Avatar) = 이 캐릭터"라고 등록
	// 이후 Ability는 "누가 시전했나(Owner)"와 "어디서 모션/판정을 하나(Avatar)"를 여기서 얻는다.
	ASC->InitAbilityActorInfo(GetPlayerState(), this);

	CachedAbilitySystemComponent = ASC;
	bAbilityActorInfoReady = true;
}

void ALOACharacter::TryFinalizeInitialization()
{
	// 이미 끝났으면 다시 하지 않는다. (택배 A, B가 각각 이 함수를 부르므로 두 번째 호출은 여기서 걸러짐)
	if (bCharacterInitialized)
	{
		return;
	}

	// 택배 A, B 중 하나라도 아직이면 기다린다. (늦게 오는 쪽이 다시 이 함수를 부른다)
	if (!bAbilityActorInfoReady || !CharacterClassData)
	{
		return;
	}

	bCharacterInitialized = true;

	// [서버] Ability / Effect / AttributeSet 부여 요청
	if (HasAuthority())
	{
		RequestGrantClassGameplayData();
	}

	// [모든 컴퓨터] 외형, 직업 전용 컴포넌트
	ApplyClassAvatarData();
	CreateClassComponents();

	LOA_LOG(this, Log, TEXT("Character initialized. Class = [%s]"), *GetNameSafe(CharacterClassData));

	OnCharacterInitializedInternal();
}

void ALOACharacter::OnCharacterInitializedInternal()
{
	// 확장 지점: 자식 C++ → BP 이벤트 → 외부 구독자 순서로 알린다.
	K2_OnCharacterInitialized();
	OnCharacterInitialized.Broadcast(this);
}

void ALOACharacter::RequestGrantClassGameplayData()
{
	// 부여 자체와 "부활 시 중복 방지"는 PlayerState(ASC 주인)가 판단한다. 캐릭터는 요청만 한다.
	ALOAPlayerState* LOAPlayerState = GetPlayerState<ALOAPlayerState>();
	if (!LOAPlayerState)
	{
		LOA_LOG(this, Error, TEXT("RequestGrantClassGameplayData: PlayerState is not ALOAPlayerState."));
		return;
	}

	LOAPlayerState->GrantClassAbilityDataSets(CharacterClassData);
}

void ALOACharacter::ApplyClassAvatarData()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !CharacterClassData)
	{
		return;
	}

	// 지금은 필요한 순간에 바로 로드(동기 로드)한다. 에셋이 적은 지금 단계에서는 충분하다.
	// 나중에 로딩 멈춤이 문제가 되면, 직업 데이터의 AssetBundles("Avatar")로 스폰 전에 미리 비동기 로드하도록 바꾼다.
	if (USkeletalMesh* LoadedMesh = CharacterClassData->SkeletalMesh.LoadSynchronous())
	{
		MeshComp->SetSkeletalMeshAsset(LoadedMesh);
	}

	if (UClass* LoadedAnimClass = CharacterClassData->AnimClass.LoadSynchronous())
	{
		MeshComp->SetAnimInstanceClass(LoadedAnimClass);
	}

	// 메시 원점/방향 보정값도 데이터에서 가져온다. (코드에 숫자 없음)
	MeshComp->SetRelativeTransform(CharacterClassData->MeshRelativeTransform);
}

// =============================================================================
// 직업 전용 컴포넌트
// =============================================================================

namespace
{
	/** 이 캐릭터를 조종하는 사람이 "이 컴퓨터"에 있는지 */
	bool IsOwnedByLocalPlayer(const APawn* Pawn)
	{
		// PlayerState의 Owner는 PlayerController다. PlayerController는 서버와 "그 플레이어 본인 컴퓨터"에만 존재한다.
		// Pawn의 Controller보다 PlayerState 쪽이 초기화 시점에 더 확실하게 준비되어 있어서 이쪽으로 판단한다.
		const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
		const APlayerController* PC = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
		return PC && PC->IsLocalController();
	}
}

void ALOACharacter::CreateClassComponents()
{
	if (!CharacterClassData)
	{
		return;
	}

	// 주의: 여기서 만드는 컴포넌트는 "각 컴퓨터가 자기 것을 따로 만드는" 로컬 컴포넌트다.
	//       컴포넌트 BP에서 Replicates를 켜면 서버 것이 복제되어 두 개가 생길 수 있으므로 끈 채로 사용한다.
	for (const FLOAClassComponentEntry& Entry : CharacterClassData->ClassComponents)
	{
		bool bShouldCreate = false;
		switch (Entry.NetPolicy)
		{
		case ELOAClassComponentNetPolicy::AllMachines:      bShouldCreate = true;                        break;
		case ELOAClassComponentNetPolicy::ServerOnly:       bShouldCreate = HasAuthority();              break;
		case ELOAClassComponentNetPolicy::OwningClientOnly: bShouldCreate = IsOwnedByLocalPlayer(this);  break;
		}

		if (!bShouldCreate)
		{
			continue;
		}

		UClass* ComponentClass = Entry.ComponentClass.LoadSynchronous();
		if (!ComponentClass)
		{
			continue;
		}

		UActorComponent* NewComponent = NewObject<UActorComponent>(this, ComponentClass);
		AddInstanceComponent(NewComponent);   // 에디터 디테일 패널/디버깅에서 보이도록
		NewComponent->RegisterComponent();    // 월드에 등록 → BeginPlay, Tick 등이 동작하기 시작

		ClassComponents.Add(NewComponent);
	}
}

void ALOACharacter::DestroyClassComponents()
{
	for (UActorComponent* Component : ClassComponents)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	ClassComponents.Reset();
}

// =============================================================================
// 정리
// =============================================================================

void ALOACharacter::UninitializeCharacter()
{
	DestroyClassComponents();

	UAbilitySystemComponent* ASC = CachedAbilitySystemComponent.Get();

	// ASC의 몸이 아직 "나"일 때만 정리한다.
	// (이미 새 캐릭터가 몸으로 등록됐다면 그 캐릭터의 연결을 끊으면 안 됨)
	if (ASC && ASC->GetAvatarActor() == this)
	{
		// 이 몸으로 실행 중이던 Ability 취소, 이 몸에 붙은 연출(Cue) 제거
		ASC->CancelAbilities();
		ASC->RemoveAllGameplayCues();

		// 주인(PlayerState)은 살아 있으므로 몸만 비운다. Spec, AttributeSet, Effect는 PlayerState 쪽에 그대로 남는다.
		if (ASC->GetOwnerActor())
		{
			ASC->SetAvatarActor(nullptr);
		}
		else
		{
			ASC->ClearActorInfo();
		}
	}

	CachedAbilitySystemComponent.Reset();
	bAbilityActorInfoReady = false;
	bCharacterInitialized = false;
}