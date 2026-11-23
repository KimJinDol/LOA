// LOAAnimNotifyState_JustGuardWindow.cpp

#include "Animation/LOAAnimNotifyState_JustGuardWindow.h"
#include "Combat/LOAJustGuardSubsystem.h"
#include "Core/LOAGameplayTags.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAAnimNotifyState_JustGuardWindow)

ULOAAnimNotifyState_JustGuardWindow::ULOAAnimNotifyState_JustGuardWindow()
{
	// 노란 빛 (보스 BP 이벤트 On Just Guard Window Changed가 듣는다)
	Tags.AddTag(LOAGameplayTags::State_Window_JustGuardable);
}

void ULOAAnimNotifyState_JustGuardWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	// ① 노란 빛 GE (부모, 서버만)
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// ② [서버] 저가 구간 열기 (미리보기·클라에서는 아무것도 안 함. 클라는 ClientJustGuardWindow로 받는다)
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	ULOAJustGuardSubsystem* JustGuardSubsystem = Owner ? ULOAJustGuardSubsystem::Get(Owner) : nullptr;
	if (!JustGuardSubsystem || !Owner->HasAuthority())
	{
		return;
	}

	// 구간 길이(초). 엔진이 주는 TotalDuration은 애니메이션 시간이라 몽타주 재생 속도로 나눈다
	//   재생 속도 = 몽타주 인스턴스의 Play Rate × 몽타주의 Rate Scale (엔진 AnimMontage.cpp와 같은 계산)
	float WindowSeconds = TotalDuration;
	const UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	const UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (const FAnimMontageInstance* MontageInstance = (Montage && AnimInstance) ? AnimInstance->GetActiveInstanceForMontage(Montage) : nullptr)
	{
		const float PlayRate = FMath::Abs(MontageInstance->GetPlayRate() * Montage->RateScale);
		if (PlayRate > KINDA_SMALL_NUMBER)
		{
			WindowSeconds = TotalDuration / PlayRate;
		}
	}

	const int32 WindowId = JustGuardSubsystem->OpenWindow(Owner, Mode, WindowSeconds);
	if (WindowId == 0)
	{
		return;
	}

	// 사라진 메시의 기록은 이참에 정리
	for (auto It = OpenWindowIds.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	OpenWindowIds.FindOrAdd(MeshComp).Add(WindowId);
}

void ULOAAnimNotifyState_JustGuardWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// ① [서버] 이 메시에서 가장 먼저 연 구간을 닫는다
	if (TArray<int32>* WindowIds = OpenWindowIds.Find(MeshComp))
	{
		if (WindowIds->Num() > 0)
		{
			if (ULOAJustGuardSubsystem* JustGuardSubsystem = ULOAJustGuardSubsystem::Get(MeshComp))
			{
				JustGuardSubsystem->CloseWindow((*WindowIds)[0]);
			}
			WindowIds->RemoveAt(0);
		}
		if (WindowIds->Num() == 0)
		{
			OpenWindowIds.Remove(MeshComp);
		}
	}

	// ② 노란 빛 GE 제거 (부모)
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString ULOAAnimNotifyState_JustGuardWindow::GetNotifyName_Implementation() const
{
	switch (Mode)
	{
	case ELOAJustGuardMode::Hold:			return TEXT("Just Guard (Hold)");
	case ELOAJustGuardMode::HoldFromMiddle:	return TEXT("Just Guard (Hold From Middle)");
	default:								return TEXT("Just Guard (Tap)");
	}
}
