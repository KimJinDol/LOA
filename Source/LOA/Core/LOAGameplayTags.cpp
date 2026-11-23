// LOAGameplayTags.cpp

#include "Core/LOAGameplayTags.h"

namespace LOAGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "캐릭터 이동 입력 (캐릭터가 직접 처리)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look, "InputTag.Look", "카메라 시점 입력 (캐릭터가 직접 처리)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_DamageRate, "SetByCaller.DamageRate", "데미지 GE에 코드가 넣는 스킬 배율 (공격력의 몇 배)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_ManaCost, "SetByCaller.ManaCost", "마나 소모 GE에 코드가 넣는 마나 변화량 (음수)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Cooldown, "SetByCaller.Cooldown", "쿨타임 GE에 코드가 넣는 쿨타임 시간 (초)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Duration, "SetByCaller.Duration", "태그 부여 GE에 코드가 넣는 유지 시간 (초)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_ChargeMultiplier, "SetByCaller.ChargeMultiplier", "모든 Payload GE에 코드가 넣는 차지 배율 (차지 스킬이 아니면 1)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_LaunchHeight, "SetByCaller.LaunchHeight", "(2026-10-21부터 안 씀: 띄움 높이는 GE의 Launch Height로 고정) 예전 Payload에 남아 있어도 무시된다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_DownDuration, "SetByCaller.DownDuration", "띄움 GE: 착지한 뒤 누워 있는 시간 초 (Payload 값. 없으면 GE의 Default Down Duration)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_LaunchDistance, "SetByCaller.LaunchDistance", "띄움·경직이 날아가는/밀리는 수평 거리 cm (Payload 값. 없으면 GE의 Default Distance)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_HealRate, "SetByCaller.HealRate", "회복 GE: 힐러 공격력의 몇 배를 회복하나 (ULOAHealExecution)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_ShieldRate, "SetByCaller.ShieldRate", "보호막 GE: 힐러 공격력의 몇 배를 보호막으로 주나 (ULOAShieldMagnitude)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_IdentityGain, "SetByCaller.IdentityGain", "아이덴티티 게이지 변화량 (+ 스킬 수급, − 켜진 동안 감소·끌 때 환급). 코드가 넣음");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_ShieldUsed, "SetByCaller.ShieldUsed", "보호막 GE 하나가 이미 깎인 양. 맞을 때 코드가 고친다 (스킬 데이터에 넣지 않음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_HealthChange, "SetByCaller.HealthChange", "체력 바꾸기 GE(ULOAGE_HealthChange)에 넣는 변화량. 음수 = 깎기. 대난투 누적 피해 (ALOABrawlArena)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stagger, "SetByCaller.Stagger", "무력화 GE(ULOAGE_Stagger)에 스킬 데이터가 넣는 무력화 수치");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Block_Move, "State.Block.Move", "이 태그가 붙어 있는 동안 클릭 이동 불가 (스킬 시전 중, 기절 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Block_Ability, "State.Block.Ability", "이 태그가 붙어 있는 동안 다른 플레이어 Ability 발동 불가 (스킬 후딜 시작 전)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Body_Hidden, "State.Body.Hidden", "몸이 사라짐: 안 보이고, 맞지 않고, 몸 막이 꺼짐 (보스가 하늘로 사라진 구간. LOA Grant Tags로 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Brawl_Active, "State.Brawl.Active", "대난투 진행 중 (보스). 받는 피해 × Brawl Damage Multiplier, 체력 대신 대난투 입구로 (ULOADamageExecution). ALOABrawlArena가 붙이고 뗌");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Body_Groggy, "State.Body.Groggy", "그로기: 무력화를 다 깎여 대난투가 도는 동안 보스에 붙음 (ULOAGA_BossBrawl). 보스 StateTree 대난투 상태가 본다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Body_PassThroughBodyBlock, "State.Body.PassThroughBodyBlock", "이 태그가 붙어 있는 동안 보스·몬스터의 몸 막이를 뚫고 지나감 (관통 스킬. Move 모듈이 이동 중에 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immune_Push, "State.Immune.Push", "피격 이상 면역 (Push Immunity). 띄움/다운 등 + 경직까지 받지 않음 (데미지는 받음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immune_Status, "State.Immune.Status", "상태 이상 면역 (Status Immunity). 기절/빙결/공포 등을 받지 않음 (데미지는 받음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Body_Dead, "State.Body.Dead", "죽어 있음. 모든 Ability 발동 불가, 판정 대상에서 제외, 몸 충돌 꺼짐 (GA_Death가 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Body_Invulnerable, "State.Body.Invulnerable", "무적. 적의 공격이 아무것도 주지 않음 (아군 힐·버프는 받음). 부활 직후 3초");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Buff, "State.Buff", "버프의 부모. 이 아래 태그를 주는 GE는 죽을 때 지워짐 (ULOAGA_Death)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Debuff, "State.Debuff", "디버프의 부모. 이 아래 태그를 주는 GE는 죽을 때 지워짐 (ULOAGA_Death)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Buff_Shield, "State.Buff.Shield", "보호막이 걸려 있음 (GE_Shield가 붙임). 맞을 때 깎을 보호막을 이 태그로 찾음");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Buff_AttackPower, "State.Buff.AttackPower", "공격력 버프 중 (GE_AttackBuff가 붙임). 표시용");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Buff_DamageDealt, "State.Buff.DamageDealt", "주는 피해 증가 중 (아이덴티티 오라 GE가 붙임). 표시용");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Buff_MoveSpeed, "State.Buff.MoveSpeed", "이동속도 증가 중 (GE가 MoveSpeedBonus를 올림). 표시용");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Debuff_Mark, "State.Debuff.Mark", "낙인. 받는 피해 증가 중 (GE가 IncomingDamageBonus를 올림). 표시용");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Debuff_CritResistance, "State.Debuff.CritResistance", "치명타 저항 감소 중, 로아 약점 노출 (GE가 CritResistance를 내림). 표시용");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Debuff_WeakPoint, "State.Debuff.WeakPoint", "약점 공격: 대난투 진행 중 보스에 붙음 (ALOABrawlArena). 백·헤드 어택 스킬이 방향과 상관없이 보너스를 받음");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Identity_Active, "State.Identity.Active", "아이덴티티가 켜져 있음. 게이지가 줄고 스킬로 차지 않음 (ULOAGA_Identity의 감소 GE가 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Window_Counterable, "State.Window.Counterable", "카운터 가능 구간 (파란 빛). 카운터 스킬로 헤드를 치면 카운터 성공 (패턴 몽타주의 LOA Grant Tags로 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Window_JustGuardable, "State.Window.JustGuardable", "저스트 가드 구간 (노란 빛). 연출용. 보스 BP 이벤트 On Just Guard Window Changed (패턴 몽타주의 LOA Just Guard Window가 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Status, "State.CrowdControl.Status", "상태 이상의 부모. 플레이어 Ability가 이 태그로 막힘");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Status_Stunned, "State.CrowdControl.Status.Stunned", "스턴 중. 아무 행동도 못 함 (스턴 GE ULOAGE_Stun이 붙임, ULOAGA_Stun이 켜짐)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Push, "State.CrowdControl.Push", "피격 이상 중 (띄움, 다운 등의 부모). 플레이어 Ability 발동 불가 (기상기만 예외)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Push_Airborne, "State.CrowdControl.Push.Airborne", "띄움 중 (공중 ~ 완전히 일어날 때까지). GE_CC_Airborne이 붙이고 ULOAGA_Airborne이 발동하고 지운다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Push_Down, "State.CrowdControl.Push.Down", "누워 있음 (띄움 후 착지 ~ 완전히 일어날 때까지). 다시 뜨지 않음, 회피 막힘, 기상기 발동 조건");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Push_GettingUp, "State.CrowdControl.Push.GettingUp", "일어나는 중 (기상 몽타주 구간). 서버가 붙이고, 맞은 사람 컴퓨터가 보고 GetUp 구간으로 넘어감");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immune_Paralysis, "State.Immune.Paralysis", "경직 면역 (Paralysis Immunity, 슈퍼아머). 경직을 받지 않음. 띄움 등 피격 이상은 막지 못함 (데미지는 받음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Paralysis, "State.CrowdControl.Paralysis", "경직 중. 아무것도 못 함 (스킬·회피·이동). GE_CC_Paralysis가 붙이고 ULOAGA_Paralysis가 발동하고 지운다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Common_Paralysis, "Ability.Common.Paralysis", "경직 GA(ULOAGA_Paralysis)의 Asset Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Common_Airborne, "Ability.Common.Airborne", "띄움 GA(ULOAGA_Airborne)의 Asset Tag. 기상기가 이 태그로 띄움을 끊는다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Common_Dodge, "Ability.Common.Dodge", "회피 GA(ULOAGA_Dodge)의 Asset Tag. 저스트 가드 중에는 이 태그의 Ability가 막힌다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Common_Death, "Ability.Common.Death", "사망 GA(ULOAGA_Death)의 Asset Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Common_Stun, "Ability.Common.Stun", "스턴 GA(ULOAGA_Stun)의 Asset Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Trait_KeepOnCC, "Ability.Trait.KeepOnCC", "CC(띄움·경직·스턴)를 맞아도 취소되지 않는 Ability (아이덴티티 등). 사망은 취소");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attribute,"Skill.Attribute", "공격 속성의 부모. 이 아래 태그가 공격과 함께 맞은 쪽에 전달된다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attribute_Counter, "Skill.Attribute.Counter", "공격 속성: 카운터. 플레이어 GA의 Asset Tags에 넣으면 카운터 스킬");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attribute_BackAttack, "Skill.Attribute.BackAttack", "공격 속성: 백 어택. 맞은 쪽 뒤(±반각)에서 치면 백 어택 피해 보너스");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attribute_HeadAttack, "Skill.Attribute.HeadAttack", "공격 속성: 헤드 어택. 맞은 쪽 앞(±반각)에서 치면 헤드 어택 피해 보너스");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attribute_JustGuard, "Skill.Attribute.JustGuard", "공격 속성: 저스트 가드로 막을 수 있음 (맞는 순간 저가 구간이 열려 있을 때). 보스 패턴 GA의 Asset Tags에 넣는다");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_Unlock, "Event.Montage.Unlock", "몽타주 후딜 시작. 이동/다른 Ability 잠금이 풀린다 (노티파이는 반드시 Branching Point)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_ComboOpen, "Event.Montage.ComboOpen", "콤보 다음 타 입력을 받기 시작하는 지점. 콤보 GA가 기다린다 (노티파이는 반드시 Branching Point)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_IdentityGain, "Event.Montage.IdentityGain", "아이덴티티 게이지 수급 시점. 사용형 스킬 몽타주에 노티파이로 찍는다 (스킬 1번에 1번만 참)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Skill_Charge_Started, "Event.Skill.Charge.Started", "차지 GA가 차지를 시작할 때 보내는 신호. 조종하는 사람 컴퓨터만 (OptionalObject = 그 차지 GA). 차지 게이지 UI가 받는다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Skill_Charge_Released, "Event.Skill.Charge.Released", "차지 GA가 발사 순간 보내는 신호 (EventMagnitude = 누른 시간, OptionalObject = 그 차지 GA)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Skill_Charge_Cancelled, "Event.Skill.Charge.Cancelled", "차지 중 떼기 전에 끊김 (회피·CC 등). 조종하는 사람 컴퓨터만 (OptionalObject = 그 차지 GA). 차지 게이지 UI가 받는다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Skill_Charge_Perfect, "Event.Skill.Charge.Perfect", "차지 GA가 퍼펙트 존 안에서 발사했을 때 추가로 보내는 신호");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Trigger_Death, "Event.Trigger.Death", "체력이 처음 0이 됨. Common 스탯창이 보내고 GA_Death가 받는다 (EventMagnitude = 마지막 데미지)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Trigger_StaggerBroken, "Event.Trigger.StaggerBroken", "무력화 게이지가 처음 0이 됨. 보스 스탯창이 보스 자신에게 보낸다 (Instigator = 마지막으로 깎은 쪽)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_JustGuard_PatternResolved, "Event.JustGuard.PatternResolved", "저가 패턴 하나가 끝남. 보스 Just Guard 모듈이 보스 자신에게 보낸다 (EventMagnitude = 그 패턴의 저가 타격을 전부 막은 사람 수)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Hit, "Event.Combat.Hit", "맞았다. 서버가 Payload 적용 뒤 맞은 적에게 보낸다 (InstigatorTags = 공격 속성)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Counter, "Event.Combat.Counter", "카운터 성공. 보스 카운터 모듈이 판정에 성공하면 보스 자신에게 보낸다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Location_Raid_Respawn, "Location.Raid.Respawn", "정비소 자리 = 전멸 뒤 부활 자리 (위치 표시에 붙임). 보스 전투 시작 거리보다 멀리, 정비소 출구 상자 밖에");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Location_Raid_Entrance, "Location.Raid.Entrance", "전투 구역 입구. 정비소 출구로 나가면 여기로 (표시의 오른쪽 방향으로 벌려 세움)");
}