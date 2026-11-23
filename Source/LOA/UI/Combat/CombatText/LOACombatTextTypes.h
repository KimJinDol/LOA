// LOACombatTextTypes.h
// [배관] 전투 글자 하나(데미지 숫자 / 회복 숫자 / 버프·디버프·상태 이상 이름)를 띄우는 데 필요한 정보 묶음.
// (2026-11-16 apply116: 옛 LOADamageNumberTypes.h의 FLOADamageNumberInfo. 회복·버프도 같은 통로로 → 이름을 "전투 글자"로)
//
// [종류] Kind 하나로 나눈다. 화면(레이어·위젯)은 하나, 모양은 WBP가 Kind를 보고 정한다 (사용자: 체력 변화 피드백 UI를 종류마다 늘리지 않는다)
//   Damage : 내가 때린 피해 숫자 (서버 → 때린 사람 PC의 ULOACombatRPCComponent → RPC)
//   Heal   : 내가 받은 회복 숫자, 들어온 양 (서버 → 받은 사람 PC의 ULOACombatRPCComponent → RPC)
//   Buff   : 내 캐릭터에 처음 붙은 버프 이름 (각자 컴퓨터. 태그는 이미 복제됨 → RPC 없음. ALOAHUD가 만든다)
//   Debuff : 내 캐릭터에 처음 붙은 디버프 이름 (Buff와 같은 길)
//   Status : 내 캐릭터에 처음 걸린 상태 이상 이름 (State.CrowdControl.Status.*, 예: 기절. Buff와 같은 길, 2026-11-16 apply117)
//            피격 이상(띄움·다운)·경직은 띄우지 않는다 (사용자)
//   DamageTaken : 내가 맞은 피해 숫자, 빨강 (서버 → 맞은 사람 PC의 ULOACombatRPCComponent → RPC. Heal과 같은 길, 2026-11-16 apply118)
//            값 순서를 지키려고 맨 뒤에 둔다 (WBP Switch 핀이 이름으로 이어지지만 저장된 값은 숫자)
//   예) 버서커 PC: 보스를 330 때림 → Damage 330 / 세라스가 300 회복 → Heal 300 / 공증 오라 → Buff, State.Buff.DamageDealt / 보스에게 900 맞음 → DamageTaken 900
//
// [RPC 크기] 이 구조체는 RPC 인자다. 상태 이름(FText)은 넣지 않고 태그만 둔다
//   → 데미지·회복 숫자에는 빈 태그(작음)만 붙는다. 이름은 레이어가 상태 정보 표(DA_StatusInfo)에서 찾는다
//
// 나중에 항목이 생기면 여기에만 추가한다. (RPC, HUD, 레이어 함수 모양은 그대로)

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"
#include "Library/LOACombatTypes.h"		// ELOADirectionalAttack (값 타입만 있는 헤더)
#include "LOACombatTextTypes.generated.h"

/** 전투 글자 종류. WBP가 이것으로 모양(색)을 정한다 */
UENUM(BlueprintType)
enum class ELOACombatTextKind : uint8
{
	Damage	UMETA(DisplayName = "피해"),
	Heal	UMETA(DisplayName = "회복"),
	Buff	UMETA(DisplayName = "버프"),
	Debuff	UMETA(DisplayName = "디버프"),
	Status	UMETA(DisplayName = "상태 이상"),
	DamageTaken	UMETA(DisplayName = "받은 피해"),
};

USTRUCT(BlueprintType)
struct FLOACombatTextInfo
{
	GENERATED_BODY()

	/** 종류 (헤더 [종류]) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	ELOACombatTextKind Kind = ELOACombatTextKind::Damage;

	/** 숫자. Damage·DamageTaken = 들어온 데미지, Heal = 들어온 회복량. Buff·Debuff·Status는 0 (옛 칸 이름 Damage, CoreRedirects) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	float Amount = 0.f;

	/** Buff·Debuff·Status: 붙은 상태 태그 (State.Buff.* / State.Debuff.* / State.CrowdControl.Status.*). 이름은 레이어가 상태 정보 표에서 찾는다. 숫자면 비어 있음 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	FGameplayTag StatusTag;

	/**
	 * 맞은 지점 정보가 있는가. (Damage만)
	 *   true  : 히트 판정이 있는 공격 → HitOffset 위치에 띄운다
	 *   false : 도트처럼 맞은 지점이 없는 데미지 → 받은 클라가 "대상 캡슐에서 내 캐릭터와 가장 가까운 점"을 계산한다
	 *   Damage가 아닌 종류는 이 칸과 상관없이 대상 캡슐 가운데 (레이어, 2026-11-16 apply117)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	bool bHasHitOffset = false;

	/**
	 * 맞은 지점 - 대상 위치 (월드 기준 상대 위치, 서버 기준으로 계산)
	 * 절대 좌표가 아니라 상대 위치로 보내는 이유:
	 * 클라의 대상 위치는 서버와 약간 어긋나 있다(지연). 클라의 대상 위치에 더해야 숫자가 대상에 붙어 보인다.
	 * NetQuantize10: 소수점 한 자리까지만 보낸다. (연출용이라 충분, 데이터 절약)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	FVector_NetQuantize10 HitOffset = FVector::ZeroVector;

	/** 백 / 헤드 어택이었나 (2026-10-25, Damage만). 숫자 옆에 "BACK ATTACK" / "HEAD ATTACK" 글자를 띄울지 WBP가 정한다. 1바이트 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	ELOADirectionalAttack DirectionalAttack = ELOADirectionalAttack::None;

	/** 치명타였나 (2026-11-13, Damage만). WBP가 숫자를 노랗고 크게 (사용자). 공격 판정 기록의 결과 그대로 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	bool bCritical = false;

	/** 약점 공격으로 백 / 헤드가 됐나 (2026-11-18 apply124, Damage만). WBP가 "BACK ATTACK" / "HEAD ATTACK" 대신 "약점 공격"을 띄운다 (사용자). 공격 판정 기록 그대로 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	bool bWeakPoint = false;

	/** 대난투 피해였나 (2026-11-18 apply125, Damage만). WBP가 색을 바꾼다: 치명타 빨강 / 아니면 옅은 빨강(분홍) (사용자) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|CombatText")
	bool bBrawlDamage = false;
};
