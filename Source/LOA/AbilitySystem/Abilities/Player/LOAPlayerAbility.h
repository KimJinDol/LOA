// LOAPlayerAbility.h
// [배관] 플레이어 스킬의 부모 클래스. (평타, Q/W/E…, 아이덴티티)
// 몬스터/보스 Ability는 이 클래스가 아니라 ULOAMonsterAbility(예정)를 상속한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [플레이어만 하는 것]
//   예측 실행     : 키를 누른 사람 컴퓨터에서 먼저 실행 (LocalPredicted)
//   이동 금지     : 스킬을 쓰는 동안 우클릭 이동을 막음 (bBlockMove)
//   행동 금지     : 스킬을 쓰는 동안 다른 Ability를 막음 (bBlockAbility)
//   후딜          : 몽타주의 "후딜 시작" 지점부터 이동/다른 행동 허용 (평캔, 이동 캔슬)
//   발동 흐름     : 쿨타임/마나 확정 → 커서 쪽으로 회전 → StartSkill() (자식은 StartSkill만 덮어쓴다)
//   마나 소모     : 발동할 때 마나 검사 + 차감 (ManaCost)
//   아이덴티티    : 이 스킬로 게이지를 얼마나, 언제 채우나 (IdentityGain, IdentityGainTrigger)
//   UI 정보       : 스킬 슬롯·툴팁에 보일 아이콘, 이름, 설명 (SkillDisplayInfo)
//   툴팁 숫자     : 마나, 데미지 계수 읽기 (GetManaCostAtLevel, FindDamageRate). 쿨타임은 부모 GetCooldownAtLevel (2026-11-14)
//   도우미        : 내 캐릭터 / 내 PlayerController 가져오기
//
// ─────────────────────────────────────────────────────────────────────────────
// [기본값 설명]
//
// NetExecutionPolicy = LocalPredicted
//   누른 사람 컴퓨터에서 "먼저" 실행하고, 서버가 확인한다. (로아처럼 키를 누르면 바로 나가는 느낌)
//   서버가 거부하면 클라이언트 쪽 실행이 취소된다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [시전 중 이동 금지 - bBlockMove]
// 로아에서 우클릭으로 달리다가 Q를 누르면 그 자리에서 멈추고, 스킬이 끝날 때까지 움직이지 않는다. 그 동작이다.
//
//   Q 입력 → Ability 발동
//      PreActivate : ① 지금 가던 길 지우기 (조종하는 사람 컴퓨터에서만, ClickMoveComponent->StopMove())
//                    ② ASC에 State.Block.Move 태그 붙이기 → 클릭 이동 부품이 확인해서 새 이동을 막음
//      …시전 중 (우클릭을 눌러도 움직이지 않음)…
//      EndAbility  : ③ State.Block.Move 태그 떼기 → 우클릭을 누르고 있었다면 바로 다시 이동
//
// - ①만 하면 시전 중에 우클릭을 누를 때 다시 움직인다. 그래서 ②가 필요하다.
// - 두 동작은 항상 같이 켜고 끈다. (하나만 켜면 "멈췄다가 시전 중에 다시 걷는" 어정쩡한 스킬이 됨)
//   → 설정값 하나(bBlockMove)로 묶었다. 이동 중에 쓰는 스킬(이동기, 버프, 아이덴티티)은 이것만 끄면 된다.
// - 태그는 Activation Owned Tags(BP 설정)가 아니라 코드로 붙이고 뗀다. 그래야 체크박스 하나로 둘 다 제어된다.
//   (태그는 각 컴퓨터가 자기 ASC에 직접 붙이는 "로컬 태그"다. 클릭 이동은 조종하는 사람 컴퓨터에서 확인하므로 충분)
// - 기절 같은 상태 이상도 같은 태그를 쓴다. 태그는 개수를 세므로, 스킬이 끝나 하나 떼도 기절 태그가 남아 있으면 계속 금지.
// - PreActivate를 쓰는 이유: 자식이 ActivateAbility를 오버라이드하면서 Super 호출을 빼먹어도
//   이 동작은 항상 실행된다. (PreActivate는 ActivateAbility보다 먼저, 모든 Ability에서 호출됨)
//
// ─────────────────────────────────────────────────────────────────────────────
// [후딜 - Event.Montage.Unlock]
// 로아처럼 스킬 몽타주가 끝나기 전, "후딜"이 시작되면 이동이나 다른 행동으로 남은 동작을 끊을 수 있다.
//
//   Q ─[시전]──[타격]──[후딜 시작]────────────[몽타주 끝]
//      이동/다른 Ability 금지   │  여기부터 후딜
//                               ├ 아무것도 안 함 → 몽타주 끝까지 재생 → 정상 종료
//                               ├ 이동           → 남은 몽타주를 끊고 정상 종료 (이동 캔슬)
//                               └ 평타/스킬      → 새 몽타주가 끊음 → 정상 종료 (평캔)
//
// - "후딜 시작"은 몽타주에 찍은 LOA Send Gameplay Event 노티파이(Event.Montage.Unlock)로 알린다.
//   게임 로직용 노티파이이므로 Montage Tick Type은 반드시 Branching Point. (Queued는 데디케이티드 서버에서 두 번 울릴 수 있음)
// - 노티파이가 없는 몽타주는 끝날 때까지 잠금. (실수로 안 찍어도 안전한 쪽)
// - 후딜에서 끊기는 건 "취소"가 아니라 "정상 종료"다. 할 일(타격)은 이미 끝났기 때문.
//   후딜 전에 끊기면(기절, 회피기) 취소 종료.
// - 몽타주보다 오래 사는 Ability(아이덴티티: 켜져 있는 동안 = Ability가 살아 있는 동안)는 (2026-11-09)
//     bEndOnRecoveryMove = false (생성자) → 후딜 중 이동해도 끝나지 않는다
//     몽타주가 노티파이 전에 끊겨도 EnterRecovery()로 잠금을 직접 푼다 (안 풀면 Ability가 끝날 때까지 못 움직임)
//
// [행동 금지 - State.Block.Ability]
// - 모든 플레이어 Ability는 발동 차단 태그(ActivationBlockedTags)에 State.Block.Ability를 기본으로 가진다. (생성자)
//   → 이 태그가 붙어 있는 동안 다른 플레이어 Ability는 발동하지 않는다.
// - 예외: 회피(ULOAGA_Dodge)는 생성자에서 발동 차단 태그의 State.Block.Ability를 빼서 후딜 전에도 쓸 수 있다.
// - 이 태그는 "조종하는 사람 컴퓨터"에서만 붙인다. 서버는 붙이지 않는다. (= 서버는 검사하지 않음)
//   이유: 서버 몽타주는 조금 늦게 시작하므로 서버가 검사하면 타이밍 차이로 평캔이 거부되어 화면이 어긋날 수 있다.
//         PvE라 조작 동기가 약하고, 타격 전에 끊으면 데미지도 못 주므로 앞당겨도 이득이 거의 없다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [발동 흐름 - ActivateAbility는 여기서 끝낸다]
// 로아는 거의 모든 스킬이 "커서 쪽으로 돌고 나서" 나간다. 그래서 이 부분을 자식마다 쓰지 않고 여기 한 곳에 둔다.
//
//   ActivateAbility (이 클래스)
//      ├─ CommitAbility : 쿨타임 시작 + 마나 차감. 실패하면 취소
//      ├─ bFaceCursor = true  → [WaitCursorTarget] 커서 위치를 서버와 공유 → 그쪽으로 회전
//      │                         (CursorLocation에 위치 저장. 못 구하면 지금 보는 방향 그대로)
//      │  bFaceCursor = false → 바로 다음 단계 (버프, 아이덴티티 등 방향이 없는 Ability)
//      └─ BeginSkill()
//           ├─ ActivateSkillModules() : 스킬 모듈 시작 (이벤트 대기 등록. 몽타주보다 먼저)
//           └─ StartSkill()        ← 자식이 덮어쓰는 곳. 기본 동작은 "몽타주 재생 + 끝나면 종료"
//
//   예) Dodge::StartSkill       : 몽타주 + 밀기 이동
//       몽타주만 재생하면 되는 Ability는 StartSkill을 덮어쓸 필요도 없다.
//
// - 자식은 ActivateAbility를 덮어쓰지 않는다. (쿨타임/커서 순서가 스킬마다 달라지는 것을 막음)
// - 조종하는 사람 컴퓨터와 서버 모두 같은 커서 위치를 받아 같은 방향으로 돈 뒤 BeginSkill이 불린다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [마나 소모 - ManaCost]
//   Q 누름 → CanActivateAbility → CheckCost : 현재 마나 >= ManaCost ? 아니면 발동 거부
//          → ActivateAbility → CommitAbility → ApplyCost : 공용 GE(ULOAGE_ManaCost)에 -ManaCost를 넣어 적용
//
// - CheckCost를 직접 만드는 이유: GAS 기본 CheckCost는 GE Spec을 SetByCaller 값 "없이" 만들어 검사한다.
//   공용 GE는 값을 SetByCaller로 받으므로 기본 검사는 "값 없음" 오류 + 0으로 계산된다.
// - ManaCost가 0이면 검사/차감 모두 하지 않는다. (평타)
// - 레벨: 지금은 GetAbilityLevel() 값(=1)으로 꺼낸다. 캐릭터 레벨에 따라 오르게 하는 방법은 레벨업 작업 때 정한다.
// - 차지 중 계속 줄어드는 마나는 "소모"가 아니다. 차지 스킬이 시전 중에 따로 거는 효과로 만든다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [아이덴티티 게이지 수급 - IdentityGain] (2026-11-09 사용자 결정)
//   스킬마다 둘 중 하나:
//     적 타격 (OnEnemyHit) : 판정 1번에 적이 1명 이상 맞으면 +IdentityGain. 맞은 적 수와 무관, 판정마다 (콤보는 타마다)
//     사용     (OnUse)      : 몽타주 노티파이(LOA Send Gameplay Event, Event.Montage.IdentityGain)가 오면 +IdentityGain 1번
//                            맞힌 사람과 무관. 노티파이 전에 끊기면(선딜 중 CC 등) 안 참. 노티파이가 여러 개여도 1번
//   예) 낙인(적 타격 20) 헛침 → 0 / 맞힘 → +20, 기본 공격(적 타격 2) 4타 → +8
//       버프(사용 15) 허공 → +15 / 선딜 중 끊김 → 0
//   아이덴티티가 켜져 있으면(State.Identity.Active) 차지 않는다. 0이면 이 스킬로는 안 참
//   게이지는 서버만 바꾼다 (ULOAGE_IdentityGain::ApplyGaugeDelta)
//
// ─────────────────────────────────────────────────────────────────────────────
// [UI 정보 - SkillDisplayInfo] (2026-10-19 사용자 결정: 아이콘은 GA에)
//   "이 스킬이 어떻게 보이나"만 가진다. 어느 키에 놓이는지는 모른다. (키는 플레이어 꾸러미가 정한다)
//     예) GA_Berserker_Whirlwind : Icon = T_Icon_Whirlwind, Display Name = 휠윈드
//   쓰는 곳: 내 화면 위젯 컨트롤러가 "Q에 놓인 스킬"의 이 값을 슬롯 위젯에 보낸다. (ULOAOverlayWidgetController)
//   보스 패턴(ULOAMonsterAbility)은 이 부모가 아니라서 칸이 없다.
//   비워 두면(회피, 평타) 스킬 슬롯 목록에서 빠진다. 아이콘이 있으면 회피도 쓸 수 있다 (화면 가운데 쿨타임).
//   무력화 등급 (2026-11-18 apply121, 사용자 A): 툴팁의 "무력화 : 상" 한 줄. 표시용 분류라 숫자가 아니다
//     실제로 깎는 양은 Payload의 LOAGE_Stagger + CT_SkillValues 행 (숫자는 거기 한 곳)
//     예) 길로틴 스윙 = 상 (실제 30) / 스태빙 = 중 (실제 10 × 2타)
//     둘이 어긋나면 저장할 때 경고 (IsDataValid: 등급만 있고 무력화 Payload가 없음 / Payload는 있는데 등급이 없음)
//     등급을 수치에서 계산하지 않는 이유: 모듈 하나가 노티파이 여러 번 치면(휠윈드 4타) 횟수가 몽타주에만 있어 총량을 알 수 없다

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "ScalableFloat.h"
#include "LOAPlayerAbility.generated.h"

class ALOACharacter;
class ALOAPlayerController;
class ULOAClickMoveComponent;
struct FGameplayEventData;
struct FGameplayAbilityTargetDataHandle;
class UTexture2D;

/**
 * 무력화 등급 (툴팁 표시용, 2026-11-18 apply121). 로아처럼 하·중·상·최상
 * 글자("상" 등)는 툴팁 WBP 칸(Stagger Grade Texts)에 있다. UMETA DisplayName은 에디터 전용이라 게임 글자로 쓰지 않는다
 */
UENUM(BlueprintType)
enum class ELOAStaggerGrade : uint8
{
	None	UMETA(DisplayName = "None"),
	Low		UMETA(DisplayName = "Low"),
	Mid		UMETA(DisplayName = "Mid"),
	High	UMETA(DisplayName = "High"),
	Top		UMETA(DisplayName = "Top"),
};

/** 플레이어 스킬의 UI 정보 (슬롯 아이콘, 툴팁). 숫자(쿨타임, 데미지)는 넣지 않는다 */
USTRUCT(BlueprintType)
struct FLOASkillDisplayInfo
{
	GENERATED_BODY()

	/** 슬롯 아이콘. Soft 참조 → 위젯에서 Set Brush from Soft Texture로 읽는다. 비우면 슬롯 목록에서 빠진다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** 스킬 이름 (예: 휠윈드) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|UI")
	FText DisplayName;

	/**
	 * 스킬 설명 (툴팁, 나중에 스킬창). 자리표시를 쓰면 숫자가 자동으로 들어간다 (2026-11-14)
	 *   {Cooldown} 쿨타임 초 / {Mana} 마나 / {Damage} 공격력(BaseValue) × 첫 DamageRate = 한 번 맞을 때 데미지
	 *   예) "주변 적을 4회 베어 회당 {Damage}의 피해를 준다." → "… 회당 2,500의 피해를 준다."
	 *   값이 없으면(데미지 없는 스킬의 {Damage}, 로비처럼 공격력을 모를 때) 빈칸. 채우는 곳 = LOATooltipFormat::FillSkillSlotInfo
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|UI", meta = (MultiLine = "true"))
	FText Description;

	/**
	 * 무력화 등급 (툴팁 "무력화 : 상" 줄, 2026-11-18). None이면 줄을 숨긴다
	 * 표시용이다. 실제 양은 Payload의 LOAGE_Stagger (헤더 [UI 정보]). 어긋나면 저장할 때 경고
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|UI")
	ELOAStaggerGrade StaggerGrade = ELOAStaggerGrade::None;
};


/** 아이덴티티 게이지가 언제 차나 (헤더 [아이덴티티 게이지 수급]) */
UENUM(BlueprintType)
enum class ELOAIdentityGainTrigger : uint8
{
	/** 판정 1번에 적이 1명 이상 맞을 때마다 */
	OnEnemyHit	UMETA(DisplayName = "On Enemy Hit"),
	/** 몽타주 노티파이(Event.Montage.IdentityGain)가 올 때 1번 */
	OnUse		UMETA(DisplayName = "On Use"),
};

UCLASS(Abstract)
class LOA_API ULOAPlayerAbility : public ULOAGameplayAbility
{
	GENERATED_BODY()

public:
	ULOAPlayerAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 도우미 ----------
	// 전부 "지금 이 Ability를 가진 쪽"을 기준으로 한다. 없으면 nullptr.

	/** 이 Ability를 실행 중인 캐릭터 (ASC의 Avatar) */
	UFUNCTION(BlueprintPure, Category = "LOA|Ability")
	ALOACharacter* GetLOACharacterFromActorInfo() const;

	/**
	 * 이 캐릭터를 조종하는 PlayerController.
	 * 서버와 "조종하는 사람 컴퓨터"에서만 값이 있다. 다른 사람 컴퓨터에서는 nullptr. (PlayerController의 존재 위치)
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Ability")
	ALOAPlayerController* GetLOAPlayerControllerFromActorInfo() const;

	/**
	 * 이번 발동에서 커서 위치를 얻었으면 true + 위치. (장판 설치처럼 위치가 필요한 스킬이 쓴다)
	 * 발동할 때 받은 위치 하나뿐이다. 콤보의 "타마다 커서 방향"(LOAGA_Combo)은 회전만 하고 이 값을 바꾸지 않는다.
	 * public인 이유: Spawn 모듈(Spawn Location = Cursor)이 주인 Ability를 이 클래스로 Cast해서 부른다.
	 */
	bool GetCursorLocation(FVector& OutLocation) const;

	/** 이 스킬의 UI 정보 (아이콘, 이름, 설명). 클래스 기본값이라 어느 컴퓨터에서나 같다 */
	const FLOASkillDisplayInfo& GetSkillDisplayInfo() const { return SkillDisplayInfo; }

	// ---------- 툴팁 숫자 (2026-11-14) : UI가 클래스 기본값에서 읽기만 한다 ----------

	/** 그 레벨의 마나 소모. 0 = 소모 없음 */
	float GetManaCostAtLevel(float Level) const { return ManaCost.GetValueAtLevel(Level); }

	/**
	 * 모듈 순서 → Payload 순서로 훑어 처음 나오는 SetByCaller.DamageRate 값 (그 레벨). 없으면 false
	 * = 한 번 맞을 때의 계수 (사용자 A). 차지 배율은 넣지 않는다 (1단계 기준, 대신 정함)
	 *   예) 휠윈드 Modules[0] Apply : DamageRate 2.5 → 2.5 (노티파이가 4개여도 2.5. 횟수는 설명 글에)
	 *       단두대 Modules[0] Spawn : BP_Area_GuillotineSwing에 넘기는 Payloads의 DamageRate
	 */
	bool FindDamageRate(float Level, float& OutDamageRate) const;

	/** 모듈의 Payload 중 SetByCaller.Stagger 값이 있나 (무력화가 있는 스킬인가, 2026-11-18). 등급 칸과 맞는지 저장할 때 본다 */
	bool HasStaggerPayload() const;

	//~ Begin UGameplayAbility Interface
	/** 현재 마나가 ManaCost 이상인지. 부족하면 false → 발동 거부 */
	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 공용 마나 GE에 -ManaCost를 넣어 나에게 적용한다. (CommitAbility가 부름) */
	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	//~ End UGameplayAbility Interface

	//~ Begin ULOAGameplayAbility Interface
	/** 적 타격형이면 적이 맞았을 때 아이덴티티 게이지 + (헤더 [아이덴티티 게이지 수급]) */
	virtual void NotifyPayloadsApplied(bool bHitEnemy) override;
	//~ End ULOAGameplayAbility Interface

#if WITH_EDITOR
	/** 부모 검사 + 사용형 수급인데 이 GA의 몽타주에 Event.Montage.IdentityGain 노티파이가 없으면 경고 (몽타주를 직업 표에서 고르면 검사 못 함) */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	//~ Begin UGameplayAbility Interface
	/**
	 * 발동 흐름 전체: 쿨타임/마나 확정 → (bFaceCursor면) 커서 쪽으로 회전 → StartSkill().
	 * 자식은 이 함수 대신 StartSkill을 덮어쓴다.
	 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** ActivateAbility 직전에 모든 플레이어 Ability에서 호출된다. 이동 금지 시작을 여기서 한다. */
	virtual void PreActivate(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	/** Ability가 끝날 때 (정상 종료든 취소든) 호출된다. 이동/행동 금지 해제, 후딜 정리를 여기서 한다. */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	//~ Begin ULOAGameplayAbility Interface
	/** 몽타주가 끊김. 후딜 중이면 정상 종료(평캔), 아니면 부모 처리(취소 종료) */
	virtual void OnMontageCancelled() override;
	//~ End ULOAGameplayAbility Interface

	// ---------- 자식이 덮어쓰는 곳 ----------
	/**
	 * 실제 스킬 동작 시작. 쿨타임/마나 확정과 커서 회전이 끝난 뒤 호출된다. (조종하는 사람 컴퓨터와 서버 각각)
	 * 기본: 몽타주 재생 + 끝나면 종료. 몽타주가 없으면 바로 정상 종료.
	 * 덮어쓸 때 몽타주가 필요하면 PlayMontageAndEnd()를 직접 부른다.
	 */
	virtual void StartSkill();

	/**
	 * 잠금을 처음 상태로 다시 건다. 한 번의 발동 안에서 "다음 동작"이 시작될 때 쓴다. (예: 콤보 다음 타)
	 *   ① 후딜 상태 끝내기 (이동 캔슬 구독 해제, bInRecovery = false)
	 *   ② 이동/행동 금지 태그를 다시 붙이고, 후딜 시작(Event.Montage.Unlock)을 다시 기다림 (발동 때와 같은 규칙)
	 * 이미 잠겨 있으면(후딜 전) 뗐다가 다시 붙이므로 결과는 같다.
	 * 클릭 이동은 멈추지 않는다. (후딜에서 이동하면 이미 Ability가 끝났으므로 멈출 이동이 없다)
	 */
	void RestartLock();

	// ---------- 데이터 (각 Ability BP의 Class Defaults에서 조정) ----------

	/**
	 * 시전하는 동안 클릭 이동을 막을지. (발동할 때 정지 + 끝날 때까지 이동 금지)
	 * 기본 true (대부분의 스킬은 제자리에서 나감). 이동 중 쓰는 Ability는 false.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability")
	bool bBlockMove = true;

	/**
	 * 시전하는 동안 다른 플레이어 Ability를 막을지. (후딜 시작 전까지)
	 * 기본 true. 버프처럼 쓰는 중에도 다른 스킬을 써야 하는 Ability는 false.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability")
	bool bBlockAbility = true;

	/**
	 * 발동할 때 커서 쪽으로 돌지. 기본 true (로아는 거의 모든 스킬이 커서 방향).
	 * 방향이 없는 Ability(버프, 아이덴티티 변신 등)는 false → 커서를 기다리지 않고 바로 StartSkill.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability")
	bool bFaceCursor = true;

	/** 커서 트레이스 채널. 클릭 이동과 같은 채널을 쓴다. (bFaceCursor일 때) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability", meta = (EditCondition = "bFaceCursor"))
	TEnumAsByte<ECollisionChannel> CursorTraceChannel = ECC_Visibility;

	/** 마나 소모량. 0이면 소모 없음. 커브를 지정하면 레벨에 따라 바뀐다. */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Cost")
	FScalableFloat ManaCost;

	/** 이 스킬이 채우는 아이덴티티 게이지 양. 0이면 안 참. 헤더 [아이덴티티 게이지 수급] */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Identity")
	FScalableFloat IdentityGain;

	/** 아이덴티티 게이지가 언제 차나: 적 타격(판정마다) / 사용(노티파이 Event.Montage.IdentityGain에 1번) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|Ability|Identity")
	ELOAIdentityGainTrigger IdentityGainTrigger = ELOAIdentityGainTrigger::OnEnemyHit;

	/** [서버] 아이덴티티 게이지 +IdentityGain. 켜져 있거나(State.Identity.Active) 0이면 아무것도 안 함 */
	void GrantIdentityGain(const FGameplayAbilityActorInfo* ActorInfo, float Level) const;

	/** [서버] 사용형 수급 노티파이 기다리기 시작 (PreActivate. 후딜 대기와 같은 방식) */
	void ListenIdentityGainEvent(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo);

	/** 사용형 수급 노티파이 대기 해제 (받았을 때 / 종료) */
	void StopListeningIdentityGain();

	/** 사용형 수급 노티파이가 왔다 → 1번 채우고 대기 해제 */
	void OnIdentityGainEvent(const FGameplayEventData* Payload);

	/** 사용형 수급 노티파이 대기 핸들 */
	FDelegateHandle IdentityGainEventHandle;

	/** 스킬 슬롯·툴팁에 보일 아이콘, 이름, 설명. 헤더 [UI 정보] */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI")
	FLOASkillDisplayInfo SkillDisplayInfo;

	// ---------- 후딜 (자식용) ----------
	/**
	 * 후딜 중 이동하면 Ability를 끝낼지. 기본 true (이동 캔슬)
	 * 몽타주보다 오래 사는 Ability(아이덴티티)는 생성자에서 false: 끝나면 효과까지 꺼지므로 (헤더 [후딜], 2026-11-09)
	 * C++에서만 정한다 (사용자 결정: BP 칸으로 열지 않음. 스킬 데이터가 아니라 클래스 성질)
	 */
	bool bEndOnRecoveryMove = true;

	/**
	 * 후딜 상태로: 잠금(이동/행동 금지) 해제 + (bEndOnRecoveryMove면) 이동 캔슬 구독. 이미 후딜이면 아무것도 안 함
	 * 보통은 Event.Montage.Unlock 노티파이가 부른다. 노티파이 전에 몽타주가 끊겨도 Ability가 남는 자식이 직접 부른다
	 */
	void EnterRecovery();

private:
	// ---------- 툴팁 숫자 ----------
	/** 모듈 → Payload → 값 순서로 훑어 처음 나오는 ValueTag 값 (그 레벨). 없으면 false. FindDamageRate·HasStaggerPayload가 같이 쓴다 */
	bool FindFirstSetByCallerValue(const FGameplayTag& ValueTag, float Level, float& OutValue) const;

	// ---------- 스킬 시작 ----------
	/**
	 * 스킬 시작 = 모듈 켜기 → StartSkill.
	 * 커서 회전이 끝난 뒤(또는 bFaceCursor가 꺼져 있으면 Commit 직후) 여기로 들어온다.
	 * 모듈 켜기를 StartSkill 밖에 두는 이유: 자식이 StartSkill을 덮어쓰면서 Super를 빠뜨려도 모듈은 항상 켜진다.
	 * (모듈의 이벤트 대기가 몽타주보다 먼저 등록돼야 첫 노티파이를 놓치지 않는다)
	 */
	void BeginSkill();

	// ---------- 커서 ----------
	/** 커서 위치 도착 → 저장 + 회전 → BeginSkill */
	UFUNCTION()
	void OnCursorTargetReady(const FGameplayAbilityTargetDataHandle& DataHandle);

	/** 커서 위치를 못 구함 → 회전 없이 BeginSkill */
	UFUNCTION()
	void OnCursorTargetFailed(const FGameplayAbilityTargetDataHandle& DataHandle);

	/** 이번 발동의 커서 위치 (bHasCursorLocation이 true일 때만 의미 있음) */
	FVector CursorLocation = FVector::ZeroVector;
	bool bHasCursorLocation = false;

	/** 로컬(조종하는 사람 컴퓨터)이면 클릭 이동을 멈춘다. */
	void StopClickMove(const FGameplayAbilityActorInfo* ActorInfo) const;

	/**
	 * 잠금 걸기: 이동/행동 금지 태그 붙이기 + 후딜 시작 대기 등록.
	 * 발동 때(PreActivate)와 다음 동작 때(RestartLock)가 같은 규칙을 쓰도록 한 곳에 둔다.
	 */
	void ApplyLock(const FGameplayAbilityActorInfo* ActorInfo);

	// ---------- 후딜 ----------
	/** "후딜 시작" 이벤트(Event.Montage.Unlock)가 왔다 → EnterRecovery */
	void OnUnlockEvent(const FGameplayEventData* Payload);

	/** 후딜 중에 이동이 시작됐다 → 남은 몽타주를 끊고 정상 종료 */
	void OnMoveStartedInRecovery();

	/** 붙였던 이동/행동 금지 태그를 뗀다. (후딜 시작, 종료 때) 붙인 적이 없으면 아무것도 안 함 */
	void RemoveBlockTags();

	/** 후딜 이벤트 대기, 이동 시작 알림 구독을 모두 해제한다. (종료 때) */
	void StopListening();

	/**
	 * 이번 발동에서 State.Block.Move 태그를 붙였는지.
	 * EndAbility가 두 번 불려도(BlendOut 후 Completed 등) 태그를 한 번만 떼기 위해 기억한다.
	 */
	bool bBlockMoveTagAdded = false;

	/** 이번 발동에서 State.Block.Ability 태그를 붙였는지. (조종하는 사람 컴퓨터에서만 붙인다) */
	bool bBlockAbilityTagAdded = false;

	/** 후딜 중인가. (Event.Montage.Unlock을 받은 뒤 ~ 종료) */
	bool bInRecovery = false;

	/** 후딜 이벤트 대기 등록 번호표 (ASC의 GameplayEvent 알림) */
	FDelegateHandle UnlockEventHandle;

	/** 이동 시작 알림 구독 번호표 + 구독한 클릭 이동 부품 */
	FDelegateHandle MoveStartedHandle;
	TWeakObjectPtr<ULOAClickMoveComponent> ListenedClickMove;
};