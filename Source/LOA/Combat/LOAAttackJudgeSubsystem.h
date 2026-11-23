// LOAAttackJudgeSubsystem.h
// [배관] 공격자 판정. "때리는 쪽 화면으로 맞았나를 정한다." (2026-11-02)
// 판정 매니저(ULOACombatJudgeManagerSubsystem)가 맡긴다. 보고 보내기, 받기, 되감아 검사를 여기서 한다.
// 판정만 한다. 적용도, 저가도, 판정 매니저도 모른다. 결과는 알림(OnReportJudged)으로 올린다. (2026-11-04 판정 구조 개편)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 있나] 예: 핑 200ms(편도 0.1초), 보스가 초당 500cm로 걷는 중
//   0.4초 [내 컴퓨터] Hit 노티파이 (내 스킬은 예측이라 서버보다 편도만큼 먼저 돈다)
//                     내 화면의 보스 = 서버 0.3초 때 자리 A (복제가 편도만큼 늦게 온다)
//   0.5초 [서버]      서버 노티파이. 보스는 이미 100cm 더 간 B → 반지름 끝에 걸친 공격이 빗나간다
//   → 타격 순간은 앞당겨지고 보스는 늦게 보여서 양쪽으로 편도씩 벌어진다. 내 화면(A)으로 판정해야 한다.
//
// [어느 공격에 쓰나] 타격 순간을 "내 컴퓨터"가 정하는 공격만 = 즉발(Apply 모듈)
//   장판·투사체는 서버가 소환·판정한다. 터지는 순간과 보스 자리가 똑같이 편도만큼 늦게 보여서
//   내 화면 안에서 서로 맞는다 → 서버 판정이 곧 내 화면 (ULOAServerJudgeSubsystem).
//   나중에 장판을 내 컴퓨터에서 먼저 띄우게(예측 소환) 바꾸면 그때 여기를 쓴다.
//
// [다른 판정 방식과 나눈 기준] (2026-11-04)
//   서버 판정 (ULOAServerJudgeSubsystem)  = 서버가 아는 위치
//   피해자 판정 (ULOAVictimJudgeSubsystem) = 맞는 쪽 화면 (보스 모션을 보고 피하는 공격)
//   공격자 판정 (이 클래스)                = 때리는 쪽 화면. 저가·CC·예고·판정 요청 목록을 쓰지 않는다.
//   셋 다 서로를 모른다. 고르기·확정(저가 확인 → 적용)·알리기는 판정 매니저가 한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [판정 매니저가 부르는 것] (Apply 모듈 → 판정 매니저 BeginAttackReports / JudgeInstantHit / EndAttackReports → 여기)
//   BeginAttackReports : [서버, 원격 플레이어의 공격] 이 발동의 보고 듣기 시작
//   SendReport             : [원격 플레이어 컴퓨터] 내 화면으로 대상 찾기 → 공격 보고
//   EndAttackReports  : 듣기 끝 (스킬이 끝나면 늦게 온 보고는 받지 않는다)
//   IsAttackerJudged       : 이 공격이 공격자 판정 공격인가 (판정 매니저가 판정 방식을 고를 때. 규칙만)
//
// [공격자 판정 공격인가] IsAttackerJudged
//   공격자 판정 공격 = Targeting에 범위(모양 + 편)가 있고 편이 Enemy + 플레이어 스킬(LocalPredicted)
//     Targeting은 범위를 데이터로만 내준다 (GetRange). 판정 규칙과 계산은 전부 여기. Targeting 종류로 캐스트하지 않는다
//     예) Shape + Enemy(평타·근접 스킬) → 공격자 판정 / Shape + Ally(범위 버프) → 서버 판정 / Self(자버프, 범위 없음) → 서버 판정
//     아군 대상은 안 한다: 다른 플레이어 위치에 붙는 시각은 그 플레이어 컴퓨터의 이동 시각이라(엔진 CharacterMovementComponent.cpp
//     ServerAccumulatedClientTimeStamp) 보스처럼 서버 기록으로 되감을 수 없고, 버프 경계에서 조금 틀려도 손해가 작다
//     내 컴퓨터(원격 플레이어) : 내 화면으로 대상 찾기 → 공격 보고 (SendReport)
//     서버                      : 자기 노티파이로는 판정하지 않는다. 보고가 오면 검사 (OnReportReceived → JudgeReport)
//     리슨 호스트 본인          : 서버 화면 = 내 화면 → 서버 판정
//   그 밖 (보스 패턴, 아군 힐·버프, Self) : 서버 판정
//   누가 어느 일을 맡는지(서버 / 내 보고 / 보고 기다림 / 없음)는 판정 매니저가 정한다 (ULOACombatJudgeManagerSubsystem::GetRole)
//
// [보고 통로 = GAS TargetData] (엔진 코드 확인, AbilitySystemComponent_Abilities.cpp)
//   ServerSetReplicatedTargetData = 스킬 발동과 같은 ASC의 Reliable RPC → 발동보다 먼저 도착하지 않는다
//     (PC의 ULOACombatRPCComponent로 보내면 다른 액터 통로라 순서 보장이 없다. CC 때 겪음)
//   칸은 (스킬, 발동 번호)마다 하나. 새로 오면 덮어쓰지만 오는 순간 듣는 쪽에 알리므로 다단 히트도 하나씩 받는다
//   커서 TargetData와 같은 칸 → 구조체 종류(FLOAAttackReportTargetData)와 모듈 번호로 가른다
//   지금 예측 번호는 빈 값으로 보낸다 → 서버가 데미지 GE에 예측 번호를 붙이지 않고 확인 응답도 안 보낸다
//     (FScopedPredictionWindow 소멸자는 유효한 번호일 때만 확인 응답. GameplayPrediction.cpp)
//
// [보스 자리의 서버 시각 — 짐작하지 않는다]
//   클라는 맞은 보스 액터의 ACharacter::GetReplicatedServerLastTransformUpdateTimeStamp()를 그대로 옮겨 적는다
//   (엔진이 위치와 같이 보내 주는 "그 위치의 서버 시각". ULOAPositionHistoryComponent가 켠다)
//   서버는 그 시각으로 자리 기록(ULOAPositionHistoryComponent::FindAt)을 찾는다.
//   이 시각은 보스가 움직이거나 돌 때만 새로 찍힌다. 제자리면 옛날 값 → 한도로 잘린다 (자리는 같으니 자리 검사엔 문제없음)
//
// [내 화면이 보여 주는 서버 시각] (2026-11-03) 보고 하나에 하나 = ViewServerTime
//   클라의 엔진 서버 시각 추정값 (LOANetTime::GetServerTimeNow − GetOneWayLatency). 서버에서 오는 데 걸린 시간(편도)만큼 늦은 값이라
//   = 내 화면의 보스·파란 빛이 서버 몇 초 때 모습인가. 보스가 서 있어도 계속 흐른다.
//   쓰는 곳: 공격 판정 기록의 시각 (카운터: 그 시각에 파란 구간이었나). 자리 되감기는 위의 보스 자리 시각 그대로
//     예) 카운터 패턴 중 보스 제자리. 보스 자리 시각 = 마지막 이동 1.06 (옛날) / 내 화면 서버 시각 = 3.00 → 기록 시각 3.00
//   검사: 자리 시각과 같은 한도 (지금 − (핑 + 0.3초) ~ 지금)로 자른다
//
// [서버 검사] JudgeReport. 예: 보고 {원 반지름 250, 내 자리 (0,0), 보스, 10.30} 이 10.50에 도착
//   ① 내 자리: 보고 위치 ↔ 보고가 도착한 순간 서버가 아는 내 위치, LocationTolerance(150cm) 안   → (5,0) 5cm 통과
//              (보고와 그 순간까지의 이동은 같이 도착한다 — 피해자 판정 보고 검사와 같은 이유)
//   ② 대상   : 지금 살아 있음 + 적
//   ③ 되감기 : 보고에 적힌 시각. 한도 = 지금 − (그 플레이어 핑 + RewindMargin 0.3초)               → 한도 10.00, 10.30 그대로
//              더 옛날이면 한도로 자른다 (보고 시각을 일부러 옛날로 적기 막기. 보스가 오래 서 있었으면 같은 자리라 문제없음)
//              시각이 없으면(허수아비 = 캐릭터가 아님) 지금 자리 (안 움직이니 결과가 같다)
//   ④ 범위   : Targeting의 범위(GetRange)를 보고한 내 자리·방향에 놓고, 되감은 보스 자리로 편 + IsBodyInShape
//                                                                                                       → 10.30 보스 (280,0): 280 ≤ 250 + 60 → 맞음
//   대상마다 결과 하나 → OnReportJudged로 판정 매니저에 올린다 (판정 매니저가 확정: 저가 확인 → 적용)
//     통과한 대상 = 맞을 자리. 공격 판정 기록의 공격 방향·백·헤드는 되감은 자리·방향으로, 시각은 내 화면 서버 시각(ViewServerTime)으로
//     실패한 대상 = 피함 + 경고 (사용자 결정. 거짓 "맞음"은 보낸 사람 이득이라 서버 판정으로 대신하지 않는다)
//   방향(Yaw)은 검사하지 않는다 (플레이어 회전은 원래 그 플레이어 이동이 서버로 보낸다). 로그에 차이만
//   보고 개수: 한 발동에서 그 모듈이 받는 보고 ≤ Apply가 알려 준 수 (몽타주의 그 노티파이 수). 넘으면 버리고 경고
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1 서버만 : 듣기 목록, 보고 검사(내 자리 · 대상 · 되감기 · 모양), 개수 제한 (적용은 판정 매니저)
//   2 전달   : 공격 보고 (그 플레이어 → 서버, GAS TargetData = 스킬 발동과 같은 ASC 통로, Reliable)
//              보스 위치 + 그 서버 시각 (엔진 복제, ULOAPositionHistoryComponent가 켬)
//   3 각자   : 공격한 플레이어 컴퓨터의 대상 찾기 (내 화면의 보스 자리)
//   이 서브시스템은 복제되지 않는다. 컴퓨터마다 따로 있다 (다른 판정 방식과 같은 방식)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Abilities/GameplayAbilityTargetTypes.h"                   // FGameplayAbilityTargetData
#include "GameplayAbilitySpecHandle.h"
#include "GameplayPrediction.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // FLOASkillModuleContext, FLOAEffectPayload
#include "Combat/LOAHitJudgeTypes.h"                               // FLOAAttackInfo, FLOAJudgeResult
#include "LOAAttackJudgeSubsystem.generated.h"

class ULOAGameplayAbility;
class ULOASkillModule;
class ULOASkillTargeting;
class UAbilitySystemComponent;
class AActor;

// =============================================================================
// 공격 보고 (내 컴퓨터 → 서버). 이 클래스만 쓰므로 여기 둔다
// =============================================================================
//
// [직접 직렬화] 예: 대상 1명 ≈ 1(모듈) + 약 7(내 자리) + 2(방향) + 4(내 화면 서버 시각) + 1(수) + 약 4(대상) + 4(시각) ≈ 23바이트
//   내 자리 : FVector_NetQuantize10 (0.1cm) / 내 방향 : 2바이트 (FRotator::CompressAxisToShort) / 내 화면 서버 시각 : float
//   대상 수 : 1바이트, 최대 MaxTargets / 대상 : 액터 참조(PackageMap) + 그 대상 자리의 서버 시각 float (엔진 값 그대로. 0 = 없음)

/** 공격 보고 안의 대상 하나 */
USTRUCT()
struct FLOAAttackReportTarget
{
	GENERATED_BODY()

	/** 내 화면에서 맞은 대상 */
	UPROPERTY()
	TObjectPtr<AActor> Target = nullptr;

	/**
	 * 내 화면의 그 대상 자리가 "서버 몇 초 때 자리"인가 = ACharacter::GetReplicatedServerLastTransformUpdateTimeStamp
	 * 0 = 모름 (캐릭터가 아님(허수아비) / 그 칸이 꺼진 캐릭터) → 서버는 지금 자리로 검사
	 */
	UPROPERTY()
	float TargetServerTime = 0.f;
};

/** 공격 보고 하나 */
USTRUCT()
struct LOA_API FLOAAttackReportTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** 한 보고에 담는 최대 대상 수. 넘으면 앞에서부터 이만큼만 (3인 레이드 = 보스 하나) */
	static constexpr int32 MaxTargets = 16;

	/** 이 GA의 몇 번째 모듈이 보냈나 (ULOAGameplayAbility::Modules 순서). 서버에서 같은 번호의 듣기만 받는다 */
	UPROPERTY()
	uint8 ModuleIndex = 0;

	/** 판정할 때 내 캐릭터 위치 */
	UPROPERTY()
	FVector_NetQuantize10 CasterLocation;

	/** 판정할 때 내 캐릭터 방향 (도) = 모양 방향 */
	UPROPERTY()
	float CasterYaw = 0.f;

	/**
	 * 판정할 때 내 화면이 보여 주는 서버 시각 (초). 클라의 엔진 서버 시각 추정값 (편도만큼 늦음). 0 = 모름 → 서버는 지금 시각 (2026-11-03)
	 * 공격 판정 기록의 시각이 된다 (카운터). 헤더 [내 화면이 보여 주는 서버 시각]
	 */
	UPROPERTY()
	float ViewServerTime = 0.f;

	/** 내 화면에서 맞은 대상들 */
	UPROPERTY()
	TArray<FLOAAttackReportTarget> Targets;

	//~ Begin FGameplayAbilityTargetData Interface
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FString ToString() const override;
	//~ End FGameplayAbilityTargetData Interface

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FLOAAttackReportTargetData> : public TStructOpsTypeTraitsBase2<FLOAAttackReportTargetData>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/** [서버] 보고를 듣고 있는 발동 하나 (Apply 모듈 하나 × 발동 하나) */
USTRUCT()
struct FLOAAttackReportListener
{
	GENERATED_BODY()

	// 약한 포인터는 GC가 따로 알 필요가 없어서 UPROPERTY가 아니다 (GA·모듈이 사라지면 비어 있음)
	TWeakObjectPtr<ULOAGameplayAbility> Ability;
	TWeakObjectPtr<const ULOASkillModule> Module;
	TWeakObjectPtr<const ULOASkillTargeting> Targeting;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	/** 그 모듈의 칸 번호 (보고의 ModuleIndex와 맞춘다) */
	int32 ModuleIndex = INDEX_NONE;

	/** 그 모듈의 Payloads 복사본 (GE 클래스 참조라 UPROPERTY) */
	UPROPERTY()
	TArray<FLOAEffectPayload> Payloads;

	/** 듣고 있는 (스킬, 발동 번호)와 등록 번호 */
	FGameplayAbilitySpecHandle SpecHandle;
	FPredictionKey ActivationKey;
	FDelegateHandle DelegateHandle;

	/** 이 발동에서 더 받을 수 있는 보고 수 */
	int32 NumReportsLeft = 0;
};

/**
 * [서버] 공격자 판정 → 판정 매니저: 보고 하나를 검사했다 (보고 하나 = 공격 하나). (2026-11-04)
 * @param Attack   공격 정보 (공격 번호는 비어 있다. 판정 매니저가 매긴다)
 * @param Results  보고한 대상마다 하나 (맞을 자리 / 피함)
 */
DECLARE_DELEGATE_TwoParams(FLOAOnReportJudged, const FLOAAttackInfo& /*Attack*/, const TArray<FLOAJudgeResult>& /*Results*/);

UCLASS()
class LOA_API ULOAAttackJudgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 이 객체가 속한 월드의 공격자 판정 담당. 게임 월드가 아니면 nullptr */
	static ULOAAttackJudgeSubsystem* Get(const UObject* WorldContextObject);

	// ---------- 판정 매니저가 부른다 ----------

	/** 공격자 판정 공격인가 (범위 + Enemy + 플레이어 스킬). 헤더 [공격자 판정 공격인가]. 규칙만 본다 */
	static bool IsAttackerJudged(const ULOAGameplayAbility* Ability, const ULOASkillTargeting* Targeting);

	/**
	 * [서버] 발동 시작: 이 발동의 보고를 듣기 시작한다 (원격 플레이어의 공격자 판정 공격일 때 판정 매니저가 부른다)
	 * @param NumAllowedReports  이 발동에서 받을 보고 수 (= 몽타주의 그 노티파이 수. 모듈 설정이라 모듈이 센다)
	 */
	void BeginAttackReports(ULOAGameplayAbility* Ability, const ULOASkillModule* Module, const ULOASkillTargeting* Targeting,
		const TArray<FLOAEffectPayload>& Payloads, int32 NumAllowedReports);

	/** [내 컴퓨터] 노티파이 순간: 내 화면으로 대상 찾기 → 공격 보고 */
	void SendReport(ULOAGameplayAbility* Ability, const ULOASkillModule* Module, const ULOASkillTargeting* Targeting);

	/** 발동 끝. 듣기 해제 (듣고 있지 않으면 아무것도 안 함) */
	void EndAttackReports(const ULOAGameplayAbility* Ability, const ULOASkillModule* Module);

	/** [서버] 보고 하나를 검사했다 (판정 매니저가 듣는다) */
	FLOAOnReportJudged OnReportJudged;

protected:
	//~ Begin UWorldSubsystem Interface
	/** 게임 월드(PIE, 실제 게임)에서만 만든다 */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem Interface

private:
	/** [서버] TargetData 도착 (듣기 번호 = Listeners의 키) */
	void OnReportReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag, int32 ListenerId);

	/**
	 * [서버] 보고 검사 → 보고한 대상마다 결과 하나 (맞을 자리 / 피함). 헤더 [서버 검사]
	 * @param OutAttackOrigin  보고한 내 자리에 놓은 모양 기준점 (공격 정보의 공격 기준점)
	 * @return 검사를 했나 (내 자리가 너무 멀면 false = 전부 거절, 결과 없음)
	 */
	static bool JudgeReport(const ULOASkillTargeting* Targeting, const FLOASkillModuleContext& Context,
		const FLOAAttackReportTargetData& Report, TArray<FLOAJudgeResult>& OutResults, FVector& OutAttackOrigin);

	/** [서버] 듣고 있는 발동들 (듣기 번호 → 듣기). 보통 0 ~ 몇 개 */
	UPROPERTY()
	TMap<int32, FLOAAttackReportListener> Listeners;

	/** [서버] 마지막으로 쓴 듣기 번호 */
	int32 LastListenerId = 0;
};
