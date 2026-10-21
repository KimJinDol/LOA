// LOABodyBlockerComponent.h
// [배관] 몸 막이. 플레이어만 막는 두 번째 캡슐. 보스·몬스터·허수아비 몸에 붙인다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 필요한가] 로아 규칙: 보스는 플레이어를 뚫고 지나가고, 플레이어는 보스에 막힌다. 플레이어끼리는 통과.
//   언리얼 충돌은 두 물체 사이에 대칭이다. (둘 중 약한 쪽을 따름) → 보스 캡슐 하나로는 한쪽만 막을 수 없다.
//   → 보스 캡슐(이동용)은 Pawn을 무시하고, 플레이어를 막는 일은 이 캡슐이 따로 한다.
//   캐릭터 이동은 자기 루트 캡슐로만 부딪힘을 계산하므로
//     보스가 움직일 때 : 이 캡슐을 안 봄 → 플레이어를 뚫고 지나감
//     플레이어가 움직일 때 : 이 캡슐에 부딪힘 → 막힘
//   예) 보스 반지름 120 → 몸 막이도 반지름 120 (Match Owner Capsule)
//
// [충돌 설정] (생성자에서 정함)
//   종류 = BodyBlock (LOACollisionChannels.h) / Pawn만 Block, 나머지 전부 Ignore / Query Only
//   → 판정(장판·Apply·투사체)은 Pawn을 찾으니 이 캡슐에 걸리지 않는다.
//   → 플레이어 캡슐이 BodyBlock을 Block해야 막힌다. (ALOACharacter 생성자)
//   → 관통 스킬(예: 베스티지)은 Move 모듈의 Pass Through Body Blockers로 그 순간만 BodyBlock을 무시한다.
//   → 플레이어 쪽 응답(막힘/무시)은 ULOACharacterMovementComponent 한 곳만 바꾼다.
//
// [길찾기] 내비메시에는 영향 없음. 플레이어 클릭 이동이 이 캡슐을 찾아 직접 돌아간다 (ULOAClickMoveComponent의 InsertBodyBlockDetours)
//   (2026-10-20) 내비메시에 "보스 몸" 구역으로 칠하는 방식(A안)을 해 봤다가 되돌림:
//     보스가 움직일 때마다 내비메시 조각을 다시 만듦 → 보스 AI의 Move To가 길을 버리고 다시 찾음 → 부들거림 + 모든 컴퓨터에서 계속 다시 만드는 비용
//
// [사망] 주인에게 ULOADeathReactionComponent가 있으면 죽을 때 꺼지고 살아나면 켜진다. (시체에 막히지 않게)
// [숨음] 주인이 SetSuppressed(true)로 잠깐 끌 수 있다. (보스가 하늘로 사라진 구간, State.Hidden)
//   켜짐 = 살아 있고 && 숨지 않음. 둘을 따로 기억해서, 숨은 채 죽거나 죽은 채 숨음이 풀려도 서로 덮어쓰지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만 : 없음
//   2. 복제   : 없음 (충돌 설정은 복제되지 않는다)
//   3. 각자   : 모든 컴퓨터에서 같은 설정으로 생긴다 (생성자). 사망 시 끄기도 각 컴퓨터가 State.Dead를 보고 각자.
//   ※ 이 캡슐은 보스를 따라 순간이동하듯 움직여 플레이어 몸 안으로 파고들 수 있다 (밀고 오기, 위로 착지).
//     그때 빠져나오기는 플레이어 쪽 ULOACharacterMovementComponent가 한다. (겹친 동안 막지 않고 슥 밀어냄)

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "LOABodyBlockerComponent.generated.h"

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOABodyBlockerComponent : public UCapsuleComponent
{
	GENERATED_BODY()

public:
	ULOABodyBlockerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 주인이 잠깐 끄기 / 다시 켜기 (예: 보스가 하늘로 사라진 동안). 죽어 있으면 켜지 않는다. [모든 컴퓨터] */
	void SetSuppressed(bool bInSuppressed);

protected:
	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	//~ End UActorComponent Interface

	/** 주인의 루트 캡슐과 같은 크기로 맞춘다. (끄면 이 캡슐의 Radius / Half Height를 직접) */
	UPROPERTY(EditAnywhere, Category = "LOA|BodyBlocker")
	bool bMatchOwnerCapsule = true;

	/** Match Owner Capsule일 때 반지름에 더할 값 (cm). 음수면 조금 작게 (예: -10 → 보스에 조금 더 붙을 수 있음) */
	UPROPERTY(EditAnywhere, Category = "LOA|BodyBlocker", meta = (EditCondition = "bMatchOwnerCapsule", Units = "cm"))
	float ExtraRadius = 0.f;

private:
	/** 주인이 죽었다 / 살아났다 (ULOADeathReactionComponent 알림) */
	UFUNCTION()
	void OnOwnerDied(AActor* OwnerActor);

	UFUNCTION()
	void OnOwnerRevived(AActor* OwnerActor);

	/** 살아 있고 숨지 않았으면 켬, 아니면 끔 */
	void UpdateCollision();

	bool bOwnerDead = false;
	bool bSuppressed = false;
};
