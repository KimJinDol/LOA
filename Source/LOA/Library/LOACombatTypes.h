// LOACombatTypes.h
// [배관] 전투 관련 값 타입 모음. 여러 곳에서 같이 쓰는 구조체와 Enum만 둔다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [이 파일의 규칙] (잡동사니 통이 되지 않게)
//   - 값만 담은 구조체와 Enum만. 함수는 넣지 않는다. (판정·대상 찾기·데미지는 ULOACombatLibrary)
//   - 다른 LOA 클래스를 include하지 않는다. → 누가 include해도 전투 로직을 끌고 오지 않는다.
//   - GAS 타입(ASC, GE)을 담는 상자는 여기가 아니라 LOASkillModuleDataInfos.h.
//
// [지금 있는 것]
//   ELOAHitShapeType, FLOAHitShape : 판정 모양. 판정(ULOACombatLibrary, Shape Targeting)과 예고(ULOATelegraphSubsystem)가 같이 쓴다.
//   ELOATargetFilter               : 대상 필터 (적 / 아군 / 나 포함 아군)
//
// [왜 ULOACombatLibrary에서 꺼냈나] (2026-10-13)
//   예고 쪽이 판정 모양을 그대로 받으려면 이 타입이 필요하다. 라이브러리 헤더째 include하면 전투 함수까지 알게 된다.
//   LOACombatLibrary.h가 이 파일을 include하므로 원래 쓰던 곳은 고치지 않아도 된다.

#pragma once

#include "CoreMinimal.h"
#include "LOACombatTypes.generated.h"

/**
 * 판정 모양 종류.
 *   Sphere : 구 (중심 + 반지름)
 *   Fan    : 부채꼴 (꼭짓점 + 길이 + 전체 각도, 안쪽 반지름을 주면 도넛 부채꼴)
 *   Box    : 직사각형 (뒷변 가운데 + 앞 길이 + 좌우 폭). 기준점에서 앞으로 뻗는다.
 * 추가할 때: ① 여기에 값 추가 ② FLOAHitShape에 그 모양의 값 추가 ③ FindTargetsInShape의 switch에 case 추가
 */
UENUM(BlueprintType)
enum class ELOAHitShapeType : uint8
{
	Sphere	UMETA(DisplayName = "Sphere (구)"),
	Fan		UMETA(DisplayName = "Fan (부채꼴)"),
	Box		UMETA(DisplayName = "Box (직사각형)"),
};

/**
 * 대상 필터. 모양 안에서 "누구를" 고를지.
 *
 *   Enemy       : 적                                   (공격)
 *   Ally        : 나를 뺀 아군                          (파티원에게만 주는 버프)
 *   AllyAndSelf : 나를 포함한 아군. 나도 "범위 안일 때만" (바닥 장판 힐: 내가 장판 밖이면 못 받음)
 *
 * "범위와 상관없이 나에게도" (예: 바드 광시곡 - 아군 75%, 나 50%)는 Filter가 아니라
 * Apply 모듈을 하나 더 넣고 Targeting = Self로 만든다. 나에게 주는 수치가 다른 경우가 많아서 모듈을 나눈다.
 *
 * 편 판단은 IsEnemy / IsAlly. (지금은 임시 규칙: PlayerState 유무)
 */
UENUM(BlueprintType)
enum class ELOATargetFilter : uint8
{
	Enemy		UMETA(DisplayName = "Enemy (적)"),
	Ally		UMETA(DisplayName = "Ally (나를 뺀 아군)"),
	AllyAndSelf	UMETA(DisplayName = "Ally And Self (나를 포함한 아군)"),
};

/**
 * 판정 모양. "어디에 어떤 모양으로" 대상을 찾을지만 담는다. (값만 담은 상자. 판정 코드는 라이브러리에 있음)
 * Ability, 보스 패턴, 장판 등 누구나 들고 다니다가 ULOACombatLibrary::FindTargetsInShape에 넘긴다.
 *
 * 에디터에서는 고른 모양(ShapeType)에 필요한 값만 보인다. (EditCondition + EditConditionHides)
 *
 * 기준점 = 공격자 위치 + 앞 방향 × ForwardDistance  (GetShapeCenter)
 *   Sphere : 기준점 = 구의 중심
 *   Fan    : 기준점 = 부채꼴의 꼭짓점. 부채꼴은 공격자의 앞 방향으로 펼쳐진다.
 *   Box    : 기준점 = 직사각형 뒷변의 가운데. 직사각형은 앞 방향으로 Length만큼 뻗는다. (좌우로는 Width/2씩)
 *            가운데 두기(앞뒤로 반씩)를 원하면 ForwardDistance = -Length/2
 *
 *        (위에서 본 모습 - Box)
 *            ┌────┐  ← 앞으로 Length
 *            │    │
 *            └─●──┘  ← 기준점 (뒷변 가운데). 좌우 폭 = Width
 *              공격자
 *
 *        (위에서 본 모습 - Sphere)              (위에서 본 모습 - Fan)
 *            ○  ← 반지름 Radius                   ╲  Angle  ╱
 *            │  ForwardDistance                     ╲      ╱   ← 길이 Radius
 *            ●  공격자                               ╲ ── ╱    ← 안쪽 반지름 InnerRadius (0이면 없음)
 *                                                      ●       ← 꼭짓점 (ForwardDistance 0이면 공격자 위치)
 *
 *   평타      : Sphere, ForwardDistance 100, Radius 150                → 앞쪽 원
 *   Q         : Sphere, ForwardDistance 0,   Radius 300                → 캐릭터 중심 원
 *   앞 베기   : Fan,    ForwardDistance 0,   Radius 400, Angle 90      → 앞쪽 90도 부채꼴
 *   보스 도넛 : Fan,    ForwardDistance 0,   Radius 800, Angle 360, InnerRadius 400 → 가까이는 안전한 고리
 *   돌진 베기 : Box,    ForwardDistance 0,   Length 800, Width 200                 → 앞으로 쭉 뻗은 띠
 *
 * 판정 기준 (bHitByBody)
 *   켜짐 (기본) : 대상의 "몸통"(충돌 캡슐의 반지름)이 모양에 조금이라도 걸치면 맞는다. (플레이어 스킬 → 큰 보스도 몸이 걸치면 맞음)
 *   꺼짐        : 대상의 "발"(중심점)이 모양 안이어야 맞는다. (보스 패턴 → 예고 장판 밖에 서 있으면 안 맞음)
 *   예) 반지름 350 원, 캐릭터 몸통 반지름 34 : 켜짐이면 중심에서 384cm까지 맞음 / 꺼짐이면 350cm까지
 */
USTRUCT(BlueprintType)
struct FLOAHitShape
{
	GENERATED_BODY()

	/** 판정 모양 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape")
	ELOAHitShapeType ShapeType = ELOAHitShapeType::Sphere;

	/**
	 * 기준점을 공격자 앞쪽으로 얼마나 옮길지 (cm). 0이면 공격자 위치. 음수면 뒤쪽. 모든 모양이 쓴다.
	 * Sphere는 구의 중심, Fan은 꼭짓점, Box는 뒷변의 가운데가 이 위치가 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape")
	float ForwardDistance = 100.f;

	/**
	 * [Sphere] 구의 반지름 (cm)
	 * [Fan]    부채꼴의 길이 = 꼭짓점에서 바깥 호까지 (cm)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape",
		meta = (ClampMin = "1.0", Units = "Centimeters",
			EditCondition = "ShapeType == ELOAHitShapeType::Sphere || ShapeType == ELOAHitShapeType::Fan", EditConditionHides))
	float Radius = 150.f;

	/**
	 * [Fan] 부채꼴의 전체 각도 (도). 90 = 앞 방향 기준 왼쪽 45도 + 오른쪽 45도. 360 = 원 전체.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape",
		meta = (ClampMin = "1.0", ClampMax = "360.0", Units = "Degrees",
			EditCondition = "ShapeType == ELOAHitShapeType::Fan", EditConditionHides))
	float Angle = 90.f;

	/**
	 * [Fan] 안쪽 반지름 (cm). 꼭짓점에서 이 거리 안쪽은 맞지 않는다. (도넛 부채꼴)
	 * 0이면 안쪽 빈 곳 없음 (보통 부채꼴). Radius보다 작아야 한다. (저장 시 검사)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape",
		meta = (ClampMin = "0.0", Units = "Centimeters",
			EditCondition = "ShapeType == ELOAHitShapeType::Fan", EditConditionHides))
	float InnerRadius = 0.f;

	/** [Box] 앞 길이 (cm). 기준점(뒷변 가운데)에서 앞 방향으로 이만큼 뻗는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape",
		meta = (ClampMin = "1.0", Units = "Centimeters",
			EditCondition = "ShapeType == ELOAHitShapeType::Box", EditConditionHides))
	float Length = 800.f;

	/** [Box] 좌우 전체 폭 (cm). 앞 방향 기준 왼쪽 Width/2 + 오른쪽 Width/2 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape",
		meta = (ClampMin = "1.0", Units = "Centimeters",
			EditCondition = "ShapeType == ELOAHitShapeType::Box", EditConditionHides))
	float Width = 200.f;

	/**
	 * 몸통이 걸쳐도 맞는가. 켜짐(기본) = 몸통이 조금이라도 걸치면 맞음 / 꺼짐 = 발(중심점)이 모양 안이어야 맞음.
	 * 보스가 플레이어를 때리는 장판은 끈다. (화면의 예고 장판 밖에 서 있는데 맞는 일이 없게)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape")
	bool bHitByBody = true;
};
