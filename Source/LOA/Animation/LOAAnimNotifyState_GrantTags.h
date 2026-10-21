// LOAAnimNotifyState_GrantTags.h
// [배관] 몽타주의 "구간" 동안 메시 주인의 ASC에 태그를 붙여 두는 AnimNotifyState.
//
// ─────────────────────────────────────────────────────────────────────────────
// [로아로 치면]
//   회피          : 대시하는 동안만 피격 이상·상태 이상 면역  → State.Immune.HitReaction / State.Immune.StatusEffect
//   슈퍼아머 스킬 : 돌진하는 구간만 피격 이상 면역          → State.Immune.HitReaction
//   보스 패턴     : 파란 빛이 도는 구간만 카운터 가능        → (카운터 작업 때 태그 추가)
//
// 스킬마다 "태그 붙이기/떼기" 코드를 만들지 않는다. 몽타주 타임라인에 이 구간을 칠하고 태그만 고른다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [방식 - 서버가 GE로 붙인다]
//
//   몽타주 |────── NotifyState 구간 (애니 시간 T, 재생 속도 R) ──────|
//          ↑ NotifyBegin [서버]                                      ↑ NotifyEnd [서버]
//          ULOAGE_TimedTags 적용                                     그 GE 제거
//            태그 = Tags, 유지 시간 = T / R (실제 구간 길이)
//
// - 서버만 적용한다. 피격 반응·상태 이상은 서버가 판정하므로 서버 ASC의 태그가 기준이다.
//   GE는 GAS가 복제하므로 조종하는 사람 컴퓨터와 다른 플레이어 컴퓨터에도 태그가 보인다. (UI/연출용)
// - 정상이면 NotifyEnd에서 GE를 지운다.
//   만약 NotifyEnd가 오지 않아도(캐릭터가 구간 도중 사라짐, 애님 인스턴스 교체 등)
//   GE에 유지 시간이 있으므로 구간 길이만큼 지나면 스스로 사라진다. → 태그가 영원히 남지 않는다.
// - NotifyBegin이 두 번 불려도(Tick Type 설정 실수) 각 GE가 자기 시간 뒤에 사라지므로 영원히 남지 않는다.
//
// [로컬 태그(AddLooseGameplayTag)를 쓰지 않는 이유]
// 로컬 태그는 GAS가 수명을 관리하지 않는다. NotifyEnd를 놓치면 PlayerState의 ASC에 영원히 남는다. (부활해도 계속 면역)
//
// ─────────────────────────────────────────────────────────────────────────────
// [에디터에서 사용]
//   몽타주 → Notifies 트랙 우클릭 → Add Notify State → "LOA Grant Tags"
//   구간의 시작/끝을 끌어서 맞추고, 디테일에서 Tags 선택
//   Montage Tick Type = Branching Point (게임 로직용 노티파이는 항상 이것)

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "LOAAnimNotifyState_GrantTags.generated.h"

class UAbilitySystemComponent;
class USkeletalMeshComponent;

UCLASS(meta = (DisplayName = "LOA Grant Tags"))
class LOA_API ULOAAnimNotifyState_GrantTags : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	//~ Begin UAnimNotifyState Interface
	/** 구간 시작 → [서버] 태그 GE 적용 */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	/** 구간 끝 (몽타주가 중간에 끊겨도 호출됨) → [서버] 그 GE 제거 */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 몽타주 타임라인에 표시되는 이름. 어떤 태그 구간인지 한눈에 보이게 한다. */
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotifyState Interface

protected:
	/** 구간 동안 붙일 태그. (예: State.Immune.HitReaction) */
	UPROPERTY(EditAnywhere, Category = "LOA", meta = (Categories = "State"))
	FGameplayTagContainer Tags;

private:
	/**
	 * 메시 주인의 ASC. 서버가 아니면 nullptr. (캐릭터 → IAbilitySystemInterface → PlayerState의 ASC / 보스는 자기 ASC)
	 * 에디터 몽타주 미리보기에서도 nullptr. (미리보기 액터에는 ASC가 없음)
	 */
	static UAbilitySystemComponent* GetServerASC(const USkeletalMeshComponent* MeshComp);

	/**
	 * 지금 적용 중인 GE 번호표. (메시마다, 먼저 시작한 구간부터)
	 * 이 노티파이 객체는 몽타주 에셋에 하나뿐이고 모든 캐릭터가 같이 쓰므로, 메시별로 나눠 보관한다.
	 * 같은 몽타주를 연속으로 써서 이전 구간이 끝나기 전에 다음 구간이 시작돼도 순서대로 지울 수 있게 배열로 둔다.
	 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FActiveGameplayEffectHandle>> ActiveHandles;
};
