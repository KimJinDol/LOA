// LOACollisionChannels.h
// 프로젝트 전용 충돌 종류(채널) 이름. 실제 정의는 Config/DefaultEngine.ini의 [/Script/Engine.CollisionProfile].
// 엔진은 사용자 채널을 ECC_GameTraceChannel1~18로 부른다. 숫자 대신 이 이름으로 쓴다.
//
// [BodyBlock] 몸 막이 (ULOABodyBlockerComponent의 충돌 종류)
//   목적: 보스는 플레이어를 뚫고 지나가고, 플레이어는 보스에 막힌다. (로아 규칙)
//   언리얼 충돌은 대칭(둘 중 약한 쪽을 따름)이라 캡슐 하나로는 안 된다 → 막기 전용 충돌체를 따로 둔다.
//     보스 캡슐(이동용)   : Pawn 무시 → 보스가 움직일 때 플레이어에 막히지 않음
//     몸 막이 캡슐        : 종류 BodyBlock, Pawn만 막음
//     플레이어 캡슐       : Pawn 무시(플레이어끼리 통과), BodyBlock 막음
//   캐릭터 이동은 자기 "루트 캡슐"로만 부딪힘을 계산한다. → 보스 이동은 몸 막이를 안 보고, 플레이어 이동만 몸 막이를 본다.
//   ini 기본 응답은 Ignore: 이 채널을 막겠다고 직접 켠 충돌체(플레이어 캡슐)만 막힌다.
//
// [RaidWall / RaidEdge] 레이드 구역 벽 (2026-11-01, 사용자 결정)
//   클릭 이동은 내비메시로 이미 막힌다. 내비메시를 안 따르는 이동(회피, CC 궤적, Move 모듈)을 막는 벽.
//                  걷기·회피·Move   CC 밀림·띄움        보스
//     RaidWall     막힘              막힘                 막힘      ← 일반 벽 (낙사 불가)
//     RaidEdge     막힘              통과 → 낙사          막힘      ← 낙사 가능 벽 (가장자리)
//   만드는 법: 레벨에 Blocking Volume → Collision Presets = RaidWall 또는 RaidEdge (ini 프로필. 코드 없음)
//   막는 쪽: 플레이어 캡슐(ALOACharacter), 보스 캡슐(ALOABossCharacter)이 두 채널을 Block
//   CC 통과: ULOACharacterMovementComponent가 CC 궤적이 걸려 있는 동안만 내 캡슐의 RaidEdge 응답을 Ignore로 바꾼다
//   나머지(투사체, 커서 Visibility 트레이스, 땅에 붙이기 Ground 트레이스, 판정 검색)는 기본 Ignore라 안 걸린다
//
// [PlayerWall] 플레이어만 막는 벽 (2026-11-19 apply130, 사용자: 대난투에서 보스 뒤로 못 넘어가게. 보스를 뚫는 스킬로도)
//   RaidWall은 보스 캡슐도 막는다 → 보스와 겹치게 놓으면 엔진이 보스를 밀어낸다
//   PlayerWall은 기본 응답 Ignore + 플레이어 캡슐(ALOACharacter)만 Block → 보스와 겹쳐도 보스는 모른다
//                  걷기·회피·Move   CC 밀림·띄움   보스 몸 통과(BodyBlock 무시)   보스
//     PlayerWall   막힘              막힘           막힘                            통과
//   예) 대난투 경기장: 보스 중앙선부터 뒤로 경기장 폭 전체를 상자 하나로 → 플레이어는 어떤 이동으로도 보스 뒤로 못 감
//   만드는 법: Box 컴포넌트·Blocking Volume → Collision Presets = PlayerWall (ini 프로필. 코드 없음)
//
// [Ground] 바닥 (2026-11-14, 트레이스 채널)
//   목적: 예고 장판·판정 모양을 바닥에 붙일 때(ULOACombatLibrary::FindGroundLocation) "바닥"만 맞게.
//   전에는 WorldStatic이면 다 맞아서 생긴 문제:
//     ① 장애물 프랍(WorldStatic) 위에서 생긴 장판이 프랍 윗면에 붙음
//     ② BP에 붙인 스태틱 메시(기본 BlockAllDynamic = WorldDynamic)는 바닥인데 안 맞음 → 장판이 공중에 남아 흐릿·엉뚱한 자리 (11-14 지형 파괴 더미 바닥판)
//   기본 응답 Ignore → 바닥이라고 표시한 것만 맞는다. 표시하는 법: 바닥 메시의 Collision Presets = Floor (ini 프로필. 코드 없음)
//     Floor = BlockAll과 같이 다 막고(충돌 종류 WorldStatic) + Ground도 막음
//   캐릭터·보스 캡슐, 프랍, 벽은 기본 Ignore라 안 걸린다. BSP 바닥은 프로필을 못 줘서 안 걸린다 (그 위의 장판은 처음 높이 그대로)

#pragma once

#include "Engine/EngineTypes.h"

namespace LOACollision
{
	/** 몸 막이. DefaultEngine.ini: Channel=ECC_GameTraceChannel1, Name="BodyBlock", DefaultResponse=ECR_Ignore */
	constexpr ECollisionChannel BodyBlock = ECC_GameTraceChannel1;

	/** 레이드 일반 벽 (낙사 불가). DefaultEngine.ini: Channel=ECC_GameTraceChannel2, Name="RaidWall", 프로필 RaidWall */
	constexpr ECollisionChannel RaidWall = ECC_GameTraceChannel2;

	/** 레이드 가장자리 벽 (CC로만 통과 = 낙사 가능). DefaultEngine.ini: Channel=ECC_GameTraceChannel3, Name="RaidEdge", 프로필 RaidEdge */
	constexpr ECollisionChannel RaidEdge = ECC_GameTraceChannel3;

	/** 바닥 (트레이스 채널. 땅에 붙이기 전용). DefaultEngine.ini: Channel=ECC_GameTraceChannel4, Name="Ground", bTraceType=True, 프로필 Floor */
	constexpr ECollisionChannel Ground = ECC_GameTraceChannel4;

	/** 플레이어만 막는 벽 (보스는 통과). DefaultEngine.ini: Channel=ECC_GameTraceChannel5, Name="PlayerWall", 프로필 PlayerWall (apply130) */
	constexpr ECollisionChannel PlayerWall = ECC_GameTraceChannel5;
}
