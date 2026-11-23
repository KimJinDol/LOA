// LOAHitJudgeTypes.h
// [배관] 판정 쪽이 같이 쓰는 값 타입. (2026-11-04 판정 구조 개편으로 칸이 늘었다)
//   판정 기준            ELOAHitJudgeTiming      (장판 액터 칸에도 쓰여서 판정 헤더 전체를 include하지 않게 여기 둔다. 2026-10-31)
//   피해자 판정 요청 묶음 FLOAHitJudgeRequestData (서버 → 그 플레이어 RPC ULOACombatRPCComponent::ClientRequestHitJudge의 내용)
//   공격 하나의 정보     FLOAAttackInfo          (판정 매니저가 만든다. 판정 매니저 알림 "판정 시작"에도 실린다)
//   대상 하나의 판정 결과 FLOAJudgeResult         (판정 방식(서버 / 피해자 / 공격자)이 채워서 판정 매니저에 올린다)
//   내 화면에서 맞을 자리 FLOALocalHit         (클라. 판정 방식이 채워서 판정 매니저의 "먼저 반응"에 넘긴다)
//   한 사람 결과         ELOAHitOutcome          (판정 매니저 알림 "한 사람 결과")
//
// ─────────────────────────────────────────────────────────────────────────────
// [피해자 판정 요청 묶음 — 왜 묶었나] RPC 인자가 8개로 늘어 칸 하나 더할 때마다 선언·구현·판정 담당·디버그 명령을 다 고쳐야 했다.
//   이제 칸은 여기에만 더한다. RPC와 ULOAVictimJudgeSubsystem::JudgeLocal은 이 묶음 하나를 받는다.
//
// [직접 직렬화] NetSerialize (엔진 기본 대신)
//   없는 값은 안 보낸다     : 공격한 보스·공격 속성·CC GE가 없으면 안 보냄 (있음/없음 비트 3개)
//   모양은 그 모양의 칸만   : Sphere = 반지름 / Fan = 반지름·각도·안쪽 반지름 / Box = 길이·폭 (+ 몸통 판정 비트, 높이 범위 HalfHeight)
//                            ForwardDistance는 안 보낸다 (서버가 이미 ShapeCenter로 바꿔 보냄)
//   방향(Yaw)               : 2바이트 (약 0.005도 단위, FRotator::CompressAxisToShort)
//   자리(ShapeCenter)       : FVector_NetQuantize10과 같은 방식 (0.1cm)
//   판정 시각               : double 그대로 (float로 줄이면 서버가 오래 켜져 있을 때 ms 정밀도를 잃는다)
//   공격한 보스             : 액터 참조 (처음 한 번만 경로, 그 뒤는 번호)
//   공격 속성               : 엔진 태그 묶음 직렬화 (태그마다 번호)
//   예) 원 모양, 보스·속성·CC 없음 ≈ 4 + 1 + 4 + 4 + 약 7 + 2 + 8 ≈ 30바이트
//
// [저가 칸이 없어진 이유] (2026-11-04)
//   전에는 저가 구간 번호·방식을 실어 보냈다. 이제 판정 쪽은 저가를 모른다.
//   공격한 보스와 공격 속성만 보내고, 맞은 사람의 저가 컴포넌트(ULOAJustGuardComponent)가 직접 구간을 찾는다.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "GameplayTagContainer.h"
#include "Library/LOACombatTypes.h"                                // FLOAHitShape
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // FLOASkillModuleContext, FLOAEffectPayload, FLOATargetHit (값으로 가지므로 include)
#include "LOAHitJudgeTypes.generated.h"

class UGameplayEffect;
class AActor;
class APawn;

/**
 * 플레이어가 "지금 터진다"를 무엇으로 아나. 판정 방식이 달라진다. ULOACombatJudgeManagerSubsystem 헤더 [누가 판정하나]
 *   예) 핑 200ms(편도 0.1), 서버 판정 101.2 → 예고는 내 화면 101.06(편도 + 여유 0.04 앞당김)에 꽉 참 / 도끼는 내 화면 101.3에 떨어짐
 */
UENUM()
enum class ELOAHitJudgeTiming : uint8
{
	/** 예고가 꽉 차는 순간을 보고 피한다 → 항상 서버 판정 (예고가 앞당겨 그려져 화면과 맞는다) */
	Telegraph	UMETA(DisplayName = "Telegraph (예고가 꽉 참)"),
	/** 보스 모션을 보고 피한다 → Hit Judge 칸대로. 피해자 판정이면 요청을 받은 순간의 내 위치 (= 내 화면에서 도끼가 떨어진 순간. 예: 101.3) */
	Animation	UMETA(DisplayName = "Animation (보스 모션)"),
};

/** 피해자 판정 요청 하나 (서버 → 그 플레이어). 헤더 [직접 직렬화] */
USTRUCT()
struct LOA_API FLOAHitJudgeRequestData
{
	GENERATED_BODY()

	/** 판정 번호 = 공격 번호 (판정 매니저가 매김). 보고에 그대로 돌려준다 (디버그 명령은 음수) */
	UPROPERTY()
	int32 RequestId = 0;

	/** 판정 모양 (ForwardDistance는 안 보낸다. 기준점은 ShapeCenter) */
	UPROPERTY()
	FLOAHitShape Shape;

	/** 모양 기준점 (Sphere 중심 / Fan 꼭짓점 / Box 뒷변 가운데) */
	UPROPERTY()
	FVector ShapeCenter = FVector::ZeroVector;

	/** 모양 방향 (도) */
	UPROPERTY()
	float Yaw = 0.f;

	/** 서버 판정 시각. 클라는 "본 시각"을 자를 때와 로그에 쓴다 */
	UPROPERTY()
	double HitServerTime = 0.0;

	/** 맞으면 걸리는 CC GE (띄움·경직, 없으면 nullptr). 클라가 "맞음"이면 먼저 반응한다 (ULOACrowdControlPredictionComponent) */
	UPROPERTY()
	TSubclassOf<UGameplayEffect> CrowdControlEffect;

	/** 공격한 보스 (Context.Caster). 맞은 사람의 저가 컴포넌트가 이 보스의 구간을 찾는다 (2026-11-04) */
	UPROPERTY()
	TObjectPtr<AActor> Attacker = nullptr;

	/** 공격 속성 (Context.AttackTags. 예: Skill.Attribute.JustGuard). 저가 공격인지는 저가 컴포넌트가 본다 (2026-11-04) */
	UPROPERTY()
	FGameplayTagContainer AttackTags;

	/** 헤더 [직접 직렬화] */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FLOAHitJudgeRequestData> : public TStructOpsTypeTraitsBase2<FLOAHitJudgeRequestData>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/**
 * [서버] 공격 하나에 공통인 정보. 판정 매니저(ULOACombatJudgeManagerSubsystem)가 만든다. (2026-11-04)
 * 판정 매니저 알림 "판정 시작"에 실려서, 듣는 쪽(저가 매니저)이 공격한 보스·공격 속성·시각을 본다.
 */
USTRUCT()
struct LOA_API FLOAAttackInfo
{
	GENERATED_BODY()

	/** 공격 번호 (1부터). 알림의 시작 · 한 사람 결과 · 끝을 잇는다. 피해자 판정 요청 번호도 이 번호 */
	int32 AttackId = 0;

	/** 누가, 몇 레벨로, 어떤 속성으로. 복사본 (UPROPERTY: 시전자가 사라지면 GC가 비운다) */
	UPROPERTY()
	FLOASkillModuleContext Context;

	/** 맞은 대상에게 줄 것 */
	UPROPERTY()
	TArray<FLOAEffectPayload> Payloads;

	/** 플레이어가 무엇을 보고 피하나 (예고 장판 / 보스 모션) */
	ELOAHitJudgeTiming Timing = ELOAHitJudgeTiming::Animation;

	/** 공격 기준점 (모양 기준점. 저가 방향 검사) */
	FVector AttackOrigin = FVector::ZeroVector;

	/** 서버에서 판정한 시각 (저가 구간 찾기) */
	double HitServerTime = 0.0;
};

/**
 * 대상 하나의 판정 결과. 판정 방식(서버 / 피해자 / 공격자)이 채워서 판정 매니저에 올린다. (2026-11-04)
 * 판정 매니저의 확정(ConfirmHit)은 이 값을 계산 없이 쓴다 (저가 확인 → 적용)
 */
USTRUCT()
struct LOA_API FLOAJudgeResult
{
	GENERATED_BODY()

	/** 대상 + 맞은 지점 + 공격 판정 기록 재료 (ApplyPayloads에 그대로 넘긴다) */
	UPROPERTY()
	FLOATargetHit Hit;

	/** 맞을 자리였나 (모양 안). false = 피함 */
	bool bInRange = false;

	/**
	 * 그 사람 화면에서 공격이 닿은 서버 시각 (저가 "제때 눌렀나"). 판정 방식이 정한다
	 *   서버 판정 모션 = 판정 시각 / 서버 판정 예고 장판 = 판정 시각 − (편도 + 예고 여유) / 피해자 판정 = 판정 시각 + 편도(최대 0.2)
	 */
	double SeenTime = 0.0;

	/** 판정에 쓴 그 사람 위치 (저가 방향 검사) */
	FVector TargetLocation = FVector::ZeroVector;

	/** 피해자 판정에서 클라가 "맞음"으로 보고함 → 저가 컴포넌트가 맞은 시각만 적고 다시 판단하지 않는다 */
	bool bClientSaidHit = false;
};

/**
 * [클라] 내 화면에서 맞을 자리였다. 판정 방식이 채워서 판정 매니저의 ReactLocal에 넘긴다. (2026-11-04)
 *   피해자 판정 요청 도착 (ULOAVictimJudgeSubsystem::JudgeLocal)
 *   예고 장판이 내 화면에서 다 참 (ULOAServerJudgeSubsystem::JudgeTelegraphLocal)
 */
struct FLOALocalHit
{
	/** 공격한 보스 (저가 컴포넌트가 이 보스의 구간을 찾는다) */
	TObjectPtr<AActor> Attacker = nullptr;

	/** 공격 속성 (저가 공격인가) */
	FGameplayTagContainer AttackTags;

	/** 공격 기준점 (저가 방향 검사, CC 바라보기) */
	FVector AttackOrigin = FVector::ZeroVector;

	/** 서버 판정 시각 */
	double HitServerTime = 0.0;

	/** 내 화면에서 닿은 서버 시각 (저가 "제때 눌렀나") */
	double SeenTime = 0.0;

	/** 내 캐릭터와 판정에 쓴 위치 */
	TObjectPtr<APawn> MyPawn = nullptr;
	FVector MyLocation = FVector::ZeroVector;

	/** 이 공격의 CC GE (없으면 nullptr) + 먼저 반응을 되돌릴 시간 (초) */
	TSubclassOf<UGameplayEffect> CrowdControlEffect;
	float CrowdControlTimeout = 0.f;
};

/**
 * [클라] 판정 방식 → 판정 매니저: 내 화면에서 맞을 자리였다 (서버 판정·피해자 판정 둘 다 이 모양. 판정 매니저 하나만 묶는다)
 * @return 저가로 막았나 (피해자 판정은 이 값으로 "막음" / "맞음" 보고를 고른다)
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FLOAOnLocalHit, const FLOALocalHit& /*Hit*/);

/** 한 사람 결과. 판정 매니저 알림 OnHitOutcome (2026-11-04) */
enum class ELOAHitOutcome : uint8
{
	Missed,		// 피함 (모양 밖)
	Blocked,	// 맞을 자리였지만 막음 (저스트 가드)
	Hit,		// 맞음
};
