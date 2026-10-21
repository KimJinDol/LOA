// LOATrainingDummy.cpp

#include "Actors/LOATrainingDummy.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "Data/LOAAbilityDataSet.h"
#include "Core/LOALog.h"

#include "Components/CapsuleComponent.h"
#include "Character/LOABodyBlockerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOATrainingDummy)

ALOATrainingDummy::ALOATrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 체력 변화를 클라이언트에도 보여야 하므로 복제한다.
	bReplicates = true;

	// 매 프레임 할 일이 없다.
	PrimaryActorTick.bCanEverTick = false;

	// ---------- 몸통 ----------
	// 크기(반지름/높이)는 BP에서 조정. 여기서는 콜리전 성격만 정한다.
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	SetRootComponent(Capsule);

	// ---------- 겉모습 ----------
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ---------- 몸 막이 ----------
	// 플레이어 캡슐은 Pawn을 무시한다(플레이어끼리 통과). 보스처럼 플레이어를 막는 건 몸 막이가 한다.
	BodyBlocker = CreateDefaultSubobject<ULOABodyBlockerComponent>(TEXT("BodyBlocker"));
	BodyBlocker->SetupAttachment(Capsule);

	// ---------- ASC ----------
	ASC = CreateDefaultSubobject<ULOAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	ASC->SetIsReplicated(true);

	// Minimal: 버프 상세 정보를 아무에게도 보내지 않는다. (속성 값은 AttributeSet 복제로 따로 전달됨)
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// ---------- 체력 칸 ----------
	CommonAttributeSet = CreateDefaultSubobject<ULOAAttributeSet_Common>(TEXT("CommonAttributeSet"));
}

UAbilitySystemComponent* ALOATrainingDummy::GetAbilitySystemComponent() const
{
	return ASC;
}

void ALOATrainingDummy::BeginPlay()
{
	Super::BeginPlay();

	// Owner도 Avatar도 자기 자신. 서버/클라이언트 양쪽에서 호출해야 각자의 ASC가 몸을 안다.
	ASC->InitAbilityActorInfo(this, this);

	// 체력 변경 알림 등록. 초기화 GE보다 먼저 등록해야 초기 체력이 들어오는 순간도 로그에 남는다.
	HealthChangedHandle = ASC
		->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetHealthAttribute())
		.AddUObject(this, &ThisClass::OnHealthChanged);

	// [서버] 초기화 GE 등 부여
	if (HasAuthority())
	{
		if (AbilityDataSets.IsEmpty())
		{
			LOA_LOG(this, Warning, TEXT("AbilityDataSets is empty. Health will stay 0."));
		}

		for (const ULOAMonsterAbilityDataSet* DataSet : AbilityDataSets)
		{
			if (DataSet)
			{
				// 회수할 일이 없으므로 번호표는 받지 않는다. 출처는 허수아비 자신.
				DataSet->GiveToAbilitySystem(ASC, nullptr, this);
			}
		}
	}
}

void ALOATrainingDummy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ResetTimerHandle);

	if (ASC && HealthChangedHandle.IsValid())
	{
		ASC
			->GetGameplayAttributeValueChangeDelegate(ULOAAttributeSet_Common::GetHealthAttribute())
			.Remove(HealthChangedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ALOATrainingDummy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	const float Delta = Data.NewValue - Data.OldValue;
	const float MaxHealth = CommonAttributeSet ? CommonAttributeSet->GetMaxHealth() : 0.f;

	if (Delta < 0.f)
	{
		// 피격. 서버/클라이언트 양쪽에서 찍혀서 "복제가 됐는지"도 함께 확인된다.
		LOA_LOG(this, Log, TEXT("Damaged %.1f  (Health %.1f -> %.1f / %.1f)"), -Delta, Data.OldValue, Data.NewValue, MaxHealth);

		// [서버] 마지막 피격 기준으로 회복 타이머를 다시 건다. (계속 맞는 동안은 회복하지 않음)
		if (HasAuthority() && ResetDelay > 0.f)
		{
			GetWorldTimerManager().SetTimer(ResetTimerHandle, this, &ThisClass::ResetHealth, ResetDelay, false);
		}
	}
	else if (Delta > 0.f)
	{
		LOA_LOG(this, Log, TEXT("Healed %.1f  (Health %.1f -> %.1f / %.1f)"), Delta, Data.OldValue, Data.NewValue, MaxHealth);
	}
}

void ALOATrainingDummy::ResetHealth()
{
	if (!HasAuthority() || !CommonAttributeSet)
	{
		return;
	}

	// 서버에서 체력 기본값을 최대치로 직접 설정한다. → 복제되어 클라이언트에도 반영
	// (허수아비 전용 편의 기능이라 GE를 따로 만들지 않았다. 실제 회복 스킬은 GE로 처리한다)
	// SetNumericAttributeBase : 특정 어트리뷰트의 기본값을 영구적으로 수정해주는 함수
	ASC->SetNumericAttributeBase(
		ULOAAttributeSet_Common::GetHealthAttribute(),
		CommonAttributeSet->GetMaxHealth());
}
