// LOAMonsterAbility.h
// [배관] 보스·몬스터 패턴의 부모 클래스. (내려찍기, 돌진, 점프 착지 …)
// 플레이어 스킬은 이 클래스가 아니라 ULOAPlayerAbility를 상속한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [플레이어 스킬과 같은 것 / 다른 것]
//
//                      플레이어 스킬 (ULOAPlayerAbility)     보스 패턴 (이 클래스)
//   어디서 실행        누른 사람 컴퓨터 먼저 + 서버          서버만
//   방향               커서 쪽                              노릴 대상(플레이어) 쪽
//   마나·이동 잠금·후딜  있음                                없음
//   스킬 모듈·판정 모양  씀                                  똑같이 씀  ← 스킬 시스템 하나로 플레이어와 보스를 만든다
//   쿨타임             공용 쿨타임 GE                        같은 것 (같은 패턴 연달아 쓰기 방지)
//
// ─────────────────────────────────────────────────────────────────────────────
// [발동 - TryActivatePattern]  "보스야, 이 패턴을 이 사람에게 써라"
//
//   TryActivatePattern(보스 ASC, Ability.Grux.Slam, 플레이어B)
//     → 보스가 가진 패턴 중 Asset Tags에 Ability.Grux.Slam이 있는 것을 찾는다
//     → GAS 이벤트 데이터(Target = 플레이어B)를 실어서 발동한다
//   부르는 쪽: 콘솔 명령 LOA.Boss.Pattern / StateTree의 LOA Run Pattern Task / 패턴 테스터 창
//   대상을 변수로 들고 있지 않는다. 발동마다 같이 넘어오고 패턴이 끝나면 버린다.
//
// [발동 전에 확인만 - CanActivatePattern]  "보스야, 지금 이 패턴 쓸 수 있어?" (발동은 안 함)
//   쿨타임 중 / 다른 패턴 중(State.Block.Ability) / 죽음 / Required인데 대상 없음 → false
//   부르는 쪽: LOA Run Pattern(후보 중 쓸 수 있는 것만 골라 무작위), LOA Can Use Pattern 조건(지연·중앙 복귀 상태에 들어갈지)
//   예) 포효 쿨타임 60초 중 → false → StateTree가 지연 상태를 건너뛰고 평소 패턴으로
//
// [노릴 대상 - Target Usage]  넘겨준 대상은 "이 사람을 노려 줘"라는 요청일 뿐. 쓸지는 패턴이 정한다.
//   None     : 대상을 안 쓴다. 넘겨줘도 무시.                   예) 포효, 무력화 체크 시작, 맵 전체 전멸기
//   Optional : 있으면 그쪽으로 돌고, 없으면 지금 방향 그대로.     예) 내려찍기 (기본값)
//   Required : 대상이 없으면 발동하지 않는다.                    예) 돌진 (노릴 사람이 없으면 의미 없음)
//   죽은 플레이어(State.Dead)는 "대상 없음"으로 본다. (ULOACombatLibrary::IsTargetable)
//   "장소"를 노리는 패턴(맵 중앙으로 점프 등)은 대상을 쓰지 않고 패턴이 장소를 데이터로 가진다. (생길 때 만든다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (전부 서버)
//
//   ActivateAbility (이 클래스)
//      ├─ 대상 확인        Required인데 대상이 없으면 → 취소
//      ├─ CommitAbility    쿨타임 시작. 쿨타임 중이면 애초에 발동이 거부된다
//      ├─ 이동 멈춤        걷던 중이면 그 자리에 선다 (bStopMovementOnStart)
//      ├─ 대상 쪽으로 회전  (bFaceTarget, 대상이 있을 때)
//      ├─ 스킬 모듈 시작   Apply / Spawn. 몽타주보다 먼저 (첫 노티파이를 놓치지 않게)
//      └─ StartSkill()     ← 자식이 덮어쓰는 곳. 기본 = 몽타주 재생 → 끝나면 종료
//                             이동(돌진·점프)은 Move 모듈, 카운터는 Counter 모듈로 한다. 자식 C++ 클래스는
//                             모듈로 안 되는 흐름이 생길 때만 만든다. (2026-10-18 정리: 옛 설명 "돌진·점프는 자식 C++"는 Move 모듈 전)
//
//   예) 내려찍기 GA_Grux_Slam (BP, C++ 없음)
//       Asset Tags = Ability.Grux.Slam / Montage = M_Grux_Slam / Cooldown 6초, Cooldown.Grux.Slam
//       Modules = Spawn (On Skill Start, BP_Area_Grux_Slam, Self 앞 300) → 바닥에 빨간 원 → 1.2초 뒤 판정
//
// - 자식은 ActivateAbility를 덮어쓰지 않는다. (플레이어 스킬과 같은 규칙. 순서가 패턴마다 달라지는 것을 막음)
//
// [한 번에 하나]
//   패턴이 도는 동안 State.Block.Ability가 붙고, 모든 패턴은 그 태그가 있으면 발동하지 않는다.
//   → 내려찍기 도중에 돌진이 끼어들지 않는다. (사망은 이 태그에 막히지 않음. GA_Death가 패턴을 취소한다)
//   보스는 서버에서만 패턴을 쓰므로 이 태그도 서버에만 있으면 된다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [카운터]  이 클래스는 카운터를 모른다. 카운터 패턴은 Modules에 Counter 모듈(ULOASkillModule_Counter)을 넣는다.
//   구간(언제 파랗나) = 몽타주 LOA Grant Tags(State.Counterable) / 판정·스턴 시간 = Counter 모듈 (2026-10-18)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   1. 서버만     : 발동, 회전, 판정, 데미지, 범위 액터 소환
//   2. 복제       : 보스 회전·위치(캐릭터 이동 복제), 몽타주(ASC 몽타주 복제), 범위 액터(액터 복제)
//   3. 각자 할 일 : 없음
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] GA_<보스>_<패턴> (부모: LOAMonsterAbility)
//   Tags                : Asset Tags = Ability.<보스>.<패턴>   ← TryActivatePattern이 이 태그로 찾는다 (필수)
//   LOA|Ability|Montage : Montage, Montage Play Rate
//   LOA|Ability|Cooldown: Cooldown Duration, Cooldown Tags (Cooldown.<보스>.<패턴>)
//   LOA|Ability|Modules : Apply / Spawn / Move 모듈. 카운터 패턴은 Counter 모듈 추가 (몽타주에 State.Counterable 구간도 칠한다)
//   LOA|Monster         : Target Usage, Face Target, Stop Movement On Start
//   꾸러미 등록          : DA_AbilityDataSet_Boss_<이름>의 Abilities에 추가 (InputTag 비움)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "LOAMonsterAbility.generated.h"

class UAbilitySystemComponent;

/** 패턴이 넘겨받은 대상을 어떻게 쓰나 */
UENUM(BlueprintType)
enum class ELOAPatternTargetUsage : uint8
{
	None		UMETA(DisplayName = "None (대상 안 씀)"),
	Optional	UMETA(DisplayName = "Optional (있으면 그쪽으로)"),
	Required	UMETA(DisplayName = "Required (없으면 발동 안 함)"),
};

UCLASS(Abstract)
class LOA_API ULOAMonsterAbility : public ULOAGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAMonsterAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 발동 창구 (StateTree, 콘솔 명령, 테스터가 부른다) ----------

	/**
	 * [서버] 보스(ASC)가 가진 패턴 중 Asset Tags에 PatternTag가 있는 것을 Target을 노리도록 발동한다.
	 * @param Target            노릴 대상. 없으면 nullptr (Target Usage가 None/Optional인 패턴만 발동됨)
	 * @param OutActivatedHandle 발동한 패턴의 번호표 (선택). StateTree가 "그 패턴이 끝났나"를 확인하거나 취소할 때 쓴다
	 * @return 발동했으면 true. 서버가 아님 / 그런 패턴 없음 / 쿨타임 중 / 다른 패턴 중 / Required인데 대상 없음 → false
	 */
	static bool TryActivatePattern(UAbilitySystemComponent* ASC, FGameplayTag PatternTag, AActor* Target,
		FGameplayAbilitySpecHandle* OutActivatedHandle = nullptr);

	/**
	 * [서버] 지금 이 패턴을 Target에게 쓸 수 있나. 발동하지 않고 확인만 한다. (헤더 [발동 전에 확인만])
	 * TryActivatePattern이 거부할 이유(쿨타임, 다른 패턴 중, 죽음, Required인데 대상 없음)를 미리 본다.
	 * @return 서버가 아님 / 그런 패턴 없음(경고) / 위 이유 하나라도 → false
	 */
	static bool CanActivatePattern(const UAbilitySystemComponent* ASC, FGameplayTag PatternTag, const AActor* Target);

	/** ASC가 가진 모든 패턴의 Asset Tags를 모은다. (콘솔 명령의 목록 출력, 나중에 테스터 창 버튼 목록) */
	static void GetPatternTags(const UAbilitySystemComponent* ASC, FGameplayTagContainer& OutTags);

	/** 넘겨받은 대상을 어떻게 쓰나 (패턴 테스터 창이 "대상 필수" 표시에 쓴다) */
	ELOAPatternTargetUsage GetTargetUsage() const { return TargetUsage; }

	/** 이번 발동에서 노리는 대상. 없으면 nullptr (Target Usage = None이거나 넘겨받지 못함) */
	const AActor* GetPatternTarget() const { return PatternTarget.Get(); }

	//~ Begin UGameplayAbility Interface
#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사: Asset Tags가 비어 있으면 TryActivatePattern으로 찾을 수 없다. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End UGameplayAbility Interface

protected:
	//~ Begin UGameplayAbility Interface
	/** 발동 흐름 템플릿. 자식은 덮어쓰지 않는다. (헤더 [흐름]) */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/**
	 * 패턴의 실제 동작. 자식이 덮어쓰는 곳.
	 * 기본: 몽타주 재생 → 끝나면 종료. 몽타주가 없으면 바로 종료.
	 * 이동·카운터처럼 모듈로 되는 것은 모듈로 한다. 모듈로 안 되는 흐름일 때만 덮어쓴다.
	 */
	virtual void StartSkill();

	// ---------- 데이터 (각 패턴 BP의 Class Defaults에서 조정) ----------

	/** 넘겨받은 대상을 어떻게 쓰나. 헤더 [노릴 대상] 참고 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Monster")
	ELOAPatternTargetUsage TargetUsage = ELOAPatternTargetUsage::Optional;

	/** 시작할 때 대상 쪽으로 돈다. (대상이 있을 때만. 높이 차이는 무시) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Monster",
		meta = (EditCondition = "TargetUsage != ELOAPatternTargetUsage::None", EditConditionHides))
	bool bFaceTarget = true;

	/** 시작할 때 걷던 이동을 멈춘다. (끄면 걸으면서 쓰는 패턴. 예: 걸으며 포효) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Monster")
	bool bStopMovementOnStart = true;

private:
	/** ASC가 가진 패턴 중 Asset Tags에 PatternTag가 있는 것. 없으면 경고 로그 + nullptr (Try·Can이 같이 쓴다) */
	static const FGameplayAbilitySpec* FindPatternSpec(const UAbilitySystemComponent* ASC, FGameplayTag PatternTag);

	/** 넘겨받은 대상을 Target Usage와 생존 여부로 거른다. 쓸 수 없으면 nullptr */
	const AActor* ResolveTarget(const FGameplayEventData* TriggerEventData) const;

	/** 이번 발동의 대상. 발동마다 새로 정하고 EndAbility에서 비운다. (대상이 사라지면 자동으로 빈다) */
	TWeakObjectPtr<const AActor> PatternTarget;
};
