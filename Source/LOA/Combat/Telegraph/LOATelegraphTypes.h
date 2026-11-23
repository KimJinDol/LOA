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
//                bHitByBody, HalfHeight — 판정 전용 (데칼 깊이는 Style의 Projection Depth)
//
// [Location = 모양의 기준점] (판정과 같은 규칙)
//   Sphere : 원의 중심
//   Fan    : 부채꼴의 꼭짓점. Yaw 방향으로 펼쳐진다.
//   Box    : 사각형 뒷변의 가운데. Yaw 방향으로 Length만큼 뻗는다.
//   예) 돌진: Box, 길이 800, 폭 200, Location = 보스 발밑 → 보스 앞으로 800cm 띠
//
// ─────────────────────────────────────────────────────────────────────────────
// [누가 무엇을 채우나]
//   부르는 쪽 (ALOAAreaActor) : Style, Shape, Location, Yaw, bSyncWithBossMotion, Attacker, AttackTags, CrowdControlEffect
//   예고 입구 (ULOATelegraphSubsystem::ShowToAll) : TelegraphId, StartServerTime, FillServerTime, HitServerTime
//
// [네트워크]
//   Client RPC 인자로 그대로 간다. 위치는 FVector_NetQuantize10(0.1cm 단위).
//   Style(에셋)은 포인터 그대로 보내도 받는 컴퓨터에서 같은 에셋으로 찾아진다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"   // FVector_NetQuantize10
#include "Library/LOACombatTypes.h"    // FLOAHitShape (값만 담은 타입)
#include "Templates/SubclassOf.h"
#include "GameplayTagContainer.h"
#include "LOATelegraphTypes.generated.h"

class ULOATelegraphStyle;
class UGameplayEffect;
class AActor;

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

	/**
	 * 꽉 차는 서버 시각 (초). 판정 시각과 같거나 앞 (꽉 찬 뒤 일정 시간 후 터지는 장판). 머티리얼의 HitTime 칸에 들어간다. (입구가 채운다)
	 *   예) 100.0 시작, 101.2 꽉 참, 101.7 판정 → 101.2 ~ 101.7 꽉 찬 채로 남음
	 */
	UPROPERTY()
	double FillServerTime = 0.0;

	/** 판정 시각 (초). 이 시각이 지나면 예고는 풀로 돌아가고 OnTelegraphHit이 울린다. (입구가 채운다) */
	UPROPERTY()
	double HitServerTime = 0.0;

	/**
	 * true = 보스 모션에 맞춰 그린다 (앞당기지 않고, 보스 모션이 늦게 보이는 만큼 늦춘다). (부르는 쪽이 채운다)
	 * 빨간 구역만 보여 주고 보스 모션을 보고 피하는 장판 (장판 액터 Hit Timing = Animation). ULOATelegraphSubsystem [보스 모션에 맞춰 그리기]
	 */
	UPROPERTY()
	bool bSyncWithBossMotion = false;

	/**
	 * 공격한 보스 (Context.Caster). 비어 있을 수 있다. (부르는 쪽이 채운다. 예고가 꽉 차는 장판만) (2026-11-04)
	 * 판정 순간 맞은 사람 컴퓨터의 저가 컴포넌트가 이 보스의 구간을 찾아 먼저 성공 모션을 보여 준다
	 * (ULOAServerJudgeSubsystem::JudgeTelegraphLocal → 판정 매니저 ReactLocal → ULOAJustGuardComponent::TryJustGuard)
	 * 예전 칸 bJustGuardable 대신. 저가 공격인지는 저가 컴포넌트가 공격 속성으로 본다
	 */
	UPROPERTY()
	TObjectPtr<AActor> Attacker = nullptr;

	/** 공격 속성 (Context.AttackTags. 예: Skill.Attribute.JustGuard). (부르는 쪽이 채운다. 예고가 꽉 차는 장판만) (2026-11-04) */
	UPROPERTY()
	FGameplayTagContainer AttackTags;

	/**
	 * 이 공격의 CC GE (경직·띄움 등). 없으면 비움. (부르는 쪽이 채운다. 예고가 꽉 차는 장판만) (2026-11-01)
	 * 판정 순간 맞은 사람 컴퓨터가 내 몸이 모양 안이면 서버 CC를 기다리지 않고 먼저 반응한다
	 * (ULOAServerJudgeSubsystem::JudgeTelegraphLocal → 판정 매니저 ReactLocal → ULOACrowdControlPredictionComponent)
	 */
	UPROPERTY()
	TSubclassOf<UGameplayEffect> CrowdControlEffect;
};
