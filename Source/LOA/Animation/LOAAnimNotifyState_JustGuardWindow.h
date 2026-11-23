// LOAAnimNotifyState_JustGuardWindow.h
// [배관] 보스 패턴 몽타주의 "저가 구간". 구간 하나 = 저가 ID 하나 = 막을 공격 하나 (홀딩은 연타 전체). (2026-10-26)
//
// ─────────────────────────────────────────────────────────────────────────────
// [하는 일] (LOA Grant Tags를 물려받는다)
//   구간 시작 [서버] : 노란 빛 태그(State.Window.JustGuardable) + 저가 담당에 구간 열기 → #7 → 모든 플레이어에게 "열림"
//                      구간 길이(초)도 같이 보낸다 = 노티파이 길이 ÷ 몽타주 재생 속도 (홀딩 게이지. 2026-11-05)
//                        예) 노티파이 1.88, Rate Scale 0.7 → 2.69초
//   구간 끝   [서버] : 구간 닫기 + 노란 빛 끄기 (몽타주가 중간에 끊겨도 온다)
//
// [칠하는 법]
//   Notifies 트랙 → Add Notify State → LOA Just Guard Window → Mode (Tap / Hold)
//   단타 : 공격 하나마다 구간 하나. 닿기 0.5초 전보다 일찍 시작, 닿은 뒤에 끝
//          예) 1.6초에 내려찍기 → 1.0 ~ 1.8
//          연타면 공격마다 따로, 겹치지 않게. 예) 1.6초 · 2.4초 → 1.0~1.8, 1.85~2.6
//   홀딩 : 연타 전체를 구간 하나로. 첫 타 0.5초 전보다 일찍 시작, 마지막 타 뒤에 끝
//   장판으로 때리면: 장판이 맞히는 순간에도 구간이 열려 있어야 한다
//   패턴 GA의 Asset Tags에 Skill.Attribute.JustGuard도 있어야 막을 수 있다
//   Montage Tick Type = Branching Point

#pragma once

#include "CoreMinimal.h"
#include "Animation/LOAAnimNotifyState_GrantTags.h"
#include "Library/LOACombatTypes.h"   // ELOAJustGuardMode
#include "LOAAnimNotifyState_JustGuardWindow.generated.h"

UCLASS(meta = (DisplayName = "LOA Just Guard Window"))
class LOA_API ULOAAnimNotifyState_JustGuardWindow : public ULOAAnimNotifyState_GrantTags
{
	GENERATED_BODY()

public:
	ULOAAnimNotifyState_JustGuardWindow();

	//~ Begin UAnimNotifyState Interface
	/** 구간 시작 → [서버] 노란 빛 GE (부모) + 저가 구간 열기 */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	/** 구간 끝 → [서버] 저가 구간 닫기 + 노란 빛 GE 제거 (부모) */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 타임라인 이름: "Just Guard (Tap)" / "Just Guard (Hold)" */
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotifyState Interface

protected:
	/** 단타 = 이 구간의 공격 하나 / 홀딩 = 이 구간의 연타 전체 (첫 타를 제때, 그 뒤로는 누르고 있는 동안) */
	UPROPERTY(EditAnywhere, Category = "LOA")
	ELOAJustGuardMode Mode = ELOAJustGuardMode::Tap;

private:
	/**
	 * 지금 열려 있는 저가 ID (메시마다, 먼저 연 것부터). 이 노티파이 객체는 몽타주에 하나뿐이고 모든 보스가 같이 쓰므로 메시별로 나눈다.
	 * 서버에서만 채워진다.
	 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<int32>> OpenWindowIds;
};
