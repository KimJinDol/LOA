// LOAGameplayTags.h
// C++ 코드가 "직접 이름을 알아야 하는" 태그만 여기에 선언한다. (네이티브 태그)
//
// [규칙]
// - 여기에는 "시스템 뼈대"가 반응하는 태그만 둔다. (예: 이동/시점 입력)
// - 스킬, 직업, 보스 패턴처럼 콘텐츠가 늘어날 때마다 생기는 태그는
//   Config/Tags/*.ini 에 추가한다. → C++ 수정/재컴파일 없이 태그를 늘릴 수 있다.
// - 태그는 "값"이 아니라 "이름표"다. 수치나 에셋을 태그에 담지 않는다.
// - 한 태그는 한 곳에만 정의한다 (여기 또는 ini). 같은 뿌리가 둘로 나뉘면 ini 맨 위 주석에 "C++에 있는 것"을 적는다.
//
// [뿌리] (2026-11-09 태그 정리, 사용자 결정)
//   Ability.<주인>.<이름>   어떤 스킬인가. 주인 = 직업 / Boss / Common(모든 직업 공용: 회피·기상·저가·사망·스턴·띄움·경직)
//   Ability.Trait.<성질>    어떤 성질의 Ability인가. 공용 GA는 직업 태그 대신 이것으로 고른다 (예: KeepOnCC)
//   Cooldown.<주인>.<이름>  쿨타임 중 (Ability와 끝 이름 같게)
//   Event.Montage.*         몽타주 노티파이 / Event.Trigger.* GA를 켜는 신호 / Event.Combat.* / Event.Skill.*
//   State.CrowdControl.*    걸린 CC: Push(피격 이상) / Paralysis(경직) / Status(상태 이상)
//   State.Immune.*          막는 CC: 위와 같은 종류 이름
//   State.Buff.* / State.Debuff.*  GE 버프·디버프 (죽으면 지움). "Debuff"는 이 뜻으로만 쓴다
//   State.Block.* 행동 금지 / State.Window.* 보스 판정 구간 / State.Body.* 몸 상태 / State.Identity.* 아이덴티티
//   SetByCaller.* / Skill.Attribute.* / Location.* / Class.* / Montage.* / InputTag.*
//   옛 이름 → DefaultGameplayTags.ini 리다이렉트

#pragma once

#include "NativeGameplayTags.h"

namespace LOAGameplayTags
{
	// ---------- Input: Native (캐릭터가 직접 처리하는 입력) ----------
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look);

	// ---------- SetByCaller: 코드가 GE에 값을 넣을 때 쓰는 이름표 ----------
	// GE 에셋의 Modifier(Set By Caller)와 C++ 코드가 "같은 이름"으로 값을 주고받아야 하므로 네이티브로 둔다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_DamageRate);	// 스킬 데미지 배율 (공격력의 몇 배인지)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ManaCost);	// 마나 소모량 (ULOAGE_ManaCost)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cooldown);	// 쿨타임 시간 (ULOAGE_Cooldown)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);	// 유지 시간 (ULOAGE_TimedTags, 경직·스턴 시간)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ChargeMultiplier);	// 차지 배율 (모든 Payload GE에 항상 들어감. 차지 아니면 1)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LaunchHeight);	// (안 씀, 10-21) 띄움 높이는 ULOAGE_Airborne의 Launch Height로 고정. 옛 Payload가 깨지지 않게 태그만 남김
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LaunchDistance);	// 띄움·경직 수평 거리 cm (ULOAGE_CrowdControl. 없으면 GE의 Default Distance)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_DownDuration);	// 띄움 후 착지해서 누워 있는 시간 초 (ULOAGE_Airborne. 없으면 GE의 Default Down Duration)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_HealRate);	// 회복 배율 (힐러 공격력의 몇 배를 회복). ULOAHealExecution (2026-11-07)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ShieldRate);	// 보호막 배율 (힐러 공격력의 몇 배를 보호막으로). ULOAShieldMagnitude (2026-11-07)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ShieldUsed);	// 보호막 하나가 이미 깎인 양. 맞을 때 Common 스탯창이 고친다 (스킬 데이터에 넣지 않음)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_IdentityGain);	// 아이덴티티 게이지 변화량 (+ 수급, − 감소·환급). ULOAGE_IdentityGain / ULOAGE_IdentityDrain (2026-11-09)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_HealthChange);	// 체력 변화량, 음수 = 깎기 (ULOAGE_HealthChange). 대난투 누적 피해를 돌아온 뒤 적용 (2026-11-18 apply125)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stagger);	// 무력화 수치. 무력화가 있는 스킬 Payload의 ULOAGE_Stagger에 넣는다 (2026-11-18)

	// ---------- State: ASC에 "붙어 있는 동안" 유지되는 상태 중 코드가 직접 확인하는 것 ----------
	// 클릭 이동 부품이 이 태그를 직접 확인하므로 네이티브로 둔다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Block_Move);

	// 붙어 있는 동안 다른 플레이어 Ability 발동 불가. (스킬 후딜 시작 전까지)
	// 모든 플레이어 Ability의 발동 차단 태그(ActivationBlockedTags)에 기본으로 들어 있다. 회피(ULOAGA_Dodge)만 생성자에서 뺀다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Block_Ability);

	// 붙어 있는 동안 보스·몬스터의 몸 막이(ULOABodyBlockerComponent)를 뚫고 지나간다. (관통 스킬, 예: 베스티지)
	// Move 모듈(Pass Through Body Blockers)이 이동하는 동안 로컬 태그로 붙인다. 플레이어 이동 컴포넌트가 직접 확인하므로 네이티브.
	// 버프(GE)로 줘도 된다. (예: 유령화)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Body_PassThroughBodyBlock);

	// 붙어 있는 동안 몸이 사라진다: 안 보이고, 맞지 않고(IsTargetable), 몸 막이가 꺼진다.
	// 예) 보스가 하늘로 뛰어올라 사라진 구간 (패턴 D). 몽타주의 "LOA Grant Tags" 구간 노티파이로 붙인다 (서버 GE → 모두에게 복제).
	// 숨기기·몸 막이 끄기는 각 컴퓨터가 이 태그를 보고 각자 한다. (ALOABossCharacter)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Body_Hidden);

	// 그로기 (2026-11-18 apply122): 무력화를 다 깎여 대난투가 도는 동안 보스에 붙어 있다 (ULOAGA_BossBrawl이 켜져 있는 동안)
	// 보스 StateTree "대난투" 상태가 이 태그로 들어가고 나온다 (LOA Has Tag) → 그동안 보스는 아무 패턴도 안 한다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Body_Groggy);

	// ---------- 대난투 (2026-11-18 apply125). 보스에 붙는다. 붙이고 떼는 곳 = ALOABrawlArena ----------
	// 진행 중 (다른 차원에서 딜하는 동안): 받는 피해 × Brawl Damage Multiplier (ULOADamageExecution, Project Settings → LOA Combat)
	//   피해는 체력이 아니라 보스 스탯창의 대난투 입구(BrawlDamage)로 간다 → 체력은 원래 차원에 돌아온 뒤 한 번에 (사용자 11-18)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Brawl_Active);

	// ---------- State.Immune: 붙어 있는 동안 해당 종류의 효과를 받지 않음 (데미지는 받는다) ----------
	// 나중에 피격 반응 / 상태 이상을 거는 코드가 이 태그를 직접 확인하므로 네이티브로 둔다.
	// 붙이는 방법: Ability가 실행되는 동안 내내 → GA의 Activation Owned Tags (회피는 생성자에서 기본값으로 넣음)
	//            몽타주의 일부 구간만 → "LOA Grant Tags" 노티파이 스테이트 (서버가 ULOAGE_TimedTags로 붙임. 보스 카운터 구간 등)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immune_Push);	// 피격 이상 면역 (Push Immunity). 띄움·다운 등 + 경직까지 막음
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immune_Status);	// 상태 이상 면역 (Status Immunity). 기절, 빙결, 공포 등
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immune_Paralysis);	// 경직 면역 (Paralysis Immunity, 슈퍼아머). 경직만 막음. 띄움은 못 막음

	// ---------- State.Body.Dead: 죽어 있는 동안 붙어 있음 ----------
	// GA_Death가 서버에서 GE(ULOAGE_InfiniteTags)로 붙인다 → 태그는 모든 컴퓨터에 복제된다. (Minimal 모드도 태그는 보냄)
	// 코드가 직접 확인하므로 네이티브:
	//   - 모든 Ability의 발동 차단 태그 (ULOAGameplayAbility 생성자. GA_Death만 뺌)
	//   - 맞을 수 있는 대상인가 (ULOACombatLibrary::IsTargetable)
	//   - 몸 반응 (ULOADeathReactionComponent: 캡슐 충돌 끄기, 이동 멈춤)
	//   - AnimBP 죽음 상태 (ULOAAnimInstance의 태그 → 변수 연결은 BP 데이터라 코드는 이름을 모른다)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Body_Dead);

	// ---------- State.Body.Invulnerable: 무적 ----------
	// 붙어 있는 동안 적의 공격이 아무것도 주지 않는다 (데미지, 피격 이상, 상태 이상, "맞았다" 신호 모두).
	// 아군의 힐·버프는 그대로 받는다. 보스는 계속 이 플레이어를 노릴 수 있다 (대상 고르기와 무관).
	// 확인하는 곳: ULOACombatLibrary::ApplyPayloads (모든 공격이 지나가는 한 곳)
	// 붙이는 곳: 부활 직후 3초 (ALOACharacter::ReviveFromDeath, ULOAGE_TimedTags)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Body_Invulnerable);

	// ---------- 버프 / 디버프 (2026-11-07) ----------
	// 부모 태그. 이 아래 태그를 주는 GE는 죽을 때 전부 지워진다 (ULOAGA_Death)
	//   버프 GE는 State.Buff.* 하나, 디버프 GE는 State.Debuff.* 하나를 꼭 준다. 예) 공격력 버프 → State.Buff.AttackPower
	//   죽어도 남아야 하는 효과(레이드 내내 가는 디버프 등)는 이 아래에 두지 않는다 (나중에 다른 부모, 예: State.Persist)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Buff);
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Debuff);
	// 보호막이 걸려 있음. GE_Shield가 붙이는 태그. 맞을 때 깎을 보호막 GE를 이 태그로 찾는다 (ULOAAttributeSet_Common)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Buff_Shield);
	// 공격력 버프 중. GE_AttackBuff가 붙이는 태그 (표시용, 2026-11-07)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Buff_AttackPower);
	// 주는 피해 증가 중. 아이덴티티 오라 GE(GE_sp_IdentityDamageBuff)가 붙이는 태그 (표시용, 2026-11-10)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Buff_DamageDealt);
	// 이동속도 증가 중. 버서커 시너지 스킬이 쓴 사람에게 거는 GE가 붙이는 태그 (표시용, 2026-11-13)
	//   이름이 스킬이 아니라 효과인 이유: 다른 직업이 같은 효과를 가져도 아이콘 줄 하나를 같이 쓴다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Buff_MoveSpeed);
	// 낙인. 붙어 있는 동안 받는 피해 증가 (GE가 IncomingDamageBonus를 올림). 태그는 표시용 (보스 디버프 아이콘, apply90)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Debuff_Mark);
	// 치명타 저항 감소 (로아 "약점 노출"). 버서커 시너지 스킬이 맞은 적에게 거는 GE가 붙이는 태그 (표시용, 2026-11-13)
	//   GE가 맞은 쪽 Combat의 CritResistance를 내린다 → 플레이어 치명타 확률이 오른다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Debuff_CritResistance);
	// 약점 공격 (2026-11-18 apply124). 대난투 진행 중 보스에 붙는다 (ALOABrawlArena가 붙이고 뗌)
	//   붙어 있는 동안 백 / 헤드 어택 스킬은 어느 방향에서 쳐도 방향 검사를 통과한다 → 백·헤드 보너스, 백 어택 치명타 전부 (ULOACombatLibrary)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Debuff_WeakPoint);

	// ---------- 아이덴티티 (2026-11-09) ----------
	// 아이덴티티(오라 등)가 켜져 있음. 켜진 동안 게이지가 줄고, 스킬로 게이지가 차지 않는다 (ULOAGA_Identity가 감소 GE로 붙임)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Identity_Active);

	// ---------- 카운터 / 스턴 ----------
	// 붙어 있는 동안 카운터 스킬로 헤드를 치면 카운터 성공. 보스 패턴 몽타주의 "LOA Grant Tags" 구간으로 붙인다 (파란 빛 구간).
	// 보스 쪽 카운터 모듈(ULOASkillModule_Counter)이 직접 확인하므로 네이티브.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Window_Counterable);
	// 저스트 가드 구간 (노란 빛). 보스 패턴 몽타주의 "LOA Just Guard Window" 구간이 붙인다 (2026-10-26). 보스가 듣고 BP 연출
	// 플레이어가 누를 수 있나·막았나는 이 태그가 아니라 그 구간의 저가 ID로 정한다 (ULOAJustGuardSubsystem, ULOAJustGuardComponent)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Window_JustGuardable);
	// 스턴 중. 스턴 GE(ULOAGE_Stun)가 붙인다 (모든 컴퓨터에 복제, 2026-11-10). ULOAGA_Stun이 이 태그가 있는 동안 켜져 있다.
	// StateTree가 "스턴 상태"로 가는 조건 (LOA Has Tag).
	// 이름 규칙 (2026-10-18): 걸리는 효과는 State.CrowdControl.<종류>.<이름>, 막는 면역은 State.Immune.<종류>.
	//   종류 이름은 로아 영어판 용어를 따른다 (2026-10-21 사용자 결정):
	//     피격 이상 = Push (띄움·다운 등)   경직 = Paralysis   상태 이상 = Status (기절 등)
	//   (옛 이름 HitReaction / StatusEffect / Debuff → DefaultGameplayTags.ini 리다이렉트. Debuff는 2026-11-09에 Status로: State.Debuff(GE 디버프)와 겹쳐서)
	//   예) State.CrowdControl.Status.Stunned  ↔  State.Immune.Status
	//   "상태 이상 중인가?"는 부모 State.CrowdControl.Status 하나로 물으면 된다 (자식도 "있음").
	//   면역은 스턴 GE의 Ignore Tags(State.Immune.Status)가 판단한다 (ULOAGE_Stun [면역])
	// 상태 이상의 부모. 플레이어 Ability가 이 태그로 막힌다 (ULOAPlayerAbility, 2026-11-10)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Status);
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Status_Stunned);

	// ---------- 플레이어가 받는 CC: 피격 이상 (2026-10-19) ----------
	// 효과 State.CrowdControl.Push.* ↔ 면역 State.Immune.Push (회피 등)
	// 부모: 모든 플레이어 Ability의 발동 차단 태그 (ULOAPlayerAbility 생성자). 띄움·다운 중에는 스킬·회피가 안 나간다. 기상기만 예외
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Push);
	// 띄움. GE_CC_Airborne(부모 ULOAGE_Airborne)이 붙인다 (모든 컴퓨터에 복제). 붙는 순간 ULOAGA_Airborne이 발동.
	// 다 일어나면 ULOAGA_Airborne이 GE를 지운다. 플레이어 이동 컴포넌트가 이 태그 + 땅 위면 "미끄러짐" 제동을 쓴다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Push_Airborne);
	// 누워 있음 (띄움 후 착지 ~ 완전히 일어날 때까지). ULOAGA_Airborne이 착지 순간 서버에서 GE로 붙인다 (복제).
	// GE_CC_Airborne의 Ignore 조건 → 누워 있으면 다시 뜨지 않는다 (데미지는 들어감). 기상기(ULOAGA_WakeUp)의 발동 조건
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Push_Down);
	// 일어나는 중 (누운 시간이 끝나 기상 몽타주 구간). ULOAGA_Airborne이 서버에서 GE로 붙인다 (복제), 끝날 때 지움.
	// 맞은 사람 컴퓨터의 ULOAGA_Airborne이 이 태그를 보고 GetUp 구간으로 넘어간다. (누운 시간은 SetByCaller라 클라가 모름)
	// 이때는 Push.Down·Push.Airborne도 붙어 있어서, 부모(Push)로 묻는 쪽의 결과는 바뀌지 않는다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Push_GettingUp);

	// ---------- 플레이어가 받는 CC: 경직 (2026-10-21) ----------
	// 효과 State.CrowdControl.Paralysis ↔ 면역 State.Immune.Paralysis (슈퍼아머 스킬의 Activation Owned Tags), State.Immune.Push도 막는다
	// GE_CC_Paralysis(부모 ULOAGE_Paralysis)가 붙인다 (복제). 붙는 순간 ULOAGA_Paralysis가 발동.
	// 모든 플레이어 Ability의 발동 차단 태그 (ULOAPlayerAbility 생성자) → 경직 중엔 회피·스킬 전부 안 나감.
	// 피격 이상(Push)이 더 강하다: 띄움·누움 중엔 경직이 안 붙고, 경직 중 띄움을 맞으면 띄움이 경직을 끊는다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Paralysis);

	// ---------- Ability: 코드가 직접 이름을 아는 Ability 태그 ----------
	// ULOAGA_Airborne의 Asset Tag (생성자). 기상기가 Cancel Abilities with Tag로 띄움 GA를 끊을 때 쓴다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Common_Airborne);
	// ULOAGA_Paralysis의 Asset Tag (생성자). 나중에 경직을 끊는 기능이 생기면 이 태그로
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Common_Paralysis);
	// ULOAGA_Dodge의 Asset Tag (생성자). 저스트 가드 GA가 Block Abilities with Tag로 회피를 막을 때 쓴다 (2026-10-27)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Common_Dodge);
	// ULOAGA_Death / ULOAGA_Stun의 Asset Tag (생성자, 2026-11-09)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Common_Death);
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Common_Stun);

	// ---------- Ability.Trait: Ability의 성질 (2026-11-09) ----------
	// CC(띄움·경직·스턴)가 "다른 Ability 모두 취소"할 때 남긴다. 공용 CC GA 생성자의 AbilityTagsToKeep 기본값
	// 붙이는 쪽: 아이덴티티(ULOAGA_Identity 생성자). 사망은 이 태그도 취소한다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Trait_KeepOnCC);
	// ---------- Skill.Attribute: 공격 속성 (이 공격이 어떤 성질인가) ----------
	// 플레이어 GA의 Asset Tags에 넣는다. ULOAGameplayAbility::MakeSkillModuleContext가 Skill.Attribute 아래 태그만 골라
	// 상자(FLOASkillModuleContext::AttackTags)에 담고, 맞은 쪽에 가는 Event.Combat.Hit에 실려 간다.
	// 공격 쪽은 "이 공격은 이런 속성"만 알린다. 그 속성으로 무슨 일이 생기는지는 맞은 쪽이 정한다. (2026-10-18)
	// 백·헤드 어택 (2026-10-25): 이 속성이 붙은 스킬만 방향 보너스를 받는다. ULOACombatLibrary::ApplyPayload가 GE Spec에도 붙인다 (GE 조건 칸용).
	//   (2026-11-02) 방향 판단은 ApplyPayload 한 곳(ULOACombatLibrary::GetDirectionalAttack) → Effect Context에 결과. 데미지 계산은 결과만 읽는다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attribute);			// 부모. 상자에 담을 태그를 고를 때 쓴다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attribute_Counter);	// 카운터 스킬
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attribute_BackAttack);	// 백 어택 스킬: 맞은 쪽 뒤에서 치면 피해 보너스
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attribute_HeadAttack);	// 헤드 어택 스킬: 맞은 쪽 앞에서 치면 피해 보너스
	// 저스트 가드 (2026-10-26): 보스 패턴 GA의 Asset Tags에 넣는다. 이 속성 + 맞는 순간 열린 저가 구간 → 막을 수 있는 공격
	//   단타/홀딩은 태그가 아니라 저가 구간(LOA Just Guard Window)의 칸. 맞은 플레이어의 저가 컴포넌트(ULOAJustGuardComponent)가 읽는다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attribute_JustGuard);

	// ---------- Event: 코드가 직접 기다리는 몽타주 이벤트 ----------
	// "후딜 시작" 노티파이가 보낸다. ULOAPlayerAbility가 이 이름으로 기다리므로 네이티브로 둔다.
	// (타격 이벤트 Event.Montage.Hit는 BP에서 고르는 값이라 ini에 있다)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_Unlock);

	// "다음 타 입력 받기 시작" 노티파이가 보낸다. 콤보 GA(ULOAGA_Combo)가 이 이름으로 기다린다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_ComboOpen);

	// "아이덴티티 게이지 수급" 노티파이가 보낸다. 사용형 스킬(ULOAPlayerAbility, On Use)이 서버에서 이 이름으로 기다린다 (2026-11-09)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_IdentityGain);

	// GA가 코드로 보내는 신호 (몽타주 노티파이가 아님 → 저장 시 노티파이 확인 대상이 아님)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Charge_Started);		// 차지 시작 (조종하는 사람 컴퓨터만, 차지 게이지 UI용) (2026-10-31)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Charge_Released);	// 차지 발사 (뗌 / 최대 시간)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Charge_Cancelled);	// 차지 중 끊김 (회피·CC. 조종하는 사람 컴퓨터만, 차지 게이지 UI용) (2026-10-31)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Charge_Perfect);	// 차지 퍼펙트 존 안에서 발사

	// 체력이 "0보다 컸는데 0이 됨" (처음 한 번). ULOAAttributeSet_Common이 서버에서 자기 ASC에 보낸다.
	// GA_Death의 발동 조건(Trigger). 몽타주 노티파이가 아니므로 저장 시 노티파이 확인 대상이 아니다.
	// EventData: Instigator = 막타를 친 쪽 / Target = 죽은 몸 / EventMagnitude = 마지막 데미지
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Trigger_Death);

	// 무력화 게이지가 "0보다 컸는데 0이 됨" (처음 한 번). ULOAAttributeSet_Boss가 서버에서 보스 자기 ASC에 보낸다 (2026-11-18)
	// 사망 이벤트와 같은 방식. 듣는 곳: 대난투 GA (예정). 다시 채울 때까지 또 오지 않는다
	// EventData: Instigator = 마지막으로 무력화를 깎은 쪽 / Target = 보스
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Trigger_StaggerBroken);

	// 저가 패턴 하나가 끝남 (2026-11-18 apply128). 보스 Just Guard 모듈(ULOASkillModule_JustGuard)이 패턴이 끝날 때 보스 자신에게 보낸다
	// 끊겨서 끝나도 보낸다 (그때까지 센 것으로). 듣는 곳: 대난투 그로기 GA (협동 저가 등급 → 추가 시간)
	// EventData: Instigator = 보스 / EventMagnitude = 성공 인원 (그 패턴의 저가 타격을 전부 막은 사람 수)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_JustGuard_PatternResolved);

	// ---------- Event.Combat: 전투 중 서버가 보내는 신호 (몽타주 노티파이가 아님 → 저장 시 노티파이 확인 대상이 아님) ----------
	// 맞았다. ULOACombatLibrary::ApplyPayloads가 서버에서, Payload를 다 적용한 뒤 맞은 쪽 ASC에 보낸다. 적이 적을 쳤을 때만 (힐·버프는 안 보냄)
	// 판정 방식(Server / Victim)·투사체 상관없이, 맞은 걸로 최종 결정된 대상에게 한 번씩.
	// EventData: Instigator = 시전자 / Target = 맞은 쪽 / InstigatorTags = 공격 속성 (Skill.Attribute.*)
	// 듣는 곳: 보스 카운터 모듈(ULOASkillModule_Counter). 나중에 무력화 등
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Hit);

	// 카운터 성공. 카운터 모듈이 판정에 성공한 순간 보스 자신의 ASC에 보낸다. (지금은 듣는 곳 없음. 나중에 "카운터!" 글씨·소리)
	// EventData: Instigator = 카운터 친 플레이어 / Target = 보스
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Counter);

	// (Event.Trigger.Stun은 2026-11-10에 없앰: 스턴은 GE → 태그로 건다. ULOAGE_Stun)

	// ---------- Location: 위치 표시(ALOALocationMarker) 태그 중 코드가 직접 쓰는 것 ----------
	// 정비소 자리 = 전멸 뒤 부활 자리 (2026-10-25 사용자 결정). 없으면 각자 처음 스폰한 자리
	// 보스의 전투 시작 거리(1500)보다 멀리 둘 것. 정비소 출구 상자(ALOARepairExit) 밖에 둘 것 (도착하자마자 출발하지 않게)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Location_Raid_Respawn);

	// 전투 구역 입구. 정비소 출구로 나가면 여기로 옮겨진다. 표시의 오른쪽 방향으로 150cm씩 벌려 세움. 없으면 각자 처음 스폰한 자리
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Location_Raid_Entrance);
}