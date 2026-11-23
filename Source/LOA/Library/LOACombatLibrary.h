// LOACombatLibrary.h
// [배관] 전투 함수 모음. 판정 모양, 대상 찾기(적/아군 필터), 편 판단, 효과(데미지/힐/버프) 주기.
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 라이브러리(static 함수 모음)인가]
// 데미지를 주는 쪽은 Ability만이 아니다.
//   Ability(평타, 스킬) / 투사체 / 장판 / 설치물 / 보스 패턴 …
// 투사체나 장판은 Ability가 끝난 뒤에도 남아서 데미지를 줘야 하고, 액터라서 Ability를 상속할 수 없다.
// 그래서 "누구나 부를 수 있는" static 함수로 둔다. (엔진의 UGameplayStatics::ApplyDamage와 같은 방식)
//
// 대신 static 함수는 "주인"이 없어서 "누가 때리는지"를 알 수 없다.
// → 공격자, 공격자의 ASC, 레벨 같은 정보는 전부 매개변수로 받는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [맞은 지점 → 데미지 숫자 위치]
//   히트 판정 공격 : MakeClosestHitResult → ApplyPayload(맞은 지점 있음) → GE Context의 HitResult
//                    → AttributeSet → 전투 알림판 → 때린 플레이어 PC의 ULOACombatRPCComponent가 "대상 기준 상대 위치"로 바꿔 RPC (2026-11-15)
//   도트 등        : ApplyPayload(맞은 지점 없음) → RPC에 "맞은 지점 없음"
//                    → 받은 클라가 GetClosestPointOnTarget(대상, 내 캐릭터 위치)로 계산 (연출이라 클라 담당)
//
// ─────────────────────────────────────────────────────────────────────────────
// [주는 쪽 / 받는 쪽]
//   주는 쪽 : 이 라이브러리의 ApplyPayload → GE에 Payload의 값(예: SetByCaller.DamageRate)을 넣어 대상 ASC에 적용
//   계산    : ULOADamageExecution → 공격자 공격력 × DamageRate → 적의 Damage(메타)
//   받는 쪽 : 적의 AttributeSet(PostGameplayEffectExecute) → Damage를 Health 감소로 확정, 0 아래로 안 내려감
//            (GAS에서는 AttributeSet이 엔진의 TakeDamage 역할을 한다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [백 / 헤드 어택] (사용자 결정 2026-10-25)
//   이 속성(Skill.Attribute.BackAttack / HeadAttack)이 붙은 스킬만, 때린 사람이 맞은 쪽 뒤 / 앞에 서 있으면 보너스.
//   예) 공격력 100 × 스킬 3.0 = 300 → 뒤에서 백 어택 스킬 300 × 1.05 = 315 / 앞에서 헤드 어택 스킬 300 × 1.20 = 360 / 옆 300
//   ApplyPayload : 공격 속성(Context.AttackTags)을 GE Spec 태그로도 붙인다 (지금까지는 Event.Combat.Hit에만 실렸다)
//   (2026-11-02) 방향을 정하는 곳 = ApplyPayload 한 곳 → 결과를 Effect Context(FLOAGameplayEffectContext)의 칸에 적는다
//   (2026-11-03) 그 칸 = 공격 판정 기록(FLOAAttackJudgeRecord: 맞은 쪽 앞/옆/뒤 + 백·헤드 결과 + 판정 시각). "맞았다" 신호에도 같은 기록이 간다 (카운터)
//     자리 = 대상(FLOATargetHit)의 "방향을 정할 자리"가 있으면 그것(공격자 판정 = 내 화면에서 본 보스), 없으면 지금 자리
//   ULOADamageExecution : 쪽지의 결과를 읽어 (설정의 기본 보너스 + 공격자 Combat 스탯의 추가분)을 곱함. 각도 계산은 안 한다
//   AttributeSet → 전투 알림판 → ULOACombatRPCComponent : 같은 쪽지의 결과를 데미지 숫자 정보에 실어 보냄 (글자 표시)
//   두 곳이 같은 칸을 읽으므로 결과가 갈리지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [치명타] (사용자 결정 2026-11-13)
//   플레이어가 적을 때릴 때만 굴린다 (보스는 치명타 개념 없음, 회복·보호막 없음). 굴리는 곳 = 공격 판정 기록(DecideAttackJudgeRecord) 한 곳
//   확률 = 기본 70%(LOA Combat 설정) + 때린 쪽 ExtraCritRate(Player) − 맞은 쪽 CritResistance(Combat) + 백 어택이면 +10%p → 0~1로 자름
//   예) 보스 치명타 저항 0, 약점 노출(버서커 시너지)로 −0.1 → 70% + 10% = 80%. 그 뒤 데미지 계산이 × (기본 200% + ExtraCritDamage), 데미지 숫자는 노랗고 크게
//   무작위라 같은 대상에게 두 번 정하면 결과가 갈린다 → ApplyPayloads가 대상마다 한 번 정해서 그 대상의 모든 GE와 "맞았다" 신호에 같은 기록을 넘긴다
//
// ─────────────────────────────────────────────────────────────────────────────
// [서버 전용]
// 명중 판정과 데미지는 서버가 정한다. 클라이언트에서 부르면 아무 일도 하지 않는다. (ApplyPayload)
// 예외: GetClosestPointOnTarget은 계산만 하는 함수라 클라도 쓴다. (데미지 숫자 위치)
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서도 사용 가능]
// UFUNCTION(BlueprintCallable)이라 BP 노드로도 나온다. (예: BP로 만든 장판 액터)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "GameplayEffectTypes.h"                                // FActiveGameplayEffectHandle (ApplyStateTags)
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // FLOASkillModuleContext, FLOATargetHit, FLOAEffectPayload
#include "Library/LOACombatTypes.h"                             // ELOAHitShapeType, ELOATargetFilter, FLOAHitShape (값만 담은 타입)
#include "LOACombatLibrary.generated.h"

struct FGameplayEffectContextHandle;

UCLASS()
class LOA_API ULOACombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 판정 모양의 기준점 = 공격자 위치 + 앞 방향 × ForwardDistance (Shape.bSnapToGround면 그 아래 바닥 높이로. FindGroundLocation)
	 * (Sphere = 원의 중심, Fan = 부채꼴의 꼭짓점, Box = 직사각형 뒷변의 가운데)
	 * FindTargetsInShape, 피해자 판정 요청, 예고 자리, 맞은 지점(MakeShapeHitResult의 기준점)이 같이 쓴다.
	 * [서버] 바닥 트레이스는 서버가 한다. 클라는 서버가 계산해 보낸 값을 쓴다 (피해자 판정 요청, 예고)
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static FVector GetShapeCenter(const AActor* Attacker, const FLOAHitShape& Shape);

	/**
	 * GetShapeCenter와 같은 규칙을 "그 자리·그 앞 방향"으로 (2026-11-02). 액터의 지금 자리가 아니라 정해 준 자리로 계산한다.
	 * GetShapeCenter도 이 함수를 부른다 (규칙은 한 곳).
	 * 쓰는 곳: 공격자 판정 서버 검사 (보고한 내 자리·방향으로 모양을 놓는다. ULOAAttackJudgeSubsystem::JudgeReport)
	 * @param IgnoreActor  바닥 트레이스에서 뺄 액터 (보통 공격자)
	 */
	static FVector GetShapeCenterAt(const UWorld* World, const FVector& AttackerLocation, const FVector& AttackerForward,
		const FLOAHitShape& Shape, const AActor* IgnoreActor);

	/**
	 * Location 아래 바닥 (2026-11-01). 위 300cm ~ 아래 2000cm를 아래로 트레이스, Ground 채널만 본다 (2026-11-14. Floor 프로필 바닥만 맞음, 프랍·캡슐·벽은 무시. LOACollisionChannels.h [Ground]).
	 * 찾으면 바닥 높이로 바꾼 위치를 OutGround에 넣고 true / 못 찾으면 OutGround = Location, false.
	 * 쓰는 곳: GetShapeCenter (bSnapToGround), 장판 액터 땅에 붙이기 (ALOAAreaActor::bSnapToGround)
	 * @param IgnoreActor  트레이스에서 뺄 액터 (보통 자기 자신. 없으면 nullptr)
	 */
	static bool FindGroundLocation(const UWorld* World, const FVector& Location, FVector& OutGround, const AActor* IgnoreActor = nullptr);

	/**
	 * From에서 Forward 방향으로 볼 때 Point가 정면에서 몇 도 벌어져 있나 (0 = 정면, 90 = 옆, 180 = 등 뒤). 바닥 기준(높이 무시) (2026-10-27)
	 * Point가 From과 같은 자리면 0 (방향 없음 → 정면으로 친다)
	 * 쓰는 곳: 저스트 가드 방향 (≤ 45°면 막음). 나중에 메두사 (보스를 보고 있나)
	 */
	static float GetFacingAngleTo(const FVector& From, const FVector& Forward, const FVector& Point);

	/**
	 * 모양(FLOAHitShape) 안에서 Filter에 맞는 대상을 모은다. [서버]
	 * 기준점 계산(기준 액터 위치 + 앞 방향 × ForwardDistance)을 여기서 한다. 방향은 기준 액터의 앞 방향.
	 * → 부르는 쪽(Targeting, 보스 패턴 …)은 모양 구조체와 Filter만 넘기면 된다.
	 *
	 * [판정] 모든 모양 = 바닥 모양 + 높이 (2026-11-01. 전에는 3D 구로 후보를 골라 기준점 높이가 어긋나면 가장자리가 안 맞았다)
	 *   기준점 = GetShapeCenter (bSnapToGround면 바닥)
	 *   ① 후보: 모양을 덮는 위아래로 긴 상자로 겹침 검색 (높이 = 기준점 ± HalfHeight)
	 *   ② 후보마다 IsTargetable / PassesFilter / IsBodyInShape (바닥 모양 + 높이). 피해자 판정(클라)도 같은 IsBodyInShape
	 *   → 장판 액터가 바닥에 있든 캡슐 중심 높이에 있든 결과가 같다 (Spawn Offset Z를 맞출 필요 없음)
	 *
	 * @param Source      범위 기준 액터. 기준점과 방향의 기준 (캐릭터, 또는 소환 액터 자신)
	 * @param Shape       판정 모양
	 * @param Filter      누구를 고를지 (적 / 나를 뺀 아군 / 나를 포함한 아군)
	 * @param ObjectType  찾을 물체 종류. 플레이어/허수아비/보스 몸통은 Pawn
	 * @param OutTargets  찾은 대상을 여기에 "추가"한다.
	 * @param bShowRange  true면 범위를 1초간 그린다.
	 * @param Caster      편 판단 기준 ("나"). 비우면(nullptr) Source를 쓴다. 소환 액터가 부를 때는 시전자를 넣는다.
	 * @return 이번에 새로 찾은 대상의 수
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static int32 FindTargetsInShape(
		const AActor* Source,
		const FLOAHitShape& Shape,
		ELOATargetFilter Filter,
		TEnumAsByte<ECollisionChannel> ObjectType,
		TArray<AActor*>& OutTargets,
		bool bShowRange = false,
		const AActor* Caster = nullptr);

	/**
	 * 원(Sphere 모양) 안에서 Filter에 맞는 대상을 모은다. [서버] 판정은 FindTargetsInShape [판정]과 같다 (높이 = FLOAHitShape 기본값)
	 * 보통은 FindTargetsInShape를 쓴다. 중심을 직접 정해야 할 때(장판의 커서 위치, 보스 패턴의 특정 지점 등) 이 함수를 쓴다.
	 * 제외: ASC가 없는 대상(벽, 소품) / Filter에 안 맞는 대상 / 중복
	 * (나 자신도 겹침 검사에는 포함된다. 뺄지 말지는 Filter가 정한다)
	 *
	 * @param Source      범위 기준 액터 (검색할 월드를 여기서 얻는다. Caster가 비었으면 편 판단 기준도 된다)
	 * @param Center      원의 중심 (월드 좌표)
	 * @param Radius      원의 반지름 (cm)
	 * @param Filter      누구를 고를지
	 * @param ObjectType  찾을 물체 종류. 플레이어/허수아비/보스 몸통은 Pawn
	 * @param OutTargets  찾은 대상을 여기에 "추가"한다. (이미 들어 있는 것과 겹치면 넣지 않음)
	 * @param bShowRange  true면 범위를 1초간 그린다. (빨강: 이번에 새로 찾음 / 초록: 없음)
	 * @param Caster      편 판단 기준 ("나"). 비우면(nullptr) Source를 쓴다.
	 * @param bHitByBody  true(기본) = 몸통이 원에 걸치면 맞음 / false = 발(중심점)이 원 안(바닥 거리)이어야 맞음
	 * @return 이번에 새로 찾은 대상의 수
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static int32 FindTargetsInSphere(
		const AActor* Source,
		FVector Center,
		float Radius,
		ELOATargetFilter Filter,
		TEnumAsByte<ECollisionChannel> ObjectType,
		TArray<AActor*>& OutTargets,
		bool bShowRange = false,
		const AActor* Caster = nullptr,
		bool bHitByBody = true);

	/**
	 * 부채꼴 범위 안에서 Filter에 맞는 대상을 모은다. [서버]
	 * 보통은 FindTargetsInShape를 쓴다. 꼭짓점이나 방향을 직접 정해야 할 때(보스 패턴이 특정 방향으로 쓸기 등) 이 함수를 쓴다.
	 *
	 * 판정은 FindTargetsInShape [판정]과 같다 (높이 = FLOAHitShape 기본값). 바닥 모양:
	 *   후보의 몸통 원(충돌 캡슐 반지름)이 부채꼴에 조금이라도 걸치면 맞음.
	 *      중심점만 보면 큰 보스는 몸이 걸쳐도 중심이 밖이라 안 맞는다. → 몸통 기준으로 판정한다.
	 *
	 * @param Source       범위 기준 액터 (검색할 월드를 여기서 얻는다. Caster가 비었으면 편 판단 기준도 된다)
	 * @param Apex         꼭짓점 (월드 좌표)
	 * @param Direction    부채꼴이 펼쳐지는 방향. 높이 성분은 무시한다.
	 * @param Radius       길이 = 꼭짓점에서 바깥 호까지 (cm)
	 * @param InnerRadius  안쪽 반지름 (cm). 이 안쪽은 맞지 않음. 0이면 없음. Radius 이상이면 아무도 안 맞음
	 * @param Angle        전체 각도 (도). 90 = 좌우 45도씩. 360 = 원 전체
	 * @param Filter       누구를 고를지
	 * @param ObjectType   찾을 물체 종류. 플레이어/허수아비/보스 몸통은 Pawn
	 * @param OutTargets   찾은 대상을 여기에 "추가"한다. (이미 들어 있는 것과 겹치면 넣지 않음)
	 * @param bShowRange   true면 범위를 1초간 그린다. (빨강: 이번에 새로 찾음 / 초록: 없음)
	 * @param Caster       편 판단 기준 ("나"). 비우면(nullptr) Source를 쓴다.
	 * @param bHitByBody   true(기본) = 몸통 원이 걸치면 맞음 / false = 발(중심점)이 부채꼴 안이어야 맞음
	 * @return 이번에 새로 찾은 대상의 수
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static int32 FindTargetsInFan(
		const AActor* Source,
		FVector Apex,
		FVector Direction,
		float Radius,
		float InnerRadius,
		float Angle,
		ELOATargetFilter Filter,
		TEnumAsByte<ECollisionChannel> ObjectType,
		TArray<AActor*>& OutTargets,
		bool bShowRange = false,
		const AActor* Caster = nullptr,
		bool bHitByBody = true);

	/**
	 * 직사각형 범위 안에서 Filter에 맞는 대상을 모은다. [서버]
	 * 보통은 FindTargetsInShape를 쓴다. 시작점이나 방향을 직접 정해야 할 때(보스 패턴의 직선 장판 등) 이 함수를 쓴다.
	 *
	 * 판정은 FindTargetsInShape [판정]과 같다 (높이 = FLOAHitShape 기본값). 바닥 모양:
	 *   후보의 몸통 원(충돌 캡슐 반지름)이 직사각형에 조금이라도 걸치면 맞음.
	 *      대상 위치를 직사각형 기준 좌표(앞 / 옆)로 바꾸고(내적), 각 좌표를 직사각형 범위로 잘라(Clamp)
	 *      "직사각형 안에서 대상 중심과 가장 가까운 점"을 구한다. 그 점까지 거리 ≤ 몸통 반지름이면 맞음.
	 *
	 * @param Source      범위 기준 액터 (검색할 월드를 여기서 얻는다. Caster가 비었으면 편 판단 기준도 된다)
	 * @param Start       뒷변의 가운데 (월드 좌표)
	 * @param Direction   직사각형이 뻗는 방향. 높이 성분은 무시한다.
	 * @param Length      앞 길이 (cm)
	 * @param Width       좌우 전체 폭 (cm)
	 * @param Filter      누구를 고를지
	 * @param ObjectType  찾을 물체 종류. 플레이어/허수아비/보스 몸통은 Pawn
	 * @param OutTargets  찾은 대상을 여기에 "추가"한다. (이미 들어 있는 것과 겹치면 넣지 않음)
	 * @param bShowRange  true면 범위를 1초간 그린다. (빨강: 이번에 새로 찾음 / 초록: 없음)
	 * @param Caster      편 판단 기준 ("나"). 비우면(nullptr) Source를 쓴다.
	 * @param bHitByBody  true(기본) = 몸통 원이 걸치면 맞음 / false = 발(중심점)이 직사각형 안이어야 맞음
	 * @return 이번에 새로 찾은 대상의 수
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static int32 FindTargetsInBox(
		const AActor* Source,
		FVector Start,
		FVector Direction,
		float Length,
		float Width,
		ELOATargetFilter Filter,
		TEnumAsByte<ECollisionChannel> ObjectType,
		TArray<AActor*>& OutTargets,
		bool bShowRange = false,
		const AActor* Caster = nullptr,
		bool bHitByBody = true);

	/**
	 * 몸 하나가 모양 안에 있는가. 위치 계산만 한다. (액터, 검색, 편, 생사를 보지 않음 → 클라에서도, 지난 위치로도 쓸 수 있다)
	 * 쓰는 곳: 피해자 판정 (클라가 "그 시각의 내 위치"로 자기 하나만 판정. ULOAVictimJudgeSubsystem)
	 *
	 * 서버 검색(FindTargetsInShape ②)도 이 함수를 쓴다 → 서버와 클라가 같은 계산 (2026-11-01)
	 *   ① 높이: |몸 높이 − 기준점 높이| ≤ HalfHeight + 몸 반 높이 (띄워진 플레이어도 이 안이면 맞음)
	 *   ② 바닥 모양 (위에서 본 XY)
	 *   예) Sphere 반지름 300, 몸통 반지름 40, bHitByBody → 바닥 거리 340 이하 + 높이 차 700 + 몸 반 높이 이하면 안
	 *
	 * @param ShapeCenter     모양 기준점 (= GetShapeCenter. Sphere 중심 / Fan 꼭짓점 / Box 뒷변 가운데)
	 * @param Yaw             모양이 향하는 방향 (도)
	 * @param BodyLocation    확인할 몸 위치
	 * @param BodyRadius      몸통 반지름 (충돌 캡슐). Shape.bHitByBody가 꺼져 있으면 무시하고 중심점만 본다.
	 * @param BodyHalfHeight  몸통 반 높이 (충돌 캡슐). Shape.bHitByBody가 꺼져 있으면 무시하고 중심점만 본다.
	 */
	static bool IsBodyInShape(const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw,
		const FVector& BodyLocation, float BodyRadius, float BodyHalfHeight);

	// ---------- 디버그: 판정 모양 그리기 (LOA.Debug.HitShape) ----------

	/**
	 * 모양 공격의 맞은 지점 (2026-11-01). MakeClosestHitResult와 같지만, 기준점 높이를 대상 몸 중심 높이로 맞춰서 구한다.
	 * 기준점이 바닥(bSnapToGround)이면 그대로 쓸 때 맞은 지점이 대상 발목 → 데미지 숫자가 발밑에 뜬다.
	 * 방향(어느 면을 맞았나)은 그대로, 높이만 몸 가운데. HitResult의 TraceStart는 원래 기준점 (CC 밀기 방향 = 바닥 기준이라 같음)
	 * 쓰는 곳: Shape Targeting, 피해자 판정 결과 적용
	 */
	static bool MakeShapeHitResult(AActor* Target, const FVector& ShapeCenter, FHitResult& OutHit);

	/**
	 * [디버그] 콘솔 LOA.Debug.HitShape가 켜져 있나. 켜져 있으면
	 *   서버: Shape Targeting이 판정할 때 모양을 그린다 (Show Range 칸을 켠 것과 같음. 빨강 = 누군가 맞음 / 초록 = 아무도 없음)
	 *   클라: 피해자 판정을 할 때 판정 모양 + 내 몸 원을 그린다 (빨강 = 내 판정 맞음 / 초록 = 안 맞음)
	 * 그림은 그린 컴퓨터의 화면에만 보인다. 출시 빌드에서는 늘 false.
	 */
	static bool IsHitShapeDebugEnabled();

	/** [디버그] LOA.Debug.HitShape 켜기·끄기 (패턴 테스터 창의 체크박스). 이 컴퓨터에만 적용 */
	static void SetHitShapeDebugEnabled(bool bEnabled);

	/** [디버그] 판정 모양을 바닥에 1초간 선으로 그린다. (Sphere = 원, Fan = 부채꼴, Box = 직사각형 + 높이 범위 세로선) 출시 빌드에서는 아무것도 안 함 */
	static void DrawDebugHitShape(const UWorld* World, const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw, const FColor& Color);

	/**
	 * 이 대상이 지금 "맞을 수 있는" 대상인가. 대상 하나의 상태만 본다. (편은 PassesFilter가 따로 본다)
	 *   ① ASC가 있다           → 전투에 참여하는 액터 (벽, 소품 제외)
	 *   ② State.Dead가 없다    → 살아 있다 (시체 제외)
	 *   ③ State.Hidden이 없다  → 몸이 있다 (하늘로 사라진 보스 제외)
	 * 쓰는 곳: FindTargetsInSphere(Fan/Box도 거침), 투사체 겹침, Common 스탯창(시체에 온 데미지 버리기)
	 * 맞을 수 있는 조건이 늘면(예: 무대 밖, 무적 연출) 이 함수만 고친다.
	 * (나중에) 죽은 아군을 노리는 부활 스킬은 Targeting에 "죽은 대상 포함" 칸을 두고 ②를 건너뛴다.
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool IsTargetable(const AActor* Target);

	/**
	 * Source 기준으로 "맞을 수 있는 적 플레이어"를 전부 모은다. (보스 AI가 대상을 고를 때)
	 * 플레이어 목록 = GameState의 PlayerArray(파티원 목록) → 각자의 캐릭터 중 IsTargetable && IsEnemy
	 * 예) 3인 파티, 1명 사망 → 살아 있는 2명
	 * @return 찾은 수
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static int32 FindLivingEnemyPlayers(const AActor* Source, TArray<AActor*>& OutEnemies);

	/**
	 * A와 B가 서로 적인가.
	 * [임시 규칙] 팀 시스템이 없으므로 "플레이어 편인가"만 비교한다.
	 *   플레이어 편 = PlayerState가 있는 Pawn (플레이어 캐릭터)
	 *   편이 다르면 적 → 플레이어는 허수아비/몬스터/보스를, 보스는 플레이어를 때린다.
	 * 팀 시스템이 생기면 이 함수만 바꾸면 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool IsEnemy(const AActor* A, const AActor* B);

	/**
	 * Target이 Attacker의 공격을 무시하는가. = 적 사이이고 Target에 State.Invulnerable이 있다. [서버/클라 공용]
	 * 아군 사이(힐·버프)는 항상 false.
	 * 대상 고르기(IsTargetable)와는 따로다. 무적이어도 보스는 계속 노린다. 맞아도 아무 일이 없을 뿐.
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool IsInvulnerableTo(const AActor* Attacker, const AActor* Target);

	/**
	 * A와 B가 서로 아군인가. 자기 자신(A == B)은 아군이 아니다. (나를 넣을지는 Filter가 정한다)
	 * 아군 = 둘 다 ASC가 있고(전투에 참여하는 액터) 적이 아님.
	 * → 플레이어끼리는 아군, 보스와 다른 몬스터도 서로 아군.
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool IsAlly(const AActor* A, const AActor* B);

	/**
	 * 헤드어택인가. 때린 사람(Attacker)이 맞은 쪽(Target) 앞 부채꼴 안에 서 있으면 true. (바닥 기준, 높이 무시)
	 * 부채꼴 = Target이 보는 방향 기준 ±HalfAngleDegrees
	 *   예) HalfAngleDegrees 45, 보스가 북쪽을 봄. 플레이어가 보스 북동쪽(30°)에 있음 → true / 동쪽(90°)에 있음 → false
	 * 기준은 때린 사람의 위치다. 장판·투사체로 때려도 시전자가 어디 서 있나로 본다.
	 * 지금 자리로 본다. (2026-11-03부터 C++에서 부르는 곳 없음. 카운터는 공격 판정 기록의 공격 방향을 읽는다)
	 *
	 * @param HalfAngleDegrees  반각 (도). 45 = 좌우 45°씩, 합쳐서 90°
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool IsHeadAttack(const AActor* Target, const AActor* Attacker, float HalfAngleDegrees);

	/**
	 * 백어택인가. 때린 사람(Attacker)이 맞은 쪽(Target) 뒤 부채꼴 안에 서 있으면 true. IsHeadAttack의 반대쪽. (바닥 기준)
	 *   예) HalfAngleDegrees 45, 보스가 북쪽을 봄. 플레이어가 보스 남서쪽(남에서 30°)에 있음 → true / 서쪽(90°)에 있음 → false
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool IsBackAttack(const AActor* Target, const AActor* Attacker, float HalfAngleDegrees);

	/**
	 * 공격 방향 (앞 / 옆 / 뒤). 위치 계산만 한다 (액터를 보지 않음 → 되감은 자리로도 쓸 수 있다). (2026-11-03)
	 *   맞은 쪽이 보는 방향(TargetYaw)과 "맞은 쪽 → 때린 사람" 방향의 각도 (바닥 기준)
	 *   반각 = Project Settings → LOA Combat → Directional Attack Half Angle (모든 대상 공통, 카운터도 같은 각도)
	 *   예) 반각 45, 보스가 북쪽(Yaw 90)을 봄
	 *       때린 사람이 보스 북쪽 → Front / 북동쪽(북에서 30°) → Front / 동쪽 → Side / 남서쪽(남에서 30°) → Back
	 *   같은 자리에 겹쳐 있으면 None
	 *
	 * @param AttackerLocation  때린 사람 위치 (장판·투사체로 때려도 시전자)
	 * @param TargetLocation    맞은 쪽 위치
	 * @param TargetYaw         맞은 쪽이 보는 방향 (도)
	 */
	static ELOAAttackSide GetAttackSide(const FVector& AttackerLocation, const FVector& TargetLocation, float TargetYaw);

	/**
	 * 백 / 헤드 어택 결과. 공격 방향 + 이 공격의 속성. (2026-11-03부터 위치 계산은 GetAttackSide)
	 *   백 어택 스킬(Skill.Attribute.BackAttack) + Back → Back
	 *   헤드 어택 스킬(Skill.Attribute.HeadAttack) + Front → Head
	 *   그 외 → None (속성 없는 스킬, 옆에서 침)
	 *
	 * @param AttackTags  공격 속성 (Context.AttackTags)
	 * @param AttackSide  공격 방향 (GetAttackSide)
	 */
	static ELOADirectionalAttack GetDirectionalAttack(const FGameplayTagContainer& AttackTags, ELOAAttackSide AttackSide);

	/**
	 * 약점 공격일 때의 백 / 헤드 결과 (2026-11-18 apply124). 방향 검사를 건너뛰고 스킬 속성만 본다
	 *   백 어택 스킬 → Back / 헤드 어택 스킬 → Head / 속성 없음 → None (보너스 없음, 사용자 11-18)
	 *   두 속성이 다 붙은 스킬은 Back (GetDirectionalAttack이 Back을 먼저 보는 것과 같은 순서)
	 */
	static ELOADirectionalAttack GetDirectionalAttackOnWeakPoint(const FGameplayTagContainer& AttackTags);

	/**
	 * 약점 공격 상태인가 (State.Debuff.WeakPoint, 2026-11-18 apply124). 대난투 진행 중 보스
	 * ASC가 없으면 false
	 */
	static bool IsWeakPointExposed(const AActor* Target);

	/**
	 * [서버] 이 ASC에 상태 태그를 붙인다: 공용 태그 GE 하나 (2026-11-18 apply125, 같은 코드가 세 곳에 있어 모음)
	 *   Duration > 0 : ULOAGE_TimedTags (그 초가 지나면 엔진이 지움)   예) 부활 무적 3초
	 *   Duration = 0 : ULOAGE_InfiniteTags (돌려준 번호표로 지울 때까지) 예) 대난투 입력 막기, 약점 공격, 사망 미루기
	 *   태그는 ASC 복제로 모든 컴퓨터에 간다. 시전자 = 이 ASC 자신
	 * Ability 안에서는 ULOAGameplayAbility::ApplyStateTagsToSelf를 쓴다 (그 Ability가 보낸 효과로 적용됨)
	 * @return 지울 때 쓸 번호표. 붙이지 못하면 빈 번호표
	 */
	static FActiveGameplayEffectHandle ApplyStateTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& StateTags, float Duration = 0.f);

	/**
	 * [서버] 이 GE의 Effect Context에 적힌 백 / 헤드 어택 결과 (공격 판정 기록의 칸). 우리 쪽지(FLOAGameplayEffectContext)가 아니면 None.
	 * 읽는 곳: 데미지 계산(ULOADamageExecution), 데미지 숫자(ULOAAttributeSet_Common)
	 */
	static ELOADirectionalAttack GetDirectionalAttackFromContext(const FGameplayEffectContextHandle& EffectContext);

	/**
	 * [서버] 이 GE의 Effect Context에 적힌 공격 판정 기록 전체 (공격 방향, 백 / 헤드, 시각, 치명타). 우리 쪽지가 아니면 빈 기록 (2026-11-13)
	 * 읽는 곳: 데미지 계산(ULOADamageExecution, 치명타 배율), 데미지 숫자(ULOAAttributeSet_Common)
	 */
	static FLOAAttackJudgeRecord GetAttackJudgeRecordFromContext(const FGameplayEffectContextHandle& EffectContext);

	/**
	 * Target이 Source 기준으로 Filter에 맞는가. Filter 규칙은 여기 한 곳에만 있다.
	 *   Enemy       : IsEnemy
	 *   Ally        : IsAlly (나 제외)
	 *   AllyAndSelf : 나 자신이거나 IsAlly
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool PassesFilter(const AActor* Source, const AActor* Target, ELOATargetFilter Filter);

	/**
	 * 대상 하나에게 Payload 하나(GE + 값들)를 준다. [서버]
	 * 데미지, 힐, 버프 모두 이 함수 하나로 준다. 무엇이 될지는 Payload의 GE가 정한다.
	 *
	 *   GE(에셋)          : "어떻게 계산하나" (예: GE_Damage_Basic → ULOADamageExecution)
	 *   이 함수           : Payload의 값들을 SetByCaller로 Spec에 넣어서 적용 (예: SetByCaller.DamageRate = 2.5)
	 *   계산(Execution)   : 그 값을 읽어 최종 결과를 만든다 (공격력 × DamageRate)
	 *
	 * 맞은 지점이 있으면(Target.bHasHitPoint) GE Context에 HitResult를 넣는다. → 데미지 숫자가 그 자리에 뜬다.
	 *
	 * @param Context  누가(SourceASC = 보낸 사람), 몇 레벨로(Level = GE 레벨 + 값 커브 레벨)
	 * @param Target   받을 대상 + 맞은 지점
	 * @param Payload  줄 GE + 값들
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static void ApplyPayload(
		const FLOASkillModuleContext& Context,
		const FLOATargetHit& Target,
		const FLOAEffectPayload& Payload);

	/**
	 * 대상 여러 명에게 Payload 여러 개를 준다. [서버]
	 * 대상마다, Payload마다 ApplyPayload를 부른다. (Apply 모듈, 투사체/장판, 피해자 판정이 쓴다)
	 * 대상마다 Payload를 다 준 뒤, 적이면 "맞았다" 신호(Event.Combat.Hit)를 보낸다. (SendHitEvent)
	 * 무적(State.Body.Invulnerable)인 적은 건너뛴다. GE도 신호도 없다. (IsInvulnerableTo)
	 * 이 함수는 공격 속성의 뜻(카운터 등)을 모른다. 알리기만 하고, 판정은 맞은 쪽이 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static void ApplyPayloads(
		const FLOASkillModuleContext& Context,
		const TArray<FLOATargetHit>& Targets,
		const TArray<FLOAEffectPayload>& Payloads);

	/**
	 * Payloads 중 이 GE 클래스(자식 포함)인 첫 GE. 없으면 nullptr. 이 함수는 GE의 뜻을 모른다 (찾기만 한다)
	 *   예) 피해자 판정: FindPayloadEffect(Payloads, ULOAGE_CrowdControl::StaticClass()) → 이 공격의 CC GE (클라 예측 피격 반응에 같이 보냄)
	 */
	static TSubclassOf<UGameplayEffect> FindPayloadEffect(const TArray<FLOAEffectPayload>& Payloads, TSubclassOf<UGameplayEffect> EffectClass);

	// ---------- 맞은 지점 ----------

	/**
	 * 대상의 충돌체(루트 컴포넌트, 캐릭터면 캡슐) 표면에서 FromLocation과 가장 가까운 점. [서버/클라 공용]
	 *   서버: 범위 공격의 맞은 지점 (FromLocation = 판정 모양 중심)
	 *   클라: 도트처럼 맞은 지점이 없는 데미지 숫자 위치 (FromLocation = 내 캐릭터 위치)
	 * FromLocation이 충돌체 안쪽이면 FromLocation 그대로.
	 * @return 충돌체가 없거나 계산에 실패하면 false (OutPoint는 대상 위치)
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static bool GetClosestPointOnTarget(const AActor* Target, FVector FromLocation, FVector& OutPoint);

	/**
	 * GetClosestPointOnTarget으로 맞은 지점을 구해 HitResult를 만든다. [서버]
	 * 범위 공격(겹침 판정)은 엔진이 맞은 지점을 주지 않아서, 이 함수로 만들어 ApplyPayload에 넘긴다.
	 * @return 맞은 지점을 구했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static bool MakeClosestHitResult(AActor* Target, FVector FromLocation, FHitResult& OutHit);


	/** [임시 규칙] PlayerState가 있는 Pawn = 플레이어 편. IsEnemy와 치명타, 레이드 기록(맞은 쪽이 플레이어인가)이 같이 쓴다 (팀 시스템이 생기면 여기만) */
	static bool IsPlayerSide(const AActor* Actor);

private:
	/**
	 * 맞은 쪽이 보는 방향과 "맞은 쪽 → 때린 사람" 방향의 내적 = cos(사이 각). 바닥 기준(높이 무시).
	 *   1 = 정면, 0 = 옆, -1 = 등 뒤. 같은 자리에 겹쳐 있어 방향을 알 수 없으면 false.
	 * 위치로 계산한다 (2026-11-02). 액터 버전(IsHeadAttack / IsBackAttack)과 GetAttackSide가 같이 쓴다.
	 */
	static bool GetFacingDotToAttacker(const FVector& TargetLocation, const FVector& TargetForward,
		const FVector& AttackerLocation, float& OutDot);

	/** 반각 안의 정면 / 등 뒤인가 (Dot = GetFacingDotToAttacker). IsHeadAttack / IsBackAttack / GetAttackSide가 같은 기준을 쓴다 */
	static bool IsHeadDot(float Dot, float HalfAngleDegrees);
	static bool IsBackDot(float Dot, float HalfAngleDegrees);

	/**
	 * [서버] 이 대상에 대한 공격 판정 기록 (공격 방향, 백 / 헤드 결과, 판정 시각). 공격 판정 기록을 정하는 곳은 여기 하나. (2026-11-03)
	 *   자리·시각 = Target의 "공격 판정 기록을 정할 자리·시각"이 있으면 그것(공격자 판정), 없으면 지금 액터 자리·지금 시각
	 *   때린 사람 = Context.Caster (장판·투사체로 때려도 시전자)
	 *   (2026-11-13) 치명타도 여기서 굴린다 (RollCritical). 무작위라 대상 하나에 한 번만 부른다
	 * 부르는 곳: ApplyPayloads(대상마다 한 번 → 그 대상의 GE들과 "맞았다" 신호에 같은 기록), ApplyPayload(혼자 불렸을 때)
	 */
	static FLOAAttackJudgeRecord DecideAttackJudgeRecord(const FLOASkillModuleContext& Context, const FLOATargetHit& Target);

	/**
	 * [서버] 치명타를 굴린다 (2026-11-13). 플레이어 편이 적을 때릴 때만, 아니면 false (보스 공격·아군 회복·버프)
	 *   확률 = 설정 BaseCritRate + 때린 쪽 ExtraCritRate(Player) − 맞은 쪽 CritResistance(Combat) + (백 어택이면 BackAttackCritRateBonus) → 0~1
	 *   스탯은 그 순간 ASC의 값 (때린 쪽 = Context.SourceASC, 맞은 쪽 = 대상 ASC). 스탯창이 없으면 0
	 * @param DirectionalAttack  이 타격의 백 / 헤드 결과 (기록에서 먼저 정한 것)
	 */
	static bool RollCritical(const FLOASkillModuleContext& Context, const FLOATargetHit& Target, ELOADirectionalAttack DirectionalAttack);

	/** ApplyPayload의 몸통. 공격 판정 기록을 받아서 쪽지에 적는다 (ApplyPayloads가 대상마다 정한 기록을 넘김, 2026-11-13) */
	static void ApplyPayloadWithRecord(const FLOASkillModuleContext& Context, const FLOATargetHit& Target,
		const FLOAEffectPayload& Payload, const FLOAAttackJudgeRecord& AttackJudgeRecord);

	/**
	 * [서버] 맞은 쪽 ASC에 "맞았다" 신호(Event.Combat.Hit)를 보낸다. (ApplyPayloads가 대상마다, Payload를 다 준 뒤 부름)
	 * 시전자와 대상이 적일 때만. (아군 힐·버프는 "맞음"이 아니다)
	 *   EventData: Instigator = 시전자 / Target = 맞은 쪽 / InstigatorTags = Context.AttackTags (공격 속성)
	 *              ContextHandle = 공격 판정 기록을 적은 우리 쪽지 (데미지 GE의 쪽지와 같은 기록) (2026-11-03)
	 *   (2026-11-13) 기록은 ApplyPayloads가 정해 넘긴다 (치명타가 무작위라 여기서 다시 정하면 데미지 GE와 갈림)
	 * 듣는 쪽이 없으면 아무 일도 없다. 예) 보스의 카운터 패턴이 돌고 있으면 카운터 모듈이 받아 판정한다.
	 */
	static void SendHitEvent(const FLOASkillModuleContext& Context, const FLOATargetHit& Target, const FLOAAttackJudgeRecord& AttackJudgeRecord);
};
