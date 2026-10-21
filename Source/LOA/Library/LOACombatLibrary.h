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
//                    → AttributeSet → NotifyDamageDealt가 "대상 기준 상대 위치"로 바꿔 RPC
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
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // FLOASkillModuleContext, FLOATargetHit, FLOAEffectPayload
#include "Library/LOACombatTypes.h"                             // ELOAHitShapeType, ELOATargetFilter, FLOAHitShape (값만 담은 타입)
#include "LOACombatLibrary.generated.h"

UCLASS()
class LOA_API ULOACombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 판정 모양의 기준점 = 공격자 위치 + 앞 방향 × ForwardDistance
	 * (Sphere = 구의 중심, Fan = 부채꼴의 꼭짓점, Box = 직사각형 뒷변의 가운데)
	 * FindTargetsInShape와, 맞은 지점을 계산할 때(MakeClosestHitResult의 기준점) 같이 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static FVector GetShapeCenter(const AActor* Attacker, const FLOAHitShape& Shape);

	/**
	 * 모양(FLOAHitShape) 안에서 Filter에 맞는 대상을 모은다. [서버]
	 * 기준점 계산(기준 액터 위치 + 앞 방향 × ForwardDistance)과 "모양에 맞는 함수 고르기"를 여기서 한다.
	 * 방향이 필요한 모양(Fan)은 기준 액터의 앞 방향을 쓴다.
	 * → 부르는 쪽(Targeting, 보스 패턴 …)은 모양 구조체와 Filter만 넘기면 된다.
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
	 * 구 범위 안에서 Filter에 맞는 대상을 모은다. [서버]
	 * 보통은 FindTargetsInShape를 쓴다. 중심을 직접 정해야 할 때(장판의 커서 위치, 보스 패턴의 특정 지점 등) 이 함수를 쓴다.
	 * 제외: ASC가 없는 대상(벽, 소품) / Filter에 안 맞는 대상 / 중복
	 * (나 자신도 겹침 검사에는 포함된다. 뺄지 말지는 Filter가 정한다)
	 *
	 * @param Source      범위 기준 액터 (검색할 월드를 여기서 얻는다. Caster가 비었으면 편 판단 기준도 된다)
	 * @param Center      구의 중심 (월드 좌표)
	 * @param Radius      구의 반지름 (cm)
	 * @param Filter      누구를 고를지
	 * @param ObjectType  찾을 물체 종류. 플레이어/허수아비/보스 몸통은 Pawn
	 * @param OutTargets  찾은 대상을 여기에 "추가"한다. (이미 들어 있는 것과 겹치면 넣지 않음)
	 * @param bShowRange  true면 범위를 1초간 그린다. (빨강: 이번에 새로 찾음 / 초록: 없음)
	 * @param Caster      편 판단 기준 ("나"). 비우면(nullptr) Source를 쓴다.
	 * @param bHitByBody  true(기본) = 몸통이 구에 걸치면 맞음 / false = 발(중심점)이 원 안(바닥 거리)이어야 맞음
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
	 * 판정 (위에서 본 바닥 기준. 높이는 ①의 구 검색으로만 거른다)
	 *   ① 꼭짓점 중심, 반지름 Radius 구로 후보를 찾는다. (FindTargetsInSphere: ASC / Filter / 중복을 여기서 거름)
	 *   ② 후보의 몸통 원(충돌 캡슐 반지름)이 부채꼴에 조금이라도 걸치면 맞음.
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
	 * 판정 (위에서 본 바닥 기준. 높이는 ①의 구 검색으로만 거른다. 부채꼴과 같은 방식)
	 *   ① 직사각형을 다 덮는 구(중심 = 직사각형 중심, 반지름 = 대각선의 절반)로 후보를 찾는다.
	 *      (FindTargetsInSphere: ASC / Filter / 중복을 여기서 거름)
	 *   ② 후보의 몸통 원(충돌 캡슐 반지름)이 직사각형에 조금이라도 걸치면 맞음.
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
	 * 쓰는 곳: 피해자 판정 (클라가 "그 시각의 내 위치"로 자기 하나만 판정. ULOAHitJudgeSubsystem)
	 *
	 * Fan / Box는 FindTargetsInFan / Box와 같은 계산을 쓴다.
	 * Sphere만 다르다: 서버 검색은 충돌체 겹침(높이 포함)이고 여기는 바닥 거리. (로아 장판은 바닥이라 결과가 같다)
	 *   예) Sphere 반지름 300, 몸통 반지름 40, bHitByBody → 바닥 거리 340 이하면 안
	 *
	 * @param ShapeCenter   모양 기준점 (= GetShapeCenter. Sphere 중심 / Fan 꼭짓점 / Box 뒷변 가운데)
	 * @param Yaw           모양이 향하는 방향 (도)
	 * @param BodyLocation  확인할 몸 위치
	 * @param BodyRadius    몸통 반지름. Shape.bHitByBody가 꺼져 있으면 무시하고 중심점만 본다.
	 */
	static bool IsBodyInShape(const FLOAHitShape& Shape, const FVector& ShapeCenter, float Yaw,
		const FVector& BodyLocation, float BodyRadius);

	// ---------- 디버그: 판정 모양 그리기 (LOA.Debug.HitShape) ----------

	/**
	 * [디버그] 콘솔 LOA.Debug.HitShape가 켜져 있나. 켜져 있으면
	 *   서버: Shape Targeting이 판정할 때 모양을 그린다 (Show Range 칸을 켠 것과 같음. 빨강 = 누군가 맞음 / 초록 = 아무도 없음)
	 *   클라: 피해자 판정을 할 때 판정 모양 + 내 몸 원을 그린다 (빨강 = 내 판정 맞음 / 초록 = 안 맞음)
	 * 그림은 그린 컴퓨터의 화면에만 보인다. 출시 빌드에서는 늘 false.
	 */
	static bool IsHitShapeDebugEnabled();

	/** [디버그] LOA.Debug.HitShape 켜기·끄기 (패턴 테스터 창의 체크박스). 이 컴퓨터에만 적용 */
	static void SetHitShapeDebugEnabled(bool bEnabled);

	/** [디버그] 판정 모양을 바닥에 1초간 선으로 그린다. (Sphere = 원, Fan = 부채꼴, Box = 직사각형) 출시 빌드에서는 아무것도 안 함 */
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
	 * 쓰는 곳: 보스 카운터 모듈 (ULOASkillModule_Counter, 각도는 모듈 칸). (나중에) 헤드어택·백어택 데미지 보너스도 같은 방식
	 *
	 * @param HalfAngleDegrees  반각 (도). 45 = 좌우 45°씩, 합쳐서 90°
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Combat")
	static bool IsHeadAttack(const AActor* Target, const AActor* Attacker, float HalfAngleDegrees);

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
	 * 이 함수는 공격 속성의 뜻(카운터 등)을 모른다. 알리기만 하고, 판정은 맞은 쪽이 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Combat")
	static void ApplyPayloads(
		const FLOASkillModuleContext& Context,
		const TArray<FLOATargetHit>& Targets,
		const TArray<FLOAEffectPayload>& Payloads);

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

	/**
	 * [서버 전용] "데미지가 확정됐다"를 때린 플레이어에게 알린다. → 그 플레이어 화면에 데미지 숫자
	 * 보낸 사람 → PlayerController → 전투 RPC 컴포넌트(ULOACombatRPCComponent)의 Client RPC
	 * 받는 쪽 스탯창(ULOAAttributeSet_Common)이 체력을 깎은 직후 호출한다.
	 * 스탯창이 PlayerController/UI를 직접 알지 않도록 이 함수를 거친다.
	 *
	 * @param Instigator  데미지 GE의 보낸 사람 (플레이어면 PlayerState, 보스면 보스 액터)
	 *                    플레이어가 아니면(보스가 플레이어를 때림) 아무것도 안 한다. (맞은 데미지 표시는 나중에)
	 * @param Target      맞은 대상 (숫자가 따라다닐 대상)
	 * @param Damage      확정된 데미지
	 * @param HitResult   맞은 지점. 없으면 nullptr → 받은 클라가 "내 캐릭터와 가장 가까운 점"을 계산
	 */
	static void NotifyDamageDealt(AActor* Instigator, AActor* Target, float Damage, const FHitResult* HitResult);

private:
	/**
	 * [서버] 맞은 쪽 ASC에 "맞았다" 신호(Event.Combat.Hit)를 보낸다. (ApplyPayloads가 대상마다, Payload를 다 준 뒤 부름)
	 * 시전자와 대상이 적일 때만. (아군 힐·버프는 "맞음"이 아니다)
	 *   EventData: Instigator = 시전자 / Target = 맞은 쪽 / InstigatorTags = Context.AttackTags (공격 속성)
	 * 듣는 쪽이 없으면 아무 일도 없다. 예) 보스의 카운터 패턴이 돌고 있으면 카운터 모듈이 받아 판정한다.
	 */
	static void SendHitEvent(const FLOASkillModuleContext& Context, AActor* Target);
};
