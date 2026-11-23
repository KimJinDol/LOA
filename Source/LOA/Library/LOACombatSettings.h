// LOACombatSettings.h
// [배관] 전투 규칙 설정. Project Settings → Game → LOA Combat 에 보인다. (값은 Config/DefaultGame.ini에 저장)
//
// ─────────────────────────────────────────────────────────────────────────────
// [지금 있는 것] (2026-10-25)
//   Directional Attack Half Angle : 백 / 헤드 어택 방향 판단의 반각. 모든 보스·몬스터 공통 (로아 규칙)
//     예) 45 → 맞은 쪽이 보는 방향 기준 앞 ±45° = 헤드, 뒤 ±45° = 백, 그 사이 좌우 90°씩 = 옆(보너스 없음)
//   Base Back / Head Attack Damage Bonus : 백 / 헤드 어택 기본 피해 보너스. 모든 직업 공통 (로아 규칙, 사용자 결정 2026-10-25)
//     0.05 = +5% (백) / 0.20 = +20% (헤드)
//     각인·버프로 "더해지는" 몫은 사람마다 다르므로 여기가 아니라 공격자의 Combat 스탯
//     (ULOAAttributeSet_Combat::ExtraBackAttackDamage / ExtraHeadAttackDamage, 기본 0)
//     최종 보너스 = 여기 기본값 + 공격자의 추가분.  예) 기본 0.05 + 기습의 대가 0.25 = 0.30 → 300 × 1.30 = 390
//   Just Guard Window : 저스트 가드 창 0.5초. 모든 직업 공통 (2026-10-26). 누른 시각부터 이 시간 안에 맞는 공격을 막는다
//   Critical (2026-11-13) : 기본 치명타 적중률 70%, 기본 치명타 피해 200%, 백 어택 치명타 +10%p. 모든 직업 공통
//     최종 확률 = 기본 + 때린 쪽 ExtraCritRate − 맞은 쪽 CritResistance + 백 어택이면 +10%p → 0~100%로 자름
//     예) 70% − (약점 노출로 저항 −10%) + 백 어택 10% = 90%
//
//             앞 (헤드)
//          \   ±45   /
//           \       /
//     옆     [ 보스 ]     옆
//           /       \
//          /   ±45   \
//             뒤 (백)
//
// [왜 여기에]
//   함수만 있는 ULOACombatLibrary, 데미지 계산 ULOADamageExecution은 BP 기본값 칸을 가질 수 없다.
//   보스마다 다를 일이 없다 (사용자 확인: 안 받는 보스 없음, 각도도 공통) → 프로젝트 설정 하나.
//   기본 보너스도 직업마다 다를 일이 없다 → 직업 데이터(CurveTable)에 직업마다 같은 행을 반복하지 않는다.
//   기준: 프로젝트 전체에 하나인 규칙 숫자 = 여기 / 사람마다 달라지는 것 = 스탯.
//   전투 규칙 숫자가 많이 늘면(치명타 기본값 등) 전투 규칙 DataAsset 하나로 옮기는 것을 검토.
//   보스 카운터의 정면 판단도 이 각도다 (사용자 결정 2026-11-03: 카운터 각도 = 헤드 어택 각도, 패턴마다 바꾸지 않는다).
//     공격 방향(ELOAAttackSide) Front = 헤드 범위 → 숫자에 HEAD가 뜨는 자리와 카운터가 정면으로 인정하는 자리가 같다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LOACombatSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "LOA Combat"))
class LOA_API ULOACombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Project Settings의 "Game" 묶음에 보이게 */
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 * 백 / 헤드 어택 방향 판단의 반각 (도). 앞 ±이 각도 = 헤드, 뒤 ±이 각도 = 백, 나머지 = 옆. 카운터의 정면도 같은 범위.
	 * 90을 넘으면 앞과 뒤가 겹치므로 90까지.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Directional Attack", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees"))
	float DirectionalAttackHalfAngle = 45.f;

	/** 백 어택 기본 피해 보너스. 0.05 = +5%. 백 어택 스킬로 뒤에서 쳤을 때. 공격자의 추가분(ExtraBackAttackDamage)이 더해진다 */
	UPROPERTY(Config, EditAnywhere, Category = "Directional Attack", meta = (ClampMin = "0.0"))
	float BaseBackAttackDamageBonus = 0.05f;

	/** 헤드 어택 기본 피해 보너스. 0.20 = +20%. 헤드 어택 스킬로 앞에서 쳤을 때. 공격자의 추가분(ExtraHeadAttackDamage)이 더해진다 */
	UPROPERTY(Config, EditAnywhere, Category = "Directional Attack", meta = (ClampMin = "0.0"))
	float BaseHeadAttackDamageBonus = 0.20f;

	/**
	 * 저스트 가드 창 (초). G를 누른 순간부터 이 시간 안에 맞는 공격을 막는다. 모든 직업 공통 (같은 프레임 몽타주)
	 *   예) 0.5, 보스 공격이 10.00초 → 9.50 ~ 10.00에 누르면 성공. 9.75(0.25초 전)가 가장 안전
	 * 장부(ULOAJustGuardComponent, 클라 원본·서버 사본 둘 다)와 GA(실패 판단 시각)가 같은 값을 읽는다
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Just Guard", meta = (ClampMin = "0.05", Units = "Seconds"))
	float JustGuardWindow = 0.5f;

	/**
	 * 저스트 가드 방향 (도, 정면에서 좌우로). 공격 기준점이 캐릭터 정면 ±이 각도 안에 있어야 막는다 (2026-10-27)
	 *   공격 기준점 = 그 공격 모양의 기준점 (Sphere 중심 / Fan 꼭짓점 / Box 뒷변 가운데. 장판이면 장판 위치)
	 *   예) 45 → 보스가 정면에서 왼쪽 30° = 막음 / 60° = 못 막음
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Just Guard", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float JustGuardFacingHalfAngle = 45.f;

	/**
	 * 협동 저가 등급 (2026-11-18 apply128). 성공 인원(그 패턴의 저가 타격을 전부 막은 사람)이 이 수 이상이면 그 등급
	 *   예) 3 / 2 → 3명 Perfect, 2명 Great, 0~1명 Bad (ULOAJustGuardSubsystem::GetCoopJustGuardGrade)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Just Guard", meta = (ClampMin = "1"))
	int32 CoopJustGuardPerfectCount = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Just Guard", meta = (ClampMin = "1"))
	int32 CoopJustGuardGreatCount = 2;

	// ---------- 치명타 (2026-11-13, 사용자 결정) ----------
	// 모든 직업 공통 규칙이라 여기. 각인·버프로 "더해지는" 몫은 사람마다 → 스탯 (적중률 = Player의 ExtraCritRate, 피해 = Combat의 ExtraCritDamage, 기본 0)
	// 맞는 쪽(보스)의 치명타 저항 = Combat의 CritResistance (기본 0, 약점 노출이 깎음)
	// 플레이어가 적을 때릴 때만 굴린다. 보스는 치명타 개념이 없고, 회복·보호막도 치명타가 없다.

	/** 기본 치명타 적중률. 0.7 = 70%. 모든 직업 같음 (각인 시스템은 고민 중이라 일단 70%, 사용자) */
	UPROPERTY(Config, EditAnywhere, Category = "Critical", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseCritRate = 0.7f;

	/** 기본 치명타 피해 배율. 2.0 = 200% (로아 기본). 때린 쪽 ExtraCritDamage가 더해진다. 예) 2.0 + 0.5 = 2.5배 */
	UPROPERTY(Config, EditAnywhere, Category = "Critical", meta = (ClampMin = "1.0"))
	float BaseCritDamage = 2.0f;

	/** 백 어택 치명타 적중률 보너스. 0.1 = +10%p (로아 규칙). 백 어택 스킬로 뒤에서 쳤을 때 (백 어택 피해 보너스와 같은 조건) */
	UPROPERTY(Config, EditAnywhere, Category = "Critical", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackAttackCritRateBonus = 0.1f;

	// ---------- 대난투 (2026-11-18 apply125, 사용자 결정) ----------
	/**
	 * 대난투 진행 중 보스가 받는 피해 배율. 2.0 = 2배. 다른 배율(낙인·공증·치명타)과 곱한다 (사용자: 곱하기)
	 *   예) 300, 낙인 +10% → 300 × 1.1 × 2.0 = 660
	 *   보스에 State.Brawl.Active가 붙어 있을 때만 (ALOABrawlArena)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Brawl", meta = (ClampMin = "1.0"))
	float BrawlDamageMultiplier = 2.0f;
};
