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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Block_Move, "State.Block.Move", "이 태그가 붙어 있는 동안 클릭 이동 불가 (스킬 시전 중, 기절 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Block_Ability, "State.Block.Ability", "이 태그가 붙어 있는 동안 다른 플레이어 Ability 발동 불가 (스킬 후딜 시작 전)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Hidden, "State.Hidden", "몸이 사라짐: 안 보이고, 맞지 않고, 몸 막이 꺼짐 (보스가 하늘로 사라진 구간. LOA Grant Tags로 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PassThroughBodyBlock, "State.PassThroughBodyBlock", "이 태그가 붙어 있는 동안 보스·몬스터의 몸 막이를 뚫고 지나감 (관통 스킬. Move 모듈이 이동 중에 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immune_Push, "State.Immune.Push", "피격 이상 면역 (Push Immunity). 띄움/다운 등 + 경직까지 받지 않음 (데미지는 받음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immune_Debuff, "State.Immune.Debuff", "상태 이상 면역 (Debuff Immunity). 기절/빙결/공포 등을 받지 않음 (데미지는 받음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "죽어 있음. 모든 Ability 발동 불가, 판정 대상에서 제외, 몸 충돌 꺼짐 (GA_Death가 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Counterable, "State.Counterable", "카운터 가능 구간 (파란 빛). 카운터 스킬로 헤드를 치면 카운터 성공 (패턴 몽타주의 LOA Grant Tags로 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Debuff_Stunned, "State.CrowdControl.Debuff.Stunned", "스턴 중. 아무 행동도 못 함 (ULOAGA_Stun이 붙임)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Push, "State.CrowdControl.Push", "피격 이상 중 (띄움, 다운 등의 부모). 플레이어 Ability 발동 불가 (기상기만 예외)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Push_Airborne, "State.CrowdControl.Push.Airborne", "띄움 중 (공중 ~ 완전히 일어날 때까지). GE_CC_Airborne이 붙이고 ULOAGA_Airborne이 발동하고 지운다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Push_Down, "State.CrowdControl.Push.Down", "누워 있음 (띄움 후 착지 ~ 완전히 일어날 때까지). 다시 뜨지 않음, 회피 막힘, 기상기 발동 조건");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immune_Paralysis, "State.Immune.Paralysis", "경직 면역 (Paralysis Immunity, 슈퍼아머). 경직을 받지 않음. 띄움 등 피격 이상은 막지 못함 (데미지는 받음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Paralysis, "State.CrowdControl.Paralysis", "경직 중. 아무것도 못 함 (스킬·회피·이동). GE_CC_Paralysis가 붙이고 ULOAGA_Paralysis가 발동하고 지운다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_CrowdControl_Paralysis, "Ability.CrowdControl.Paralysis", "경직 GA(ULOAGA_Paralysis)의 Asset Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_CrowdControl_Airborne, "Ability.CrowdControl.Airborne", "띄움 GA(ULOAGA_Airborne)의 Asset Tag. 기상기가 이 태그로 띄움을 끊는다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attribute,"Skill.Attribute", "공격 속성의 부모. 이 아래 태그가 공격과 함께 맞은 쪽에 전달된다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attribute_Counter, "Skill.Attribute.Counter", "공격 속성: 카운터. 플레이어 GA의 Asset Tags에 넣으면 카운터 스킬");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_Unlock, "Event.Montage.Unlock", "몽타주 후딜 시작. 이동/다른 Ability 잠금이 풀린다 (노티파이는 반드시 Branching Point)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_ComboOpen, "Event.Montage.ComboOpen", "콤보 다음 타 입력을 받기 시작하는 지점. 콤보 GA가 기다린다 (노티파이는 반드시 Branching Point)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Skill_Charge_Released, "Event.Skill.Charge.Released", "차지 GA가 발사 순간 보내는 신호 (EventMagnitude = 누른 시간)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Skill_Charge_Perfect, "Event.Skill.Charge.Perfect", "차지 GA가 퍼펙트 존 안에서 발사했을 때 추가로 보내는 신호");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "체력이 처음 0이 됨. Common 스탯창이 보내고 GA_Death가 받는다 (EventMagnitude = 마지막 데미지)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Hit, "Event.Combat.Hit", "맞았다. 서버가 Payload 적용 뒤 맞은 적에게 보낸다 (InstigatorTags = 공격 속성)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Counter, "Event.Combat.Counter", "카운터 성공. 보스 카운터 모듈이 판정에 성공하면 보스 자신에게 보낸다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Stun, "Event.Stun", "스턴 걸기. ULOAGA_Stun이 받는다 (EventMagnitude = 스턴 시간, 초)");
}