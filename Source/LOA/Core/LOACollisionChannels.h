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

#pragma once

#include "Engine/EngineTypes.h"

namespace LOACollision
{
	/** 몸 막이. DefaultEngine.ini: Channel=ECC_GameTraceChannel1, Name="BodyBlock", DefaultResponse=ECR_Ignore */
	constexpr ECollisionChannel BodyBlock = ECC_GameTraceChannel1;
}
