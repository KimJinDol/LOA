// LOAGA_Combo.h
// [배관] 콤보 GA. "누를 때마다(또는 누르고 있으면) 1타 → 2타 → 3타로 이어진다." (로아 평타)
// 발동 한 번에 콤보 전체를 처리한다. 다음 타는 같은 발동 안에서 몽타주 섹션을 점프해서 낸다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [디테일 패널]
//
//   ▼ LOA | Ability            ← 부모와 같음 (Montage, bFaceCursor …)
//   ▼ LOA | Ability | Combo
//        Combo Stages
//           [0] Section Attack1     ← 1타
//           [1] Section Attack2     ← 2타
//           [2] Section Attack3     ← 3타 (마지막)
//        Continue While Held     ☑  ← 누르고 있으면 계속 이어짐
//        Face Cursor Each Stage  ☑  ← 다음 타마다 커서 쪽으로 돎. 끄면 1타 방향 그대로 (bFaceCursor가 켜져 있을 때만 보임)
//   ▼ LOA | Ability | Modules
//        [0] Apply  Event.Montage.Hit → Payloads [0] GE_Damage + SetByCaller.DamageRate 1.0
//
//   데미지 = 공격력 × Apply의 DamageRate. 지금은 모든 타가 같은 계수다. (다른 스킬과 같은 방식)
//   타마다 다른 계수는 버프/시너지 작업 때 밸런싱과 함께 넣는다. 그때 FLOAComboStage에 칸을 추가한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [몽타주]
//
//   | Attack1                  | Attack2                  | Attack3            |
//      Hit  ComboOpen  Unlock     Hit  ComboOpen  Unlock     Hit       Unlock
//
//   - 섹션 하나 = 한 타 (타격 + 후딜 꼬리)
//   - ComboOpen 노티파이 (LOA Send Gameplay Event, Event.Montage.ComboOpen, Branching Point)
//     = 여기부터 다음 타로 넘어갈 수 있다. 마지막 타에는 필요 없다.
//   - 섹션 연결(Montage Sections 패널)은 모두 끊어 둔다. 다음 타로는 코드가 점프한다.
//     이어져 있으면 입력 없이 다음 타가 저절로 나간다. (저장 시 오류)
//   - 첫 단계의 섹션이 몽타주의 첫 섹션이어야 한다. (몽타주는 처음부터 재생된다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [누가 정하고 누가 실행하나]
//   다음 타로 넘어갈지는 조종하는 사람 컴퓨터(클라) 한 곳에서만 정한다. (ComboOpen, 예약, 누르고 있기)
//   서버는 스스로 판단하지 않고 클라의 "다음 타" 신호를 받아 실행만 한다.
//   이유: 몽타주 점프는 클라가 MontageJumpToSection을 부르면 엔진이 서버로 점프 RPC를 보낸다.
//         서버가 따로 판단하면 둘이 다르게 결정했을 때 "몽타주는 2타인데 서버는 1타라고 생각"하게 된다.
//
// [흐름 - 조종하는 사람 컴퓨터]
//
//   누름 → (부모) 커서로 회전 → BeginSkill → StartSkill(여기) → 1타 재생
//   1타 진행 중
//      ├ ComboOpen 전에 또 누름  → 예약
//      ├ ComboOpen 도착          → 예약이 있거나 (Continue While Held && 누르는 중) → 다음 타
//      │                            아니면 → 입력 창 열기
//      ├ 입력 창이 열린 뒤 누름   → 다음 타
//      └ 아무 입력 없음           → 섹션 끝까지 재생 → 몽타주 끝 → 정상 종료 (다음엔 다시 1타)
//
//   다음 타 (AdvanceLocal)
//      ① (Face Cursor Each Stage일 때만) 커서 전달: WaitCursorTarget이 커서를 구해 서버로 보냄
//      ② "다음 타" 신호를 서버로 보냄 (ServerSetReplicatedEvent - GenericSignalFromClient)
//      ③ 내 쪽: 회전 → 잠금 다시 걸기(RestartLock) → 다음 섹션으로 점프 (엔진이 점프 RPC를 서버로 보냄)
//
//   후딜(Unlock) 뒤에 이동하면 이동 캔슬로 끝난다. (부모 규칙 그대로)
//   후딜 뒤에 다시 누르면: 스킬이 아직 실행 중이라 새로 발동하지 않고 "눌림"이 이 GA로 온다 → 다음 타.
//
// [흐름 - 서버 (원격 플레이어)]
//   StartSkill에서 두 가지를 구독한다.
//      "다음 타" 신호 → 단계 번호 +1 → 잠금 다시 걸기    (항상)
//      커서 데이터    → 커서로 회전                       (Face Cursor Each Stage일 때만)
//   몽타주 점프는 하지 않는다. 클라의 엔진 점프 RPC가 한다. (서버도 점프하면 같은 섹션을 두 번 시작 → Hit 중복 위험)
//
//   [서버가 다음 타를 알아야 하는 이유]
//   - 방향: 데미지 판정은 서버가 서버 캐릭터의 앞 방향으로 한다. FaceLocation은 액터 회전을 직접 바꾸는 것이라
//           이동 복제로 서버에 넘어가지 않는다. 그래서 돌아야 하는 콤보만 커서를 보내 양쪽이 각자 돈다.
//   - 잠금: 다음 타마다 후딜 상태(bInRecovery)를 끝내야 한다. 안 하면 서버만 "1타 후딜 중"으로 남아,
//           다음 타 후딜 전에 끊겼을 때 클라는 취소 종료, 서버는 정상 종료로 갈린다. → 신호는 항상 보낸다.
//
//   [순서] 클라는 커서 → 신호 → 점프 순서로 보낸다. 셋 다 reliable RPC라 서버에서도 이 순서로 도착한다.
//   → 새 타의 Hit가 나갈 때 서버 캐릭터는 이미 새 방향을 보고 있고, 잠금도 새 타 기준이다.
//   [창 검사 없음] 서버 몽타주는 발동 요청을 받은 만큼(지연) 늦게 시작하고, 신호도 같은 만큼 늦게 도착한다.
//   → 서버 입장에서도 "자기 몽타주의 비슷한 위치"에서 받는다. 그래서 서버가 ComboOpen 창을 따로 검사하지 않는다.
//     (코옵이라 이 정도 신뢰로 충분. 차지의 "클라가 재고 서버는 따라간다"와 같은 방향)
//
// [리슨 호스트 / 스탠드얼론]
//   자기가 서버라 신호를 보내지 않는다. 클라 흐름만 탄다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [아직 없는 것]
//   - 마지막 타 뒤에 누르고 있으면 1타부터 다시 (로아 평타 반복). 필요하면 칸 하나 추가로 넣는다.
//   - 콤보 + 차지 혼합 (마지막 타를 차지) : 나중에 WaitChargeRelease를 그대로 쓴다.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "LOAGA_Combo.generated.h"

class ULOAAbilityTask_ComboInput;

/**
 * 콤보 단계 하나 = 한 타.
 * 지금은 섹션 이름만 있다. 타마다 다른 값(데미지 계수 등)이 필요해지면 여기에 칸을 추가한다.
 * (섹션 이름 배열이 아니라 구조체로 둔 이유: 칸을 추가해도 BP에 이미 적은 섹션 이름이 그대로 남는다)
 */
USTRUCT(BlueprintType)
struct FLOAComboStage
{
	GENERATED_BODY()

	/** 이 타의 몽타주 섹션 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Combo")
	FName SectionName;
};

UCLASS(Abstract)
class LOA_API ULOAGA_Combo : public ULOAPlayerAbility
{
	GENERATED_BODY()

public:
	ULOAGA_Combo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin ULOAGameplayAbility Interface
#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사: 단계, 섹션 존재/중복/연결, 첫 섹션, ComboOpen 노티파이 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End ULOAGameplayAbility Interface

protected:
	//~ Begin ULOAPlayerAbility Interface
	/**
	 * 1타 재생 + 구독 시작
	 *   조종하는 사람 컴퓨터 : ComboOpen 대기, 입력 Task
	 *   서버(원격 플레이어)  : "다음 타" 신호, (Face Cursor Each Stage면) 다음 타 커서
	 */
	virtual void StartSkill() override;

	/** 서버의 "다음 타" 신호 구독을 해제한다. (Task가 아니라 직접 구독했기 때문) */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End ULOAPlayerAbility Interface

	// ---------- 데이터 (각 콤보 스킬 BP에서) ----------

	/** 콤보 단계 (한 칸 = 한 타). 순서대로 이어진다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Combo", meta = (TitleProperty = "SectionName"))
	TArray<FLOAComboStage> ComboStages;

	/** 누르고 있으면 ComboOpen에서 다음 타로 자동으로 이어진다. 끄면 누를 때만 이어진다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Combo")
	bool bContinueWhileHeld = true;

	/**
	 * 다음 타마다 커서 쪽으로 돌지. 끄면 1타 방향 그대로 끝까지 나간다.
	 * 발동할 때 돌지 않는 스킬(bFaceCursor 꺼짐)은 다음 타도 돌지 않으므로 이 칸은 보이지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Combo", meta = (EditCondition = "bFaceCursor", EditConditionHides))
	bool bFaceCursorEachStage = true;

private:
	// ---------- 조종하는 사람 컴퓨터 ----------
	/** ComboOpen 노티파이 도착 → 예약 / 누르는 중이면 다음 타, 아니면 입력 창 열기 */
	UFUNCTION()
	void OnComboOpen(FGameplayEventData Payload);

	/** 실행 중에 키를 다시 누름 → 입력 창이 열려 있으면 다음 타, 아니면 예약 */
	UFUNCTION()
	void OnLocalPressed();

	/**
	 * 다음 타로 넘어가기로 함.
	 *   돌아야 하면 : 커서 전달 시작(WaitNextCursor) → 결과가 바로 OnNextCursorReady/Failed로 옴 → FinishAdvanceLocal
	 *   아니면      : 바로 FinishAdvanceLocal
	 */
	void AdvanceLocal();

	/** 다음 타 실행 (클라): "다음 타" 신호 보내기 → EnterStage(회전, 잠금, 점프) */
	void FinishAdvanceLocal(bool bHasCursor, const FVector& NextCursorLocation);

	/** "다음 타" 신호를 서버로 보낸다. 원격 클라이언트일 때만. (리슨 호스트 / 스탠드얼론은 자기가 서버) */
	void SendNextStageSignal();

	// ---------- 서버 (원격 플레이어) ----------
	/** 클라의 "다음 타" 신호 도착 → 단계 +1 → 잠금 다시 걸기 (점프는 엔진 RPC가 함) */
	void OnNextStageSignal();

	// ---------- 다음 타 커서 (양쪽. Face Cursor Each Stage일 때만) ----------
	/** 다음 타마다 커서 쪽으로 도는 콤보인가 (bFaceCursor && bFaceCursorEachStage) */
	bool ShouldFaceCursorEachStage() const { return bFaceCursor && bFaceCursorEachStage; }

	/**
	 * 커서 전달 Task(WaitCursorTarget)를 켠다. 발동 때 커서를 보내는 Task와 같은 것이다.
	 *   조종하는 사람 컴퓨터 : 커서를 구해 서버로 보내고, 바로 OnNextCursorReady / Failed를 부른다.
	 *   서버(원격 플레이어)  : 클라이언트의 커서가 올 때까지 기다린다.
	 */
	void WaitNextCursor();

	/** 다음 타 커서 도착 (클라는 바로, 서버는 받았을 때) */
	UFUNCTION()
	void OnNextCursorReady(const FGameplayAbilityTargetDataHandle& DataHandle);

	/** 커서를 못 구함 (빈 데이터). 방향은 그대로 */
	UFUNCTION()
	void OnNextCursorFailed(const FGameplayAbilityTargetDataHandle& DataHandle);

	/**
	 * 커서 결과 처리 (양쪽 공통)
	 *   클라 : FinishAdvanceLocal로 이어서 다음 타 실행
	 *   서버 : 회전만 하고, 다음 커서를 다시 기다림 (단계 +1과 잠금은 뒤따라 오는 신호가 한다)
	 */
	void HandleNextCursor(bool bHasCursor, const FVector& NextCursorLocation);

	// ---------- 공용 ----------
	/**
	 * 단계 바꾸기: 번호 저장 → (커서 있으면) 회전 → 잠금 다시 걸기 → (bJumpMontage면) 섹션 점프
	 * @param bJumpMontage  조종하는 사람 컴퓨터는 true. 서버(원격 플레이어)는 false (엔진의 점프 RPC가 한다)
	 */
	void EnterStage(int32 StageIndex, bool bHasCursor, const FVector& NextCursorLocation, bool bJumpMontage);

	/** 다음 타가 남아 있는가 */
	bool HasNextStage() const { return ComboStages.IsValidIndex(CurrentStageIndex + 1); }

	/** [조종하는 사람 컴퓨터] 입력 Task (발동 동안 내내 켜 둠. 주인 = 이 Ability) */
	UPROPERTY()
	TObjectPtr<ULOAAbilityTask_ComboInput> InputTask;

	/** [서버] "다음 타" 신호 구독 번호표. EndAbility에서 해제한다. */
	FDelegateHandle NextStageSignalHandle;

	/** 지금 몇 번째 타인가 (0부터) */
	int32 CurrentStageIndex = 0;

	/** [조종하는 사람 컴퓨터] 지금 타의 ComboOpen이 지나서 입력을 받는 중인가 */
	bool bComboWindowOpen = false;

	/** [조종하는 사람 컴퓨터] ComboOpen 전에 누른 입력이 있는가 (예약) */
	bool bNextBuffered = false;
};
