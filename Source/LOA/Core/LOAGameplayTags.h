// LOAGameplayTags.h
// C++ 코드가 "직접 이름을 알아야 하는" 태그만 여기에 선언한다. (네이티브 태그)
//
// [규칙]
// - 여기에는 "시스템 뼈대"가 반응하는 태그만 둔다. (예: 이동/시점 입력)
// - 스킬, 직업, 보스 패턴처럼 콘텐츠가 늘어날 때마다 생기는 태그는
//   Config/Tags/*.ini 에 추가한다. → C++ 수정/재컴파일 없이 태그를 늘릴 수 있다.
// - 태그는 "값"이 아니라 "이름표"다. 수치나 에셋을 태그에 담지 않는다.

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
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);	// 태그 유지 시간 (ULOAGE_TimedTags)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ChargeMultiplier);	// 차지 배율 (모든 Payload GE에 항상 들어감. 차지 아니면 1)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LaunchHeight);	// (안 씀, 10-21) 띄움 높이는 ULOAGE_Airborne의 Launch Height로 고정. 옛 Payload가 깨지지 않게 태그만 남김
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LaunchDistance);	// 띄움·경직 수평 거리 cm (ULOAGE_CrowdControl. 없으면 GE의 Default Distance)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_DownDuration);	// 띄움 후 착지해서 누워 있는 시간 초 (ULOAGE_Airborne. 없으면 GE의 Default Down Duration)

	// ---------- State: ASC에 "붙어 있는 동안" 유지되는 상태 중 코드가 직접 확인하는 것 ----------
	// 클릭 이동 부품이 이 태그를 직접 확인하므로 네이티브로 둔다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Block_Move);

	// 붙어 있는 동안 다른 플레이어 Ability 발동 불가. (스킬 후딜 시작 전까지)
	// 모든 플레이어 Ability의 발동 차단 태그(ActivationBlockedTags)에 기본으로 들어 있다. 회피(ULOAGA_Dodge)만 생성자에서 뺀다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Block_Ability);

	// 붙어 있는 동안 보스·몬스터의 몸 막이(ULOABodyBlockerComponent)를 뚫고 지나간다. (관통 스킬, 예: 베스티지)
	// Move 모듈(Pass Through Body Blockers)이 이동하는 동안 로컬 태그로 붙인다. 플레이어 이동 컴포넌트가 직접 확인하므로 네이티브.
	// 버프(GE)로 줘도 된다. (예: 유령화)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_PassThroughBodyBlock);

	// 붙어 있는 동안 몸이 사라진다: 안 보이고, 맞지 않고(IsTargetable), 몸 막이가 꺼진다.
	// 예) 보스가 하늘로 뛰어올라 사라진 구간 (패턴 D). 몽타주의 "LOA Grant Tags" 구간 노티파이로 붙인다 (서버 GE → 모두에게 복제).
	// 숨기기·몸 막이 끄기는 각 컴퓨터가 이 태그를 보고 각자 한다. (ALOABossCharacter)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hidden);

	// ---------- State.Immune: 붙어 있는 동안 해당 종류의 효과를 받지 않음 (데미지는 받는다) ----------
	// 나중에 피격 반응 / 상태 이상을 거는 코드가 이 태그를 직접 확인하므로 네이티브로 둔다.
	// 붙이는 방법: Ability가 실행되는 동안 내내 → GA의 Activation Owned Tags (회피는 생성자에서 기본값으로 넣음)
	//            몽타주의 일부 구간만 → "LOA Grant Tags" 노티파이 스테이트 (서버가 ULOAGE_TimedTags로 붙임. 보스 카운터 구간 등)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immune_Push);	// 피격 이상 면역 (Push Immunity). 띄움·다운 등 + 경직까지 막음
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immune_Debuff);	// 상태 이상 면역 (Debuff Immunity). 기절, 빙결, 공포 등
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immune_Paralysis);	// 경직 면역 (Paralysis Immunity, 슈퍼아머). 경직만 막음. 띄움은 못 막음

	// ---------- State.Dead: 죽어 있는 동안 붙어 있음 ----------
	// GA_Death가 서버에서 GE(ULOAGE_InfiniteTags)로 붙인다 → 태그는 모든 컴퓨터에 복제된다. (Minimal 모드도 태그는 보냄)
	// 코드가 직접 확인하므로 네이티브:
	//   - 모든 Ability의 발동 차단 태그 (ULOAGameplayAbility 생성자. GA_Death만 뺌)
	//   - 맞을 수 있는 대상인가 (ULOACombatLibrary::IsTargetable)
	//   - 몸 반응 (ULOADeathReactionComponent: 캡슐 충돌 끄기, 이동 멈춤)
	//   - AnimBP 죽음 상태 (ULOAAnimInstance의 태그 → 변수 연결은 BP 데이터라 코드는 이름을 모른다)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// ---------- 카운터 / 스턴 ----------
	// 붙어 있는 동안 카운터 스킬로 헤드를 치면 카운터 성공. 보스 패턴 몽타주의 "LOA Grant Tags" 구간으로 붙인다 (파란 빛 구간).
	// 보스 쪽 카운터 모듈(ULOASkillModule_Counter)이 직접 확인하므로 네이티브.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Counterable);
	// 스턴 중. ULOAGA_Stun이 시간 있는 GE로 붙인다 (모든 컴퓨터에 복제). StateTree가 "스턴 상태"로 가는 조건 (LOA Has Tag).
	// 이름 규칙 (2026-10-18): 걸리는 효과는 State.CrowdControl.<종류>.<이름>, 막는 면역은 State.Immune.<종류>.
	//   종류 이름은 로아 영어판 용어를 따른다 (2026-10-21 사용자 결정):
	//     피격 이상 = Push (띄움·다운 등)   경직 = Paralysis   상태 이상 = Debuff (기절 등)
	//   (옛 이름 HitReaction / StatusEffect → DefaultGameplayTags.ini 리다이렉트)
	//   예) State.CrowdControl.Debuff.Stunned  ↔  State.Immune.Debuff
	//   "상태 이상 중인가?"는 부모 State.CrowdControl.Debuff 하나로 물으면 된다 (자식도 "있음").
	//   보스 카운터 스턴은 면역에 막히지 않게 둔다 (ULOAGA_Stun 기본값). 플레이어 기절 BP는 Activation Blocked Tags에 State.Immune.Debuff.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Debuff_Stunned);

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

	// ---------- 플레이어가 받는 CC: 경직 (2026-10-21) ----------
	// 효과 State.CrowdControl.Paralysis ↔ 면역 State.Immune.Paralysis (슈퍼아머 스킬의 Activation Owned Tags), State.Immune.Push도 막는다
	// GE_CC_Paralysis(부모 ULOAGE_Paralysis)가 붙인다 (복제). 붙는 순간 ULOAGA_Paralysis가 발동.
	// 모든 플레이어 Ability의 발동 차단 태그 (ULOAPlayerAbility 생성자) → 경직 중엔 회피·스킬 전부 안 나감.
	// 피격 이상(Push)이 더 강하다: 띄움·누움 중엔 경직이 안 붙고, 경직 중 띄움을 맞으면 띄움이 경직을 끊는다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Paralysis);

	// ---------- Ability: 코드가 직접 이름을 아는 Ability 태그 ----------
	// ULOAGA_Airborne의 Asset Tag (생성자). 기상기가 Cancel Abilities with Tag로 띄움 GA를 끊을 때 쓴다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_CrowdControl_Airborne);
	// ULOAGA_Paralysis의 Asset Tag (생성자). 나중에 경직을 끊는 기능이 생기면 이 태그로
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_CrowdControl_Paralysis);
	// ---------- Skill.Attribute: 공격 속성 (이 공격이 어떤 성질인가) ----------
	// 플레이어 GA의 Asset Tags에 넣는다. ULOAGameplayAbility::MakeSkillModuleContext가 Skill.Attribute 아래 태그만 골라
	// 상자(FLOASkillModuleContext::AttackTags)에 담고, 맞은 쪽에 가는 Event.Combat.Hit에 실려 간다.
	// 공격 쪽은 "이 공격은 이런 속성"만 알린다. 그 속성으로 무슨 일이 생기는지는 맞은 쪽이 정한다. (2026-10-18)
	// 나중에 헤드어택·백어택도 여기에 (Skill.Attribute.HeadAttack 등)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attribute);			// 부모. 상자에 담을 태그를 고를 때 쓴다
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attribute_Counter);	// 카운터 스킬

	// ---------- Event: 코드가 직접 기다리는 몽타주 이벤트 ----------
	// "후딜 시작" 노티파이가 보낸다. ULOAPlayerAbility가 이 이름으로 기다리므로 네이티브로 둔다.
	// (타격 이벤트 Event.Montage.Hit는 BP에서 고르는 값이라 ini에 있다)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_Unlock);

	// "다음 타 입력 받기 시작" 노티파이가 보낸다. 콤보 GA(ULOAGA_Combo)가 이 이름으로 기다린다.
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_ComboOpen);

	// GA가 코드로 보내는 신호 (몽타주 노티파이가 아님 → 저장 시 노티파이 확인 대상이 아님)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Charge_Released);	// 차지 발사 (뗌 / 최대 시간)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Charge_Perfect);	// 차지 퍼펙트 존 안에서 발사

	// 체력이 "0보다 컸는데 0이 됨" (처음 한 번). ULOAAttributeSet_Common이 서버에서 자기 ASC에 보낸다.
	// GA_Death의 발동 조건(Trigger). 몽타주 노티파이가 아니므로 저장 시 노티파이 확인 대상이 아니다.
	// EventData: Instigator = 막타를 친 쪽 / Target = 죽은 몸 / EventMagnitude = 마지막 데미지
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);

	// ---------- Event.Combat: 전투 중 서버가 보내는 신호 (몽타주 노티파이가 아님 → 저장 시 노티파이 확인 대상이 아님) ----------
	// 맞았다. ULOACombatLibrary::ApplyPayloads가 서버에서, Payload를 다 적용한 뒤 맞은 쪽 ASC에 보낸다. 적이 적을 쳤을 때만 (힐·버프는 안 보냄)
	// 판정 방식(Server / Victim)·투사체 상관없이, 맞은 걸로 최종 결정된 대상에게 한 번씩.
	// EventData: Instigator = 시전자 / Target = 맞은 쪽 / InstigatorTags = 공격 속성 (Skill.Attribute.*)
	// 듣는 곳: 보스 카운터 모듈(ULOASkillModule_Counter). 나중에 무력화 등
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Hit);

	// 카운터 성공. 카운터 모듈이 판정에 성공한 순간 보스 자신의 ASC에 보낸다. (지금은 듣는 곳 없음. 나중에 "카운터!" 글씨·소리)
	// EventData: Instigator = 카운터 친 플레이어 / Target = 보스
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Counter);

	// 스턴 걸기. ULOAGA_Stun의 발동 조건(Trigger). EventMagnitude = 스턴 시간(초)
	// EventData: Instigator = 스턴을 건 쪽 (카운터면 카운터 친 플레이어)
	LOA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Stun);
}