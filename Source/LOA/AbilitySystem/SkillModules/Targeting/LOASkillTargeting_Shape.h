// LOASkillTargeting_Shape.h
// [배관] Shape Targeting. "기준 액터(Context.SourceActor) 주변 모양 안에서, 시전자(Context.Caster) 기준 Filter에 맞는 대상"을 찾는다.
//   기준 액터 = 캐릭터 (Apply 모듈) 또는 소환 액터 자신 (장판/낫 같은 ALOAAreaActor)
// (기존 LOAGA_ShapeAttack의 Shape / HitObjectType / bShowHitRange 칸을 옮기고 Filter를 더한 것)
//
// ─────────────────────────────────────────────────────────────────────────────
// [디테일 패널]
//
//   ▼ Targeting  [ Shape ▼ ]
//        Shape
//           Shape Type        [ Sphere ▼ ]   Sphere(구) / Fan(부채꼴) / Box(직사각형)
//           Forward Distance  0              ← 기준점 (Sphere = 중심, Fan = 꼭짓점, Box = 뒷변 가운데)
//           Radius            300            ← Sphere = 반지름, Fan = 길이
//           Angle             90             ← Fan만: 전체 각도
//           Inner Radius      0              ← Fan만: 안쪽 빈 곳 (도넛)
//           Length / Width    800 / 200      ← Box만: 앞 길이 / 좌우 폭
//        Filter        [ Enemy ▼ ]     Enemy / Ally(나 제외) / Ally And Self(나 포함, 범위 안일 때만)
//        Hit Judge     [ Server ▼ ]    누가 판정하나 (플레이어가 맞을 때만 차이). Server / Victim. 아래 [판정 방식]
//        Object Type   Pawn
//        Show Range    ☐
//
//   평타        : Sphere, Forward 100, Radius 150, Enemy        → 앞쪽 원 안의 적
//   휠윈드      : Sphere, Forward 0,   Radius 300, Enemy        → 내 주변 원 안의 적
//   앞 베기     : Fan,    Forward 0,   Radius 400, Angle 90     → 앞쪽 90도 부채꼴 안의 적
//   돌진 베기   : Box,    Forward 0,   Length 800, Width 200    → 앞으로 뻗은 띠 안의 적
//   주변 아군 힐 : Sphere, Forward 0,   Radius 500, AllyAndSelf  → 내 주변 원 안의 아군 + 나
//
//   "범위와 상관없이 나에게도"(광시곡: 아군 75%, 나 50%)는 이 Filter가 아니라
//   Apply 모듈을 하나 더 넣고 Targeting = Self로 만든다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (판정 코드는 ULOACombatLibrary에 있다. 여기는 순서만)
//   ① FindTargetsInShape(기준 액터, Shape, Filter)   → 모양 안의 대상
//   ② 대상마다 MakeClosestHitResult(대상, 기준점)    → 맞은 지점 (대상 캡슐 표면에서 기준점과 가장 가까운 점)
//      기준점 = Sphere의 중심 / Fan의 꼭짓점 / Box의 뒷변 가운데
//   ③ OutHits에 추가
//
// [판정 방식] Hit Judge
//   Server (기본) : 서버가 서버의 위치로. 즉사기·파티 전멸기·기믹 실패
//   Victim        : 맞는 플레이어의 컴퓨터가 자기 화면 위치로. 일반·빠른 패턴 ("내 화면에서 피했으면 안 맞음")
//   이 칸을 읽고 실제로 판정하는 건 ULOAHitJudgeSubsystem. 부르는 쪽(Apply 모듈, 장판 액터)은 판정 방식을 모른다.
//   몬스터·허수아비·리슨 호스트 본인은 Victim이어도 서버가 바로 판정한다. (원격 플레이어만 보고를 받는다)
//
// [커서 지점 범위 공격]
//   이 Targeting에 "커서 기준" 옵션은 없다. 커서 지점에 판정하려면
//   Spawn 모듈(Cursor)로 ALOAAreaActor를 소환하고, 그 액터의 Targeting에 이 Shape를 넣는다. (예: 길로틴 스윙)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/Targeting/LOASkillTargeting.h"
#include "Engine/EngineTypes.h"
#include "Library/LOACombatLibrary.h"   // FLOAHitShape, ELOATargetFilter를 값으로 가지므로 include
#include "LOASkillTargeting_Shape.generated.h"

/** 누가 판정하나. 플레이어가 맞을 때만 차이가 난다. (몬스터·허수아비는 항상 서버 판정) */
UENUM()
enum class ELOAHitJudgeMode : uint8
{
	/** 서버가 서버의 위치로 판정. 즉사기·기믹용 (조작된 클라가 피할 수 없다) */
	Server,
	/** 맞는 플레이어의 컴퓨터가 자기 화면 위치로 판정해 보고. 빠른 패턴용 ("내 화면에서 피했으면 안 맞음") */
	Victim,
};

UCLASS(meta = (DisplayName = "Shape"))
class LOA_API ULOASkillTargeting_Shape : public ULOASkillTargeting
{
	GENERATED_BODY()

public:
	//~ Begin ULOASkillTargeting Interface
	virtual void FindTargets(const FLOASkillModuleContext& Context, TArray<FLOATargetHit>& OutHits) const override;

#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사: Fan의 Inner Radius가 Radius보다 작은지 */
	virtual EDataValidationResult ValidateTargeting(class FDataValidationContext& Context, const FString& Prefix) const override;
#endif
	//~ End ULOASkillTargeting Interface

	/** 판정 모양. 범위 액터(ALOAAreaActor)가 같은 모양으로 예고를 띄우려고 읽는다. */
	const FLOAHitShape& GetShape() const { return Shape; }

	/** 누구를 고르나. 판정 담당이 피해자 판정 때 "보고를 받을 플레이어"를 고르려고 읽는다. (예: Enemy → 시전자의 적만) */
	ELOATargetFilter GetFilter() const { return Filter; }

	/** 누가 판정하나. 판정 담당(ULOAHitJudgeSuGbsystem)이 읽는다. */
	ELOAHitJudgeMode GetHitJudge() const { return HitJudge; }

protected:
	/** 판정 모양. 모양을 고르면 그 모양에 필요한 값만 보인다. (기준점 = 기준 액터 위치 + 앞 방향 × ForwardDistance) */
	UPROPERTY(EditDefaultsOnly, Category = "Shape")
	FLOAHitShape Shape;

	/** 누구를 고를지. 기본은 적. (Ally And Self의 "나"는 모양 안에 있을 때만 포함된다) */
	UPROPERTY(EditDefaultsOnly, Category = "Shape")
	ELOATargetFilter Filter = ELOATargetFilter::Enemy;

	/** 찾을 물체 종류. 허수아비/몬스터/보스/플레이어 몸통은 Pawn */
	UPROPERTY(EditDefaultsOnly, Category = "Shape")
	TEnumAsByte<ECollisionChannel> ObjectType = ECC_Pawn;

	/**
	 * 누가 판정하나. 플레이어가 맞을 때만 차이가 난다. (몬스터·허수아비는 항상 서버)
	 *   Server (기본) : 즉사기·기믹. 지연만큼 늦은 위치로 판정해서 내 화면에서 피했어도 맞을 수 있다.
	 *   Victim        : 일반·빠른 패턴. 맞는 플레이어 화면 기준. 보고가 1초 안에 안 오면 서버 판정 결과를 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Shape")
	ELOAHitJudgeMode HitJudge = ELOAHitJudgeMode::Server;

	/** 범위를 화면에 1초간 그릴지 (범위 조정용) */
	UPROPERTY(EditDefaultsOnly, Category = "Shape")
	bool bShowRange = false;
};
