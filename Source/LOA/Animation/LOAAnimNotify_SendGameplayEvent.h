// LOAAnimNotify_SendGameplayEvent.h
// [배관] 몽타주의 특정 프레임에서 GameplayEvent를 보내는 AnimNotify.
// "칼이 닿는 순간", "투사체를 쏘는 순간" 같은 타이밍을 Ability에 알려 준다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   몽타주 재생 중 이 Notify가 있는 프레임 도달
//      │
//      ▼ Notify()
//   메시 주인(캐릭터)의 ASC를 찾는다  (캐릭터 → IAbilitySystemInterface → PlayerState의 ASC)
//      │
//      ▼ HandleGameplayEvent(EventTag)
//   그 ASC에서 이 태그를 기다리던 AbilityTask(WaitGameplayEvent)가 깨어난다
//
// ─────────────────────────────────────────────────────────────────────────────
// [어디서 실행되나]
// Notify는 "몽타주를 재생하는 모든 컴퓨터"에서 각각 실행된다.
//   조종하는 사람 컴퓨터 : Ability 실행 중 → 이벤트 받음 (예측: 이펙트, 사운드 등)
//   서버                 : Ability 실행 중 → 이벤트 받음 (판정, 데미지)
//   다른 플레이어 컴퓨터 : Ability가 없다 → 기다리는 Task가 없으니 아무 일도 없음
// 그래서 "판정은 서버에서만"은 받는 쪽(Ability)이 HasAuthority로 구분한다.
//
// ※ 데디케이티드 서버는 화면이 없지만, 캐릭터 메시는 기본적으로 항상 애니메이션을 계산하므로
//   서버에서도 Notify가 발생한다. (ACharacter 기본 설정)
//
// ─────────────────────────────────────────────────────────────────────────────
// [에디터에서 사용]
//   몽타주 → Notifies 트랙 우클릭 → Add Notify → "LOA Send Gameplay Event"
//   디테일: Event Tag = Event.Montage.Hit
//           Montage Tick Type = Branching Point (정확한 프레임에 발생. 판정용은 이걸 권장)

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "LOAAnimNotify_SendGameplayEvent.generated.h"

class UAnimMontage;

UCLASS(meta = (DisplayName = "LOA Send Gameplay Event"))
class LOA_API ULOAAnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	//~ Begin UAnimNotify Interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 몽타주 타임라인에 표시되는 이름. 태그를 보여 줘서 어떤 알림인지 한눈에 알 수 있게 한다. */
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotify Interface

	/** 보낼 이벤트 태그. (스킬 저장 시 검사가 "몽타주에 이 태그 노티파이가 있나" 확인할 때 읽는다) */
	FGameplayTag GetEventTag() const { return EventTag; }

	/**
	 * 몽타주에 찍힌 이 노티파이 중 EventTag인 것을 찾아, FromTime(초) 이후 가장 이른 것의 시각을 돌려준다.
	 * 시각은 몽타주 타임라인 기준 (재생 속도 반영 전). 예) 도끼가 닿는 노티파이가 1.2초 지점 → 1.2
	 * 쓰는 곳: 범위 액터의 첫 판정 시각 계산 (ULOAGameplayAbility::GetTimeUntilMontageEvent)
	 * @return 없으면 false
	 */
	static bool FindEventTimeInMontage(const UAnimMontage* Montage, const FGameplayTag& InEventTag, float FromTime, float& OutTime);

protected:
	/** 보낼 이벤트 태그. Ability의 WaitGameplayEvent 태그와 같아야 한다. */
	UPROPERTY(EditAnywhere, Category = "LOA", meta = (Categories = "Event"))
	FGameplayTag EventTag;
};
