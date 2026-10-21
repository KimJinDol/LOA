// LOABossCharacter.cpp

#include "Character/Monster/Boss/LOABossCharacter.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "Data/LOAAbilityDataSet.h"
#include "Game/LOARaidSubsystem.h"
#include "Character/LOADeathReactionComponent.h"
#include "Character/LOABodyBlockerComponent.h"
#include "Animation/LOAAnimInstance.h"
#include "Core/LOALog.h"
#include "Core/LOAGameplayTags.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOABossCharacter)

ALOABossCharacter::ALOABossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 매 프레임 할 일이 없다. (이동은 CharacterMovement가 자기 Tick으로 한다)
	PrimaryActorTick.bCanEverTick = false;

	// ---------- 회전 ----------
	// 컨트롤러 방향을 따라 돌지 않는다. 걸을 때는 가는 방향으로, 패턴 중에는 GA가 정한 방향으로 돈다. (플레이어와 같은 규칙)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// ---------- ASC ----------
	AbilitySystemComponent = CreateDefaultSubobject<ULOAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Minimal: 버프 상세 정보를 아무에게도 보내지 않는다. (속성 값은 AttributeSet 복제로, 태그·Cue는 Minimal에서도 전달됨)
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// ---------- 스탯창 ----------
	CommonAttributeSet = CreateDefaultSubobject<ULOAAttributeSet_Common>(TEXT("CommonAttributeSet"));
	CombatAttributeSet = CreateDefaultSubobject<ULOAAttributeSet_Combat>(TEXT("CombatAttributeSet"));

	// ---------- 사망 반응 ----------
	DeathReactionComponent = CreateDefaultSubobject<ULOADeathReactionComponent>(TEXT("DeathReactionComponent"));

	// ---------- 몸 충돌 (로아 규칙: 보스는 플레이어를 뚫고 지나가고, 플레이어는 보스에 막힌다) ----------
	// 캡슐(이동용)은 Pawn을 무시 → 추적·점프·돌진 중에 플레이어에 막히지 않는다.
	// 플레이어를 막는 일은 몸 막이 캡슐이 따로 한다. (LOABodyBlockerComponent.h)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	BodyBlockerComponent = CreateDefaultSubobject<ULOABodyBlockerComponent>(TEXT("BodyBlockerComponent"));
	BodyBlockerComponent->SetupAttachment(GetCapsuleComponent());
}

UAbilitySystemComponent* ALOABossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ALOABossCharacter::BeginPlay()
{
	Super::BeginPlay();

	// ① Owner도 Avatar도 자기 자신. 서버/클라이언트 각자 호출해야 각 컴퓨터의 ASC가 몸을 안다.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// ①-1 [모든 컴퓨터] ASC가 준비됐으니 State.Dead를 보는 쪽들을 연결한다.
	//      늦게 들어온 플레이어: 이미 죽은 보스면 여기서 바로 시체 포즈 + 충돌 꺼짐
	DeathReactionComponent->InitializeWithAbilitySystem(AbilitySystemComponent);

	// ①-2 [모든 컴퓨터] State.Hidden → 숨기기 + 몸 막이 끄기. 이미 붙어 있으면(늦게 들어온 플레이어) 바로 반영
	HiddenTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(LOAGameplayTags::State_Hidden, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnHiddenTagChanged);
	OnHiddenTagChanged(LOAGameplayTags::State_Hidden, AbilitySystemComponent->GetTagCount(LOAGameplayTags::State_Hidden));

	// ①-3 [모든 컴퓨터] State.Counterable → BP에 "파란 빛 켜/꺼" 알림. 이미 열려 있으면(늦게 들어온 플레이어) 바로 켬
	CounterableTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(LOAGameplayTags::State_Counterable, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnCounterableTagChanged);
	if (AbilitySystemComponent->GetTagCount(LOAGameplayTags::State_Counterable) > 0)
	{
		OnCounterableTagChanged(LOAGameplayTags::State_Counterable, AbilitySystemComponent->GetTagCount(LOAGameplayTags::State_Counterable));
	}

	if (ULOAAnimInstance* AnimInstance = Cast<ULOAAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->InitializeWithAbilitySystem(AbilitySystemComponent);
	}
	else
	{
		// AnimBP 부모가 LOAAnimInstance가 아니면 시체 포즈가 동작하지 않는다. (충돌·이동은 부품이 처리하므로 동작)
		LOA_LOG(this, Warning, TEXT("[Boss] AnimInstance is not a LOAAnimInstance. Tag-driven poses (dead) will not work."));
	}

	// ② [서버] 초기화 GE 등 부여 → 체력·공격력이 채워지고 복제된다.
	if (HasAuthority())
	{
		if (AbilityDataSets.IsEmpty())
		{
			LOA_LOG(this, Warning, TEXT("[Boss] AbilityDataSets is empty. Health will stay 0."));
		}

		for (const ULOAMonsterAbilityDataSet* DataSet : AbilityDataSets)
		{
			if (DataSet)
			{
				// 회수할 일이 없으므로 번호표는 받지 않는다. 출처는 보스 자신.
				DataSet->GiveToAbilitySystem(AbilitySystemComponent, nullptr, this);
			}
		}
	}

	// ③ [모든 컴퓨터] "나 보스야" 등록 → 그 컴퓨터의 HUD가 보스 체력바를 띄운다.
	//    서버에서도 등록한다. (리슨 서버 호스트 화면에도 체력바가 필요하고, 나중에 기믹 코드가 서버에서 보스를 찾는다)
	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->RegisterBoss(this);
	}
}

void ALOABossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HiddenTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->UnregisterGameplayTagEvent(HiddenTagChangedHandle, LOAGameplayTags::State_Hidden, EGameplayTagEventType::NewOrRemoved);
		HiddenTagChangedHandle.Reset();
	}

	if (CounterableTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->UnregisterGameplayTagEvent(CounterableTagChangedHandle, LOAGameplayTags::State_Counterable, EGameplayTagEventType::NewOrRemoved);
		CounterableTagChangedHandle.Reset();
	}

	if (ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(this))
	{
		Raid->UnregisterBoss(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ALOABossCharacter::OnHiddenTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	 bool bHiddenCharacter = NewCount > 0;
	
	// 보이기 / 숨기기. 서버가 바꾼 값도 복제되지만, 태그를 보고 각 컴퓨터가 같은 순간에 맞춘다. (사망과 같은 방식)
	SetActorHiddenInGame(bHiddenCharacter);

	// 사라진 동안 플레이어가 보스 자리를 지나갈 수 있게
	BodyBlockerComponent->SetSuppressed(bHiddenCharacter);

	LOA_LOG(this, Log, TEXT("[Boss] %s"), bHiddenCharacter ? TEXT("hidden (State.Hidden)") : TEXT("visible again"));
}

void ALOABossCharacter::OnCounterableTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bCounterWindowOpen = NewCount > 0;

	// 알리기만 한다. 파란 빛은 BP가 (오버레이 머티리얼, 이펙트 등)
	OnCounterWindowChanged(bCounterWindowOpen);
}
