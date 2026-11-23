// LOACharacter.cpp

#include "Character/Player/LOACharacter.h"
#include "Core/LOALog.h"
#include "Data/LOACharacterClassData.h"
#include "Player/LOAPlayerState.h"
#include "Core/LOACollisionChannels.h"
#include "Character/Player/LOACharacterMovementComponent.h"
#include "Character/LOADeathReactionComponent.h"
#include "Combat/LOAJustGuardComponent.h"
#include "Combat/LOACrowdControlPredictionComponent.h"
#include "AbilitySystem/Abilities/LOAGA_Death.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Library/LOACombatLibrary.h"
#include "AbilitySystem/Effects/LOAGE_Kill.h"
#include "Core/LOAGameplayTags.h"
#include "Animation/LOAAnimInstance.h"
#include "Game/LOAGameState.h"
#include "Game/LOARaidSubsystem.h"

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
#include "Engine/World.h"
#include "TimerManager.h"

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

	// 레이드 벽 (LOACollisionChannels.h [RaidWall / RaidEdge]). 가장자리 벽은 CC 궤적 동안만 이동 컴포넌트가 무시로 바꾼다
	GetCapsuleComponent()->SetCollisionResponseToChannel(LOACollision::RaidWall, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(LOACollision::RaidEdge, ECR_Block);
	// 플레이어만 막는 벽 (LOACollisionChannels.h [PlayerWall], apply130). 이동 컴포넌트가 바꾸지 않는다 → CC·보스 몸 통과 중에도 막힌다
	GetCapsuleComponent()->SetCollisionResponseToChannel(LOACollision::PlayerWall, ECR_Block);

	// ---------- 사망 반응 ----------
	// State.Dead가 붙으면 충돌·이동을 끄고, 떨어지면 되돌린다. (보스와 같은 부품. ASC 준비 후 TryFinalizeInitialization에서 연결)
	DeathReactionComponent = CreateDefaultSubobject<ULOADeathReactionComponent>(TEXT("DeathReactionComponent"));

	// ---------- 저스트 가드 장부 ----------
	// 누른 기록은 GA(몸통)보다 오래 남아야 한다 (피해자 판정은 맞은 뒤 최대 1초 뒤에 검증). 그래서 몸에 붙인다. 모든 직업 공통
	JustGuardComponent = CreateDefaultSubobject<ULOAJustGuardComponent>(TEXT("JustGuardComponent"));

	// ---------- 예측 피격 반응 ----------
	// 피해자 판정 "맞음" → 서버 CC가 오기 전에 내 화면에서 먼저 멈춤·CC 몽타주. 모든 직업 공통
	CrowdControlPredictionComp = CreateDefaultSubobject<ULOACrowdControlPredictionComponent>(TEXT("CrowdControlPredictionComp"));
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

void ALOACharacter::BeginPlay()
{
	Super::BeginPlay();

	// 줌 배율의 기준 거리 (BP_LOAHero의 CameraBoom 값. 코드에 숫자 없음)
	DefaultCameraArmLength = CameraBoom->TargetArmLength;

	// [서버] 낙사했을 때 돌아올 자리. 처음엔 스폰 위치, 그 뒤로는 걷는 동안 0.25초마다 갱신
	if (HasAuthority())
	{
		LastGroundLocation = GetActorLocation();
		GetWorldTimerManager().SetTimer(LastGroundTimerHandle, this, &ThisClass::RecordLastGroundLocation, LastGroundRecordInterval, true);
	}
}

void ALOACharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LastGroundTimerHandle);

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

	// [모든 컴퓨터] State.Dead를 보기 시작한다. 이미 죽어 있으면(늦게 들어온 플레이어) 바로 시체 반응
	if (DeathReactionComponent)
	{
		// [서버] 죽으면 GameState에 알린다 (전멸 확인). 연결보다 먼저 걸어야 "이미 죽어 있음"도 받는다
		if (HasAuthority())
		{
			DeathReactionComponent->OnDied.AddUniqueDynamic(this, &ThisClass::HandleDiedOnServer);
		}
		DeathReactionComponent->InitializeWithAbilitySystem(CachedAbilitySystemComponent.Get());
	}

	// [모든 컴퓨터] AnimBP 태그 → 변수 연결 (State.Body.Dead → bIsDead). 외형 적용(AnimBP 교체) 뒤에 해야 새 AnimBP에 걸린다.
	if (ULOAAnimInstance* AnimInstance = GetMesh() ? Cast<ULOAAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
	{
		AnimInstance->InitializeWithAbilitySystem(CachedAbilitySystemComponent.Get());
	}
	else
	{
		// AnimBP 부모가 LOAAnimInstance가 아니면 시체 포즈가 동작하지 않는다. (충돌·이동은 사망 반응 부품이 처리하므로 동작)
		LOA_LOG(this, Warning, TEXT("[Character] AnimInstance is not a LOAAnimInstance. Tag-driven poses (dead) will not work."));
	}

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

	if (DeathReactionComponent)
	{
		DeathReactionComponent->OnDied.RemoveDynamic(this, &ThisClass::HandleDiedOnServer);
		DeathReactionComponent->UninitializeFromAbilitySystem();
	}

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

// =============================================================================
// 사망 / 부활
// =============================================================================

bool ALOACharacter::IsDead() const
{
	const UAbilitySystemComponent* ASC = CachedAbilitySystemComponent.Get();
	return ASC && ASC->HasMatchingGameplayTag(LOAGameplayTags::State_Body_Dead);
}

bool ALOACharacter::ReviveFromDeath(bool bGrantInvulnerability)
{
	UAbilitySystemComponent* ASC = CachedAbilitySystemComponent.Get();
	if (!HasAuthority() || !ASC || !IsDead())
	{
		return false;
	}

	// ① 아직 돌고 있는 GA_Death(사망 몽타주)를 끝낸다. 목록을 먼저 모으고 취소한다 (도는 중에 목록을 건드리지 않게)
	TArray<FGameplayAbilitySpecHandle> DeathHandles;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<ULOAGA_Death>())
		{
			DeathHandles.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : DeathHandles)
	{
		ASC->CancelAbilityHandle(Handle);
	}

	// ② 체력 가득. State.Dead가 붙어 있는 동안 채운다 → 살아난 순간 체력 0인 틈이 없다.
	//    값만 바꾸는 ApplyModToAttribute를 쓴다. (사망 알림 경로를 거칠 필요가 없음. 낙사는 GE로 해야 함 → ULOAGE_Kill)
	const float MaxHealth = ASC->GetNumericAttribute(ULOAAttributeSet_Common::GetMaxHealthAttribute());
	ASC->ApplyModToAttribute(ULOAAttributeSet_Common::GetHealthAttribute(), EGameplayModOp::Override, MaxHealth);

	// ③ 무적 N초 (State.Body.Invulnerable. 시간이 지나면 스스로 사라짐 → 모든 컴퓨터에 복제)
	if (bGrantInvulnerability && ReviveInvulnerableDuration > 0.f)
	{
		ULOACombatLibrary::ApplyStateTags(ASC, FGameplayTagContainer(LOAGameplayTags::State_Body_Invulnerable), ReviveInvulnerableDuration);
	}

	// ④ State.Dead를 준 GE를 지운다 → 태그가 모든 컴퓨터에서 사라짐
	//    → 각 컴퓨터의 ULOADeathReactionComponent가 충돌·이동을 되돌리고, AnimBP는 시체 포즈를 끝낸다.
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(LOAGameplayTags::State_Body_Dead));

	LOA_LOG(this, Log, TEXT("[Revive] Revived. Health %.0f, invulnerable %.1fs"), MaxHealth, bGrantInvulnerability ? ReviveInvulnerableDuration : 0.f);
	return true;
}

void ALOACharacter::RespawnAt(const FVector& Location)
{
	if (!HasAuthority())
	{
		return;
	}

	// ① 죽어 있으면 살린다 (전멸 뒤 부활은 무적 없음: 보스가 대기 상태라 공격이 없다)
	if (IsDead())
	{
		ReviveFromDeath(/*bGrantInvulnerability*/ false);
	}

	// ② 부활 자리로. 살아난 뒤라 이동이 켜져 있다. 멀리 순간이동이라 주인 클라에도 바로 알린다 (낙사 복귀 자리도 여기로)
	RelocateBody(Location);

	LOA_LOG(this, Log, TEXT("[Raid] Respawned at %s"), *Location.ToCompactString());
}

void ALOACharacter::RelocateBody(const FVector& Location)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);

	// 주인 컴퓨터도 같은 자리로. (시체는 이동이 꺼져 있어 이동 보정이 안 온다)
	ClientSnapToLocation(Location);

	// 낙사했을 때 돌아올 자리도 여기로
	LastGroundLocation = Location;
}

void ALOACharacter::SetCameraZoomScale(float Scale)
{
	if (DefaultCameraArmLength <= 0.f)
	{
		return;
	}

	// 예) 기준 1600, 배율 0.6 → 960. 같은 값이면 그대로
	const float NewLength = DefaultCameraArmLength * Scale;
	if (!FMath::IsNearlyEqual(CameraBoom->TargetArmLength, NewLength))
	{
		CameraBoom->TargetArmLength = NewLength;
	}
}

void ALOACharacter::HandleDiedOnServer(AActor* DeadActor)
{
	if (!HasAuthority())
	{
		return;
	}

	// 무너진 구역 위 허공에서 죽었으면 (떨어지는 중에 맞음 등) 시체는 이동이 꺼져 그 자리에 뜬 채로 남는다 → 가운데 원으로
	//   사용자 규칙: 시체는 언제나 부활할 수 있는 곳에 (2026-11-14)
	if (const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		FVector Recover;
		if (Raid->FindRecoverLocation(GetActorLocation(), Recover))
		{
			LOA_LOG(this, Log, TEXT("[Terrain] Died over a collapsed zone. Body moved to %s."), *Recover.ToCompactString());
			RelocateBody(Recover);
		}
	}

	if (ALOAGameState* LOAGameState = GetWorld() ? GetWorld()->GetGameState<ALOAGameState>() : nullptr)
	{
		LOAGameState->NotifyPlayerDied(this);
	}
}

// =============================================================================
// 낙사
// =============================================================================

void ALOACharacter::RecordLastGroundLocation()
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const UWorld* World = GetWorld();
	if (!Movement || !World || !Movement->IsMovingOnGround() || IsDead())
	{
		return;
	}

	// 몸 중심 바로 아래에 걸을 수 있는 바닥이 있을 때만 저장한다.
	// 예) 낭떠러지 끝에 캡슐이 반쯤 걸쳐 서 있음 → 중심 아래가 허공 → 저장 안 함 (이전 위치 유지)
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 50.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LOALastGround), false, this);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, GetCapsuleComponent()->GetCollisionObjectType(), Params)
		&& Movement->IsWalkable(Hit))
	{
		LastGroundLocation = Start;
	}
}

void ALOACharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super를 부르지 않는다. (엔진 기본 = 숨기고 액터 삭제 → 플레이어가 사라짐)

	// 클라(내 캐릭터의 이동 예측 중에 불림): 아무것도 안 한다. 서버가 옮겨 주고 ClientSnapToLocation으로 알려 준다.
	// 서버가 처리하기 전까지 매 프레임 다시 불릴 수 있다 → 로그도 남기지 않는다.
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	// ① 마지막으로 밟은 땅. 그 땅이 무너진 구역 안이면 가운데 원 안쪽 (2026-11-14)
	//    예) 가운데 원 반지름 800, 여유 100. 왼쪽 구역 예고를 못 피함 → 마지막으로 밟은 땅 = 중심에서 서쪽 1200 → 서쪽 700
	FVector Destination = LastGroundLocation;
	if (const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->FindRecoverLocation(LastGroundLocation, Destination);
	}

	LOA_LOG(this, Log, TEXT("[Fall] Fell out of world at %s. Moving to %s (last ground %s) and killing."),
		*GetActorLocation().ToCompactString(), *Destination.ToCompactString(), *LastGroundLocation.ToCompactString());

	// ② 그 자리로 옮긴다 (떨어지던 속도는 버림). 주인 컴퓨터도 같은 자리로 (곧 이동이 꺼지므로 이동 보정을 기다리지 않는다)
	RelocateBody(Destination);

	// ③ 체력 0 → 보통 사망 경로 (Event.Trigger.Death → GA_Death). 데미지가 아니라서 데미지 숫자는 없다. (막타 = 자기 자신)
	//    GE(ULOAGE_Kill)로 해야 스탯창의 PostGameplayEffectExecute를 거쳐 Event.Death가 나간다.
	//    ApplyModToAttribute(Unsafe)는 값만 바꿔서 체력 0인데 안 죽는다 (2026-10-23 확인. ULOAGE_Kill 헤더)
	if (UAbilitySystemComponent* ASC = CachedAbilitySystemComponent.Get())
	{
		ASC->ApplyGameplayEffectToSelf(GetDefault<ULOAGE_Kill>(), 1.f, ASC->MakeEffectContext());
	}
}

void ALOACharacter::ClientSnapToLocation_Implementation(FVector_NetQuantize Location)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);

	// 이동 예측 기록은 지우지 않는다. (ResetPredictionData_Client를 부르면 클라 이동 시계가 0부터 다시 시작해서
	//  서버가 "옛날 시각의 이동"으로 보고 한동안 전부 버린다 → ServerMove: TimeStamp expired 경고가 쏟아짐. 2026-10-23 확인)
	// 남은 기록이 서버와 달라도 서버의 보통 위치 보정으로 맞춰진다.
}