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
//   ELOADirectionalAttack          : 백 / 헤드 어택 결과. Effect Context에 적혀 데미지 계산과 데미지 숫자가 같이 읽는다 (2026-10-25, 11-02)
//   ELOAJustGuardMode              : 저스트 가드 구간의 방식 (없음 / 단타 / 홀딩 첫 타부터 / 홀딩 중간부터). 구간 노티파이·RPC·장부가 같이 쓴다 (2026-10-26)
//   ELOAJustGuardWindowState       : 클라에 알리는 저가 구간 상태 (열림 / 누르기 마감 / 닫힘). 구간 알림 RPC·장부가 같이 쓴다 (2026-11-04)
//   ELOAJustGuardGrade             : 협동 저가 등급 (Perfect / Great / Bad). 대난투 추가 시간·협동 저가 결과 위젯이 같이 쓴다 (2026-11-18 apply128)
//   ELOAAttackSide, FLOAAttackJudgeRecord   : 공격 방향(앞 / 옆 / 뒤), 공격 판정 기록(공격 방향 + 백·헤드 결과 + 판정 시각). Effect Context에 적힌다 (2026-11-03)
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
 *   셋 다 바닥 모양 + 높이 범위(HalfHeight). Sphere도 3D 구가 아니라 바닥 원 기둥 (2026-11-01)
 * 추가할 때: ① 여기에 값 추가 ② FLOAHitShape에 그 모양의 값 추가 ③ ULOACombatLibrary의 IsBodyInShape·후보 상자(LOAShapeSearch)·DrawDebugHitShape에 case 추가
 *           ④ 피해자 판정 요청 직렬화(FLOAHitJudgeRequestData::NetSerialize)와 예고(ALOATelegraph)
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
 * 백 / 헤드 어택 결과. 한 번의 타격이 어느 보너스를 받았나.
 *   None : 보너스 없음 (속성이 없는 스킬이거나, 속성은 있지만 방향이 안 맞음 = 옆에서 침)
 *   Back : 백 어택 스킬로 맞은 쪽 뒤에서 침
 *   Head : 헤드 어택 스킬로 맞은 쪽 앞에서 침
 * 판단 = ULOACombatLibrary::ApplyPayload 한 곳 (GetDirectionalAttack) → Effect Context(FLOAGameplayEffectContext)의 공격 판정 기록.
 * 데미지 계산(보너스)과 데미지 숫자(글자 표시)는 그 칸을 읽기만 한다 (2026-11-02).
 */
UENUM(BlueprintType)
enum class ELOADirectionalAttack : uint8
{
	None	UMETA(DisplayName = "None (보너스 없음)"),
	Back	UMETA(DisplayName = "Back (백 어택)"),
	Head	UMETA(DisplayName = "Head (헤드 어택)"),
};

/**
 * 저스트 가드 구간의 방식. 보스 몽타주의 저가 구간(LOA Just Guard Window) 칸에서 고른다. (2026-10-26)
 *   None : 막을 수 없음 (구간이 없는 공격)
 *   Tap  : 단타 — 구간 하나 = 공격 하나. 본 시각 0.5초(막기 유효 시간) 전 ~ 본 시각에 누르면 막음
 *   Hold : 홀딩 (첫 타부터) — 구간 하나 = 연타 전체. 첫 타를 제때 누르고, 그 뒤로는 누르고 있는 동안 계속 막음
 *          첫 타를 못 막으면 그 뒤 타격도 못 막는다
 *   HoldFromMiddle : 홀딩 (중간부터) — 누른 뒤 처음 닿은 타격(누른 시각 + 0.5초 안)을 기준으로, 그 뒤로는 누르고 있는 동안 계속 막음 (2026-11-04)
 *          누르기 전에 맞은 타격은 맞음. 첫 타 뒤에도 누를 수 있다 (누르기 마감을 하지 않는다)
 *          예) 4연타 10.0 / 10.3 / 10.6 / 10.9, 10.2에 누름 → 10.0 맞음, 10.3 · 10.6 · 10.9 막음
 *   Hold는 이름 그대로 둔다 (저장된 노티파이 값이 안 깨지게. 새 값은 뒤에 붙인다)
 * 구간 알림(서버 → 클라 RPC), 저가 매니저(ULOAJustGuardSubsystem), 장부(ULOAJustGuardComponent)가 같이 쓴다.
 */
UENUM()
enum class ELOAJustGuardMode : uint8
{
	None			UMETA(Hidden),
	Tap				UMETA(DisplayName = "Tap (단타)"),
	Hold			UMETA(DisplayName = "Hold (홀딩, 첫 타부터)"),
	HoldFromMiddle	UMETA(DisplayName = "Hold From Middle (홀딩, 중간부터)"),
};

/**
 * 클라에 알리는 저가 구간 상태 (서버 → 모든 플레이어, ULOACombatRPCComponent::ClientJustGuardWindow). (2026-11-04)
 *   Open        : 구간 열림. G를 누를 수 있다
 *   PressClosed : 누르기 마감 (구간의 첫 타격). 더 못 누르지만 구간은 아직 열려 있다 (홀딩 2타·3타)
 *   Closed      : 구간 끝 (노티파이 끝, 몽타주 끊김). 홀딩 중이면 저가 GA가 끝난다
 * 예) 단타   : Open → PressClosed(첫 타) → Closed
 *     홀딩   : Open → PressClosed(첫 타) → (2타·3타) → Closed
 *     중간부터: Open → Closed (마감 없음)
 *     타격 없이 끝남: Open → Closed
 */
UENUM()
enum class ELOAJustGuardWindowState : uint8
{
	Open,
	PressClosed,
	Closed,
};

/**
 * 협동 저가 등급 (2026-11-18 apply128, 등급 규칙은 11-06 사용자 결정)
 *   성공 = 그 패턴의 저가 타격을 전부 막은 사람 (피해서 모양 밖이었던 사람, 죽은 사람은 실패)
 *   성공 인원 → 등급: Project Settings → LOA Combat → Just Guard (기본 3명 Perfect / 2명 Great / 그 밖 Bad)
 *   ULOAJustGuardSubsystem::GetCoopJustGuardGrade가 정한다. 대난투 경기장(추가 시간)과 대난투 UI(글자)가 쓴다
 */
UENUM(BlueprintType)
enum class ELOAJustGuardGrade : uint8
{
	None		UMETA(Hidden),
	Perfect		UMETA(DisplayName = "Perfect"),
	Great		UMETA(DisplayName = "Great"),
	Bad			UMETA(DisplayName = "Bad"),
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
 *
 * 높이 (HalfHeight) (2026-11-01)
 *   모양은 바닥 모양이고, 높이는 따로 본다: 기준점 높이 ± HalfHeight에 몸(충돌 캡슐 위아래)이 걸치면 맞는다.
 *   → 장판 액터가 바닥에 있든(Cursor) 캡슐 중심 높이에 있든(Self, All Enemies) 결과가 같다. Spawn Offset Z를 맞출 필요 없음
 *   → 띄워진 플레이어도 이 범위 안이면 맞는다 (사용자 결정). 다른 층은 안 맞게 줄일 수 있다
 *
 * 땅에 붙이기 (bSnapToGround) (2026-11-01)
 *   켜짐(기본) : 기준점을 아래로 트레이스해서 바닥 높이로 옮긴다 (ULOACombatLibrary::GetShapeCenter)
 *               → 캡슐 중심(Self)·바닥(Cursor) 어디서 시작해도 기준점 = 발밑 바닥. 예고 데칼도 항상 바닥에 찍힌다
 *   꺼짐       : 공격자 위치 높이 그대로 (공중 보스의 공중 공격 등)
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
	 * 높이 범위 (cm). 기준점 높이에서 위아래로 각각 이만큼. 몸(충돌 캡슐)이 이 범위에 조금이라도 걸치면 맞는다. 모든 모양이 쓴다.
	 * 기본 700 = 큰 보스의 캡슐 중심에서 소환한 장판도 바닥의 플레이어·띄워진 플레이어가 맞는 정도. 위층·아래층이 가까운 맵이면 줄인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float HalfHeight = 700.f;

	/**
	 * 기준점을 바닥에 붙이나. 켜짐(기본) = 기준점 아래 바닥 높이로 옮긴다 (Spawn Offset Z를 맞출 필요 없음) / 꺼짐 = 공격자 높이 그대로.
	 * 바닥을 못 찾으면(아래 2000cm 안에 없음) 원래 높이를 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape")
	bool bSnapToGround = true;

	/**
	 * 몸통이 걸쳐도 맞는가. 켜짐(기본) = 몸통이 조금이라도 걸치면 맞음 / 꺼짐 = 발(중심점)이 모양 안이어야 맞음.
	 * 보스가 플레이어를 때리는 장판은 끈다. (화면의 예고 장판 밖에 서 있는데 맞는 일이 없게)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOA|HitShape")
	bool bHitByBody = true;
};

/**
 * 공격 방향. 때린 사람이 맞은 쪽의 앞 / 옆 / 뒤 중 어디에 서 있었나. (바닥 기준) (2026-11-03)
 *   Front : 맞은 쪽이 보는 방향 ±반각 안      Back : 등 뒤 ±반각 안      Side : 그 사이
 *   None  : 모름 (때린 사람과 맞은 쪽이 같은 자리에 겹쳐 서 있음)
 *   반각 = Project Settings → LOA Combat → Directional Attack Half Angle (헤드 / 백 어택과 같은 범위)
 * 스킬 속성과 상관없이 항상 정한다. 백 / 헤드 보너스(ELOADirectionalAttack)는 이것 + 스킬 속성으로 정한다.
 * 읽는 곳: 카운터 (Front여야 성공). 나중에 "특정 방향에서 쳐서 쌓는" 패턴 (로아 암바 같은)
 */
UENUM(BlueprintType)
enum class ELOAAttackSide : uint8
{
	None	UMETA(DisplayName = "None (모름)"),
	Front	UMETA(DisplayName = "Front (앞)"),
	Side	UMETA(DisplayName = "Side (옆)"),
	Back	UMETA(DisplayName = "Back (뒤)"),
};

/**
 * 공격 판정 기록. 이 타격을 판정한 순간을 찍어 둔 것. (값만 담은 상자) (2026-11-03)
 * 적는 곳: ULOACombatLibrary::ApplyPayload(데미지 GE의 쪽지), SendHitEvent("맞았다" 신호의 쪽지).
 *   (2026-11-13) 치명타는 무작위라 대상마다 ApplyPayloads가 한 번 정해서 둘에 같은 기록을 넘긴다.
 * 계산은 적는 곳에서 한 번만. 읽는 쪽은 계산하지 않는다.
 *   예) B 화면에서 1.28초, 보스 정면에서 헤드 어택 스킬로 침 (서버 도착 1.38초)
 *       → AttackSide = Front, DirectionalAttack = Head, ServerTime = 1.28
 *       데미지 계산: Head → +20% / 데미지 숫자: "HEAD" / 카운터: Front + 1.28초에 파란 구간이었나
 */
USTRUCT(BlueprintType)
struct FLOAAttackJudgeRecord
{
	GENERATED_BODY()

	/** 공격 방향 (앞 / 옆 / 뒤). 공격자 판정이면 보고한 내 자리 + 되감은 보스 자리·방향으로, 아니면 지금 자리로 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Combat")
	ELOAAttackSide AttackSide = ELOAAttackSide::None;

	/** 백 / 헤드 어택 결과 (보너스). 공격 방향 + 스킬 속성(Skill.Attribute.BackAttack / HeadAttack) */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Combat")
	ELOADirectionalAttack DirectionalAttack = ELOADirectionalAttack::None;

	/**
	 * 판정 기준 서버 시각 (초, 서버 월드 시각)
	 *   공격자 판정 : 공격한 사람 화면이 보여 주던 서버 시각 (보고의 ViewServerTime. 되감기 한도로 자른 값)
	 *   그 외       : 판정한 그 순간 (호스트, 서버 판정, 장판·투사체)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Combat")
	double ServerTime = 0.0;

	/**
	 * 치명타였나 (2026-11-13). 플레이어가 적을 때릴 때만 굴린다 (보스는 치명타 개념 없음, 회복·보호막도 없음 = 사용자)
	 *   확률 = 기본(LOA Combat 설정) + 때린 쪽 ExtraCritRate − 맞은 쪽 CritResistance + 백 어택이면 백 어택 치명타 (0~1로 자름)
	 *   데미지 계산: 치명타면 × (기본 치명타 피해 + 때린 쪽 ExtraCritDamage) / 데미지 숫자: 노랗고 크게
	 * 대상 하나에 한 번만 굴린다 (그 대상에게 가는 GE들과 "맞았다" 신호가 같은 결과를 본다)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Combat")
	bool bCritical = false;

	/**
	 * 약점 공격으로 백 / 헤드가 됐나 (2026-11-18 apply124). 맞은 쪽에 State.Debuff.WeakPoint가 있고 DirectionalAttack이 None이 아님
	 *   데미지 계산·치명타는 DirectionalAttack만 본다 (보너스는 백 / 헤드와 똑같음). 이 칸은 데미지 숫자 글자("약점 공격")만 읽는다
	 *   예) 대난투 중 보스 정면에서 백 어택 스킬 → AttackSide = Front, DirectionalAttack = Back, bWeakPoint = true
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LOA|Combat")
	bool bWeakPoint = false;
};
