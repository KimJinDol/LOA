// LOASpectateComponent.h
// [배관] 내가 죽어 있는 동안 살아 있는 아군 화면을 본다. (관전)
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙] (사용자 결정 2026-10-22)
//   - 내가 죽으면 Start Delay(기본 2초, 내 사망 모션을 보는 시간) 뒤 살아 있는 아군 카메라로 바꾼다.
//   - ← / → 로 다음·이전 아군.
//   - 보던 아군이 죽으면 다음 살아 있는 아군으로 저절로. 살아 있는 아군이 없으면 내 몸(시체)을 본다.
//   - 내가 살아나면 내 카메라로 돌아온다.
//
// [어떻게]
//   카메라를 바꾸는 건 엔진의 SetViewTargetWithBlend 하나다. 아군 캐릭터에도 카메라 부품이 있으므로
//   "보는 대상 = 그 캐릭터"로 바꾸면 그 캐릭터의 탑쿼터뷰 카메라로 보인다.
//   0.2초마다 확인한다: 내가 죽었나 / 보던 아군이 아직 살아 있나. (알림을 여러 캐릭터에 걸고 푸는 것보다 단순.
//   3명 × 태그 확인 하나라 비용은 거의 없고, 늦어도 0.2초)
//   아군 목록 = GameState의 PlayerArray 순서 (모든 컴퓨터에 같은 순서로 복제됨)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크] 3 각자 (본인 컴퓨터에서만)
//   카메라는 내 화면 일이라 서버에 알리지 않는다. 죽음 여부는 복제된 State.Body.Dead 태그로 판단한다.
//   PlayerController에 붙어 있어서 서버에도 있지만, 켜지는 건 본인 컴퓨터(SetupInputComponent)에서만이다.
//
// [BP에서 설정할 것] BP_LOAPlayerController → SpectateComponent
//   Spectate IMC          : IMC_Spectate (← = IA_SpectatePrev, → = IA_SpectateNext)
//   Next / Prev Target Action : IA_SpectateNext / IA_SpectatePrev (Value Type = Digital(bool))
//   Start Delay / Blend Time  : 기본 2초 / 0.3초

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LOASpectateComponent.generated.h"

class UEnhancedInputComponent;
class UInputMappingContext;
class UInputAction;
class APlayerController;

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOASpectateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOASpectateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * [본인 컴퓨터] 관전 키를 바인딩하고 확인(Tick)을 켠다. PlayerController::SetupInputComponent가 부른다.
	 * 서버의 남의 PlayerController에서는 불리지 않는다 → 그쪽은 꺼진 채로 비용 없음.
	 */
	void ActivateSpectate(UEnhancedInputComponent* EnhancedInput);

	/** 지금 보고 있는 아군. 관전 중이 아니면 nullptr (위젯에서 "OOO 관전 중" 표시용) */
	UFUNCTION(BlueprintPure, Category = "LOA|Spectate")
	APawn* GetSpectateTarget() const { return SpectateTarget.Get(); }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Input_Next();
	void Input_Prev();

	/** 살아 있는 아군 중 다음(+1) / 이전(−1)으로. 관전할 수 있는 때가 아니면 아무것도 안 함 */
	void Cycle(int32 Direction);

	/** 보는 대상을 바꾼다. nullptr = 내 몸 */
	void SetSpectateTarget(APawn* NewTarget);

	/** 나를 뺀, 살아 있는 아군 캐릭터들 (PlayerArray 순서) */
	void GatherLivingAllies(TArray<APawn*>& OutAllies) const;

	/** 죽었나 = ASC에 State.Body.Dead. ASC가 아직 없으면(복제 전) 살아 있다고 본다 */
	static bool IsDead(const APawn* Pawn);

	/** 내가 죽은 지 Start Delay가 지났나 (= 관전할 수 있는 때) */
	bool CanSpectate() const;

	APlayerController* GetOwningPlayerController() const;

	// ---------- 설정 (BP) ----------
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Spectate")
	TObjectPtr<UInputMappingContext> SpectateIMC;

	UPROPERTY(EditDefaultsOnly, Category = "LOA|Spectate")
	int32 SpectateIMCPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "LOA|Spectate")
	TObjectPtr<UInputAction> NextTargetAction;

	UPROPERTY(EditDefaultsOnly, Category = "LOA|Spectate")
	TObjectPtr<UInputAction> PrevTargetAction;

	/** 죽고 나서 관전으로 넘어가기까지 (초). 내 사망 모션을 보는 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Spectate", meta = (ClampMin = "0.0", Units = "Seconds"))
	float StartDelay = 2.f;

	/** 카메라가 넘어가는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Spectate", meta = (ClampMin = "0.0", Units = "Seconds"))
	float BlendTime = 0.3f;

	// ---------- 상태 ----------
	/** 보고 있는 아군. 비어 있으면 내 몸 */
	TWeakObjectPtr<APawn> SpectateTarget;

	/** 내가 죽은 시각 (게임 시간). 살아 있으면 음수 */
	double DeadSince = -1.0;
};
