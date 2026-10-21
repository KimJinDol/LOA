// LOATelegraphTypes.h
// [배관] 예고에 넘기는 값 묶음 (FLOATelegraphData). 코드 없음, 구조체만.
//
// ─────────────────────────────────────────────────────────────────────────────
// [역할]
//   예고 쪽(ULOATelegraphSubsystem, ALOATelegraph)이 알아듣는 말은 이것뿐이다.
//   모양은 판정과 같은 FLOAHitShape를 그대로 받는다. (Library/LOACombatTypes.h — 값만 담은 타입, 전투 함수는 없음)
//   스킬 모듈, 범위 액터, ULOACombatLibrary는 모른다.
//
// [FLOAHitShape에서 예고가 쓰는 칸 / 안 쓰는 칸]
//   쓰는 칸   : ShapeType, Radius, Angle, InnerRadius, Length, Width
//   안 쓰는 칸 : ForwardDistance — 부르는 쪽이 기준점(Location)에 이미 반영해서 넘긴다. (판정과 같은 GetShapeCenter 한 곳)
//                bHitByBody      — 판정 전용
//
// [Location = 모양의 기준점] (판정과 같은 규칙)
//   Sphere : 원의 중심
//   Fan    : 부채꼴의 꼭짓점. Yaw 방향으로 펼쳐진다.
//   Box    : 사각형 뒷변의 가운데. Yaw 방향으로 Length만큼 뻗는다.
//   예) 돌진: Box, 길이 800, 폭 200, Location = 보스 발밑 → 보스 앞으로 800cm 띠
//
// ─────────────────────────────────────────────────────────────────────────────
// [누가 무엇을 채우나]
//   부르는 쪽 (ALOAAreaActor) : Style, Shape, Location, Yaw
//   예고 입구 (ULOATelegraphSubsystem::ShowToAll) : TelegraphId, StartServerTime, HitServerTime
//
// [네트워크]
//   Client RPC 인자로 그대로 간다. 위치는 FVector_NetQuantize10(0.1cm 단위).
//   Style(에셋)은 포인터 그대로 보내도 받는 컴퓨터에서 같은 에셋으로 찾아진다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"   // FVector_NetQuantize10
#include "Library/LOACombatTypes.h"    // FLOAHitShape (값만 담은 타입)
#include "LOATelegraphTypes.generated.h"

class ULOATelegraphStyle;

/** 예고 한 장 */
USTRUCT()
struct FLOATelegraphData
{
	GENERATED_BODY()

	/** 예고 번호. 판정 전에 취소될 때 "이 번호를 지워라"에 쓴다. 0 = 번호 없음. (입구가 채운다) */
	UPROPERTY()
	int32 TelegraphId = 0;

	/** 겉모습 (데칼 층 머티리얼, 깊이, 풀 크기). 비어 있으면 그리지 않는다. */
	UPROPERTY()
	TObjectPtr<ULOATelegraphStyle> Style = nullptr;

	/** 모양과 크기 (판정 모양 그대로. ForwardDistance, bHitByBody는 쓰지 않는다) */
	UPROPERTY()
	FLOAHitShape Shape;

	/** 모양의 기준점 (월드, 바닥 높이). 위 [Location = 모양의 기준점] 참고 */
	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	/** 바라보는 방향 (월드 Yaw, 도). 부채꼴이 펼쳐지는 방향, 사각형이 뻗는 방향 */
	UPROPERTY()
	float Yaw = 0.f;

	/** 차오르기 시작하는 서버 시각 (초). (입구가 채운다) */
	UPROPERTY()
	double StartServerTime = 0.0;

	/** 꽉 차는 서버 시각 = 판정 시각 (초). 이 시각이 지나면 예고는 풀로 돌아간다. (입구가 채운다) */
	UPROPERTY()
	double HitServerTime = 0.0;
};
