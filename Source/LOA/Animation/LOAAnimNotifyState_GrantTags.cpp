// LOAAnimNotifyState_GrantTags.cpp

#include "Animation/LOAAnimNotifyState_GrantTags.h"
#include "AbilitySystem/Effects/LOAGE_TimedTags.h"
#include "Core/LOAGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAnimNotifyState_GrantTags)

void ULOAAnimNotifyState_GrantTags::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (Tags.IsEmpty())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetServerASC(MeshComp);
	if (!ASC)
	{
		return;   // 서버가 아님 / 미리보기 → 아무것도 안 함 (서버가 붙인 GE가 복제로 온다)
	}

	// ① 실제 구간 길이 = 애니 시간 / 몽타주 재생 속도 (MontagePlayRate 2.0이면 절반)
	float PlayRate = 1.f;
	if (const UAnimMontage* Montage = Cast<UAnimMontage>(Animation))
	{
		if (const UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			const float CurrentRate = AnimInstance->Montage_GetPlayRate(Montage);
			if (CurrentRate > KINDA_SMALL_NUMBER)
			{
				PlayRate = CurrentRate;
			}
		}
	}
	const float Duration = TotalDuration / PlayRate;

	// ② 공용 태그 GE에 시간과 태그를 넣어 적용
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);   // 디버그용: 어느 노티파이가 붙였는지

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ULOAGE_TimedTags::StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(LOAGameplayTags::SetByCaller_Duration, Duration);
	SpecHandle.Data->DynamicGrantedTags.AppendTags(Tags);
	

	const FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// ③ 번호표 보관 (NotifyEnd에서 지우기 위해). 사라진 메시의 기록은 이참에 정리한다.
	for (auto It = ActiveHandles.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	ActiveHandles.FindOrAdd(MeshComp).Add(ActiveHandle);
}

void ULOAAnimNotifyState_GrantTags::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UAbilitySystemComponent* ASC = GetServerASC(MeshComp))
	{
		// 이 메시에서 가장 먼저 시작한 구간의 GE를 지운다. (이미 시간이 지나 사라졌으면 아무 일도 없음)
		if (TArray<FActiveGameplayEffectHandle>* Handles = ActiveHandles.Find(MeshComp))
		{
			if (Handles->Num() > 0)
			{
				ASC->RemoveActiveGameplayEffect((*Handles)[0]);
				Handles->RemoveAt(0);
			}

			if (Handles->Num() == 0)
			{
				ActiveHandles.Remove(MeshComp);
			}
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString ULOAAnimNotifyState_GrantTags::GetNotifyName_Implementation() const
{
	// 예: "Tags: State.Immune.HitReaction, State.Immune.StatusEffect"
	return Tags.IsEmpty() ? TEXT("LOA Grant Tags (None)") : FString::Printf(TEXT("Tags: %s"), *Tags.ToStringSimple());
}

UAbilitySystemComponent* ULOAAnimNotifyState_GrantTags::GetServerASC(const USkeletalMeshComponent* MeshComp)
{
	UAbilitySystemComponent* ASC = MeshComp
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner())
		: nullptr;

	// ASC 주인(플레이어는 PlayerState, 보스는 보스 자신)이 이 컴퓨터에서 권한을 가질 때 = 서버 (리슨 서버 호스트 포함)
	return (ASC && ASC->IsOwnerActorAuthoritative()) ? ASC : nullptr;
}
