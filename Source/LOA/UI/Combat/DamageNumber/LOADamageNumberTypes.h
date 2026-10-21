// LOADamageNumberTypes.h
// [배관] 데미지 숫자 한 개를 띄우는 데 필요한 정보 묶음.
//
// 서버(ULOACombatLibrary::NotifyDamageDealt)가 만들어 RPC로 보내고,
// PlayerController → HUD → 데미지 숫자 레이어까지 이 구조체 하나로 전달된다.
// 나중에 치명타, 백 어택 같은 항목이 생기면 여기에만 추가한다. (RPC, HUD, 레이어 함수 모양은 그대로)

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "LOADamageNumberTypes.generated.h"

USTRUCT(BlueprintType)
struct FLOADamageNumberInfo
{
	GENERATED_BODY()

	/** 확정된 데미지 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|DamageNumber")
	float Damage = 0.f;

	/**
	 * 맞은 지점 정보가 있는가.
	 *   true  : 히트 판정이 있는 공격 → HitOffset 위치에 띄운다
	 *   false : 도트처럼 맞은 지점이 없는 데미지 → 받은 클라가 "대상 캡슐에서 내 캐릭터와 가장 가까운 점"을 계산한다
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|DamageNumber")
	bool bHasHitOffset = false;

	/**
	 * 맞은 지점 - 대상 위치 (월드 기준 상대 위치, 서버 기준으로 계산)
	 * 절대 좌표가 아니라 상대 위치로 보내는 이유:
	 * 클라의 대상 위치는 서버와 약간 어긋나 있다(지연). 클라의 대상 위치에 더해야 숫자가 대상에 붙어 보인다.
	 * NetQuantize10: 소수점 한 자리까지만 보낸다. (연출용이라 충분, 데이터 절약)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|DamageNumber")
	FVector_NetQuantize10 HitOffset = FVector::ZeroVector;
};
