// LOAGA_Airborne.h
// [배관] 띄움. 플레이어가 띄움 태그를 받으면 자동으로 발동한다 (키로 발동하지 않음). 모든 직업 공용.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (서버)  예) 보스 내려찍기, GE_CC_Airborne { DownDuration 1.0, Distance 600 } (Launch Height 250, Air Time 1.0)
//
//   B가 맞음 → GE_CC_Airborne → B에게 State.CrowdControl.Push.Airborne → 이 Ability 발동 (Trigger = 태그가 붙음)
//     (GE의 시간은 안전용 최대 시간뿐. 끝내는 쪽은 언제나 이 Ability → EndAbility가 띄움 GE를 지운다)
//     ① 하던 스킬 전부 취소, 이동 금지(State.Block.Move GE → 복제 → B 컴퓨터의 클릭 이동이 멈춤)
//     ② 발사: 정해진 궤적 (2026-10-21 바꿈: 물리 → 궤적. ULOACharacterMovementComponent::StartCrowdControlMove)
//          Air Time(1.0초) 동안 옆은 일정하게 600cm, 높이는 포물선으로 0.5초에 250 → 1.0초에 착지
//     ②' 공중에서 또 띄움 GE를 맞으면 → 지금 높이에서 다시 발사. 꼭대기는 "처음 뜬 땅 + 250" 그대로, 시간도 1.0초 그대로,
//          높이 맞을수록 멀리 (High Hit Distance Bonus). 예) 200 높이에서 맞음 → 50 더 올라 꼭대기 → 1.0초에 착지
//     ③ 착지 → State.CrowdControl.Push.Down GE (복제) → 누움 몽타주. DownDuration(1.0초)을 착지 순간부터 센다
//          → 높이를 바꿔도 누워 있는 시간은 같다 (2026-10-20 사용자 결정 B)
//          이제 띄움 GE는 Ignore 조건에 걸려 안 붙는다 (데미지는 들어감)
//          착지하면 그 자리에서 멈춘다 (미끄러짐 없음). 착지점이 낭떠러지 밖이면 1.0초 뒤부터 보통 낙하 → 낙사 가능
//     ④ 1.0초 뒤 → 기상 몽타주 → 끝나면 종료 (EndAbility가 Down·띄움·이동 금지 GE를 지움). 무적 없음
//          누움 태그는 완전히 일어날 때까지 남는다 (2026-10-20 사용자 결정):
//          일어나는 중에도 기상기가 나가고, 회피는 막히고, 맞아도 안 뜬다
//   기상기(ULOAGA_WakeUp): 누워 있을 때 스페이스 → 이 Ability를 끊는다(Cancel Abilities with Tag)
//     → EndAbility가 띄움·누움·이동 금지 GE를 모두 지운다 → 기상기가 구르며 일어남
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 서버가 숫자를 정하나] (2026-10-19 사용자 결정, 10-21 궤적으로 바꾸며 유지)
//   방향을 "판정 기준점 → B 위치"로 정하는데, B 위치는 서버와 B 컴퓨터가 서로 조금 다르다.
//   장판 중심 바로 옆이면 서버는 남서쪽, B 컴퓨터는 북동쪽으로 계산할 수 있다 → 각자 정하면 정반대로 갔다가 순간이동.
//   그래서 서버가 방향·거리·높이·시간을 정하고, 이동 컴포넌트가 그 숫자를 B 컴퓨터로 복제해 같은 궤적을 건다.
//   대가: B 화면에서 핑만큼 늦게 뜬다 (핑 100ms → 약 0.1초). Victim 판정이면 보고가 오가는 만큼 더 늦다.
//
// [왜 GE가 아니라 이 GA가 날리나] GE는 "붙어 있다"만 한다. 날리기·착지·기상 순서는 흐름이라 GA가 쥔다 (흐름은 GA).
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만     : 발동, 취소, 궤적 숫자 정하기, 착지 판단, 누움 GE, 기상 시각
//   2. 복제       : 띄움·누움·이동 금지 태그 (GE), 몽타주, 궤적 숫자 (이동 컴포넌트의 CrowdControlMove, B에게만)
//   3. 각자 할 일 : 궤적 걸기, 때린 쪽 바라보기 (서버와 B 컴퓨터가 같은 숫자로)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_Airborne (부모: LOAGA_Airborne)
//   LOA|Ability|Montage : Montage = 띄움 몽타주 하나에 섹션 셋
//                           Air    (공중. 자기 자신으로 반복 연결)
//                           Down   (누움. 자기 자신으로 반복 연결)
//                           GetUp  (일어남. 연결 없음 → 끝나면 Ability 종료)
//   LOA|Airborne        : 섹션 이름 (기본 Air / Down / GetUp), Ability Tags To Keep (선택)
//   꾸러미 등록         : 직업의 플레이어 꾸러미에 InputTag 없이 추가 (모든 직업 공용 꾸러미가 있으면 거기)
//   몽타주가 없으면 섹션 없이 동작한다 (기상은 바로 끝남)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffectTypes.h"
#include "Engine/HitResult.h"
#include "LOAGA_Airborne.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;
struct FActiveGameplayEffect;

UCLASS(Abstract)
class LOA_API ULOAGA_Airborne : public ULOAGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Airborne(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	// ---------- 데이터 (GA_Airborne BP의 Class Defaults에서 조정) ----------

	/** 몽타주 섹션: 공중 (반복) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne")
	FName AirSection = TEXT("Air");

	/** 몽타주 섹션: 누움 (반복) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne")
	FName DownSection = TEXT("Down");

	/** 몽타주 섹션: 일어남 (끝나면 Ability 종료) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne")
	FName GetUpSection = TEXT("GetUp");

	/** 뜰 때 취소하지 않을 Ability (Asset Tags 기준). 비우면 이 Ability를 뺀 전부 취소. 예) 패시브 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Airborne")
	FGameplayTagContainer AbilityTagsToKeep;

private:
	/** 지금 어느 단계인가 */
	enum class EPhase : uint8
	{
		InAir,		// 날아가는 중
		Down,		// 누워 있음 (미끄러지는 중 포함)
		GettingUp,	// 일어나는 몽타주
	};

	/** 이 띄움 GE의 숫자·방향으로 날린다. 공중에서 또 맞았을 때도 같은 함수 */
	void LaunchFromEffect(const FGameplayEffectSpec& Spec);

	/** 지금 붙어 있는 띄움 GE 중 가장 늦게 붙은 것 (발동 순간 처음 날릴 GE). 없으면 nullptr */
	const FActiveGameplayEffect* FindNewestAirborneEffect() const;

	/** [알림] 내게 GE가 적용됨 → 띄움 GE면 다시 발사 */
	void OnEffectAppliedToSelf(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle ActiveHandle);

	/** [알림] 띄움 태그 개수가 바뀜 → 0이 됨 = 안전 시간이 다 됨 (착지를 못 함 등) → 경고하고 종료 */
	void OnAirborneTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** [알림] 착지 */
	UFUNCTION()
	void OnLanded(const FHitResult& Hit);

	/** 착지 → 누움 + 누워 있을 시간 대기 */
	void EnterDown();

	/** 누워 있을 시간이 끝남 → 일어남 */
	UFUNCTION()
	void OnDownTimeFinished();

	/** 누움 → 일어남 (누움 태그는 그대로). 몽타주가 없으면 바로 종료 */
	void StartGetUp();

	/** 몽타주가 끝남 (일어나는 섹션이 끝까지 재생됨) */
	UFUNCTION()
	void OnAirborneMontageEnded();

	/** 몽타주 섹션으로 점프 (몽타주·섹션이 있을 때만) */
	void JumpToSectionIfAny(FName Section);

	/** 내가 서버에서 붙인 GE를 지운다 (번호표를 비움) */
	void RemoveOwnedEffect(FActiveGameplayEffectHandle& InOutHandle) const;

	EPhase Phase = EPhase::InAir;

	/** 착지한 뒤 누워 있을 시간 (초). 마지막으로 날린 띄움 GE의 SetByCaller.DownDuration (없으면 GE의 Default Down Duration) */
	float PendingDownDuration = 1.f;

	/** 처음 뜰 때의 발 높이 (월드 Z). 꼭대기 = 이 높이 + Launch Height. 공중에서 또 맞을 때 "지금 몇 cm 떠 있나"의 기준 */
	float LaunchGroundZ = 0.f;
	bool bHasLaunchGround = false;

	/** 이번 발동에서 마지막으로 날릴 때 쓴 띄움 GE. 같은 GE의 "적용됨" 알림이 한 번 더 오면 무시 */
	FActiveGameplayEffectHandle LastLaunchEffectHandle;

	/** 이동 금지(State.Block.Move) GE 번호표. 종료 때 지운다 */
	FActiveGameplayEffectHandle BlockMoveEffectHandle;

	/** 누움(State.CrowdControl.Push.Down) GE 번호표. 종료 때 지운다 (일어나는 동안에도 유지) */
	FActiveGameplayEffectHandle DownEffectHandle;

	/** 알림 구독 번호 (종료 때 해제) */
	FDelegateHandle EffectAppliedHandle;
	FDelegateHandle AirborneTagHandle;
};
