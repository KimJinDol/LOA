// LOAHitJudgeTypes.cpp

#include "Combat/LOAHitJudgeTypes.h"

#include "Engine/NetSerialization.h"
#include "UObject/CoreNet.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOAHitJudgeTypes)

bool FLOAHitJudgeRequestData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	// 있음/없음 비트: CC가 있는가, 공격한 보스가 있는가, 공격 속성이 있는가 (디버그 명령의 가짜 요청은 셋 다 없다)
	uint8 bHasCrowdControl = CrowdControlEffect ? 1 : 0;
	uint8 bHasAttacker = Attacker ? 1 : 0;
	uint8 bHasAttackTags = AttackTags.IsEmpty() ? 0 : 1;
	Ar.SerializeBits(&bHasCrowdControl, 1);
	Ar.SerializeBits(&bHasAttacker, 1);
	Ar.SerializeBits(&bHasAttackTags, 1);

	Ar << RequestId;

	// 모양: 종류(2비트) + 몸통 판정(1비트) + 그 종류의 칸만
	uint8 ShapeType = static_cast<uint8>(Shape.ShapeType);
	uint8 bHitByBody = Shape.bHitByBody ? 1 : 0;
	Ar.SerializeBits(&ShapeType, 2);
	Ar.SerializeBits(&bHitByBody, 1);
	if (Ar.IsLoading())
	{
		Shape.ShapeType = static_cast<ELOAHitShapeType>(ShapeType);
		Shape.bHitByBody = bHitByBody != 0;
	}
	switch (Shape.ShapeType)
	{
	case ELOAHitShapeType::Sphere:
		Ar << Shape.Radius;
		break;
	case ELOAHitShapeType::Fan:
		Ar << Shape.Radius;
		Ar << Shape.Angle;
		Ar << Shape.InnerRadius;
		break;
	case ELOAHitShapeType::Box:
		Ar << Shape.Length;
		Ar << Shape.Width;
		break;
	default:
		bOutSuccess = false;
		break;
	}
	Ar << Shape.HalfHeight;	// 높이 범위 (모든 모양. 2026-11-01)

	// 자리: FVector_NetQuantize10과 같은 방식 (0.1cm)
	FVector_NetQuantize10 PackedCenter(ShapeCenter);
	bool bCenterOk = true;
	PackedCenter.NetSerialize(Ar, Map, bCenterOk);
	bOutSuccess &= bCenterOk;
	if (Ar.IsLoading())
	{
		ShapeCenter = PackedCenter;
	}

	// 방향: 2바이트
	uint16 PackedYaw = FRotator::CompressAxisToShort(static_cast<double>(Yaw));
	Ar << PackedYaw;
	if (Ar.IsLoading())
	{
		Yaw = static_cast<float>(FRotator::DecompressAxisFromShort(PackedYaw));
	}

	// 판정 시각: double 그대로 (헤더 [직접 직렬화])
	Ar << HitServerTime;

	// CC GE: 있을 때만 (클래스 참조. 처음 한 번만 경로, 그 뒤는 번호)
	if (bHasCrowdControl)
	{
		UObject* EffectClassObject = CrowdControlEffect.Get();
		bOutSuccess &= Map && Map->SerializeObject(Ar, UClass::StaticClass(), EffectClassObject);
		if (Ar.IsLoading())
		{
			CrowdControlEffect = Cast<UClass>(EffectClassObject);
		}
	}
	else if (Ar.IsLoading())
	{
		CrowdControlEffect = nullptr;
	}

	// 공격한 보스: 있을 때만 (액터 참조. 처음 한 번만 경로, 그 뒤는 번호) (2026-11-04)
	if (bHasAttacker)
	{
		UObject* AttackerObject = Attacker.Get();
		bOutSuccess &= Map && Map->SerializeObject(Ar, AActor::StaticClass(), AttackerObject);
		if (Ar.IsLoading())
		{
			Attacker = Cast<AActor>(AttackerObject);
		}
	}
	else if (Ar.IsLoading())
	{
		Attacker = nullptr;
	}

	// 공격 속성: 있을 때만 (엔진 태그 묶음 직렬화) (2026-11-04)
	if (bHasAttackTags)
	{
		bool bTagsOk = true;
		AttackTags.NetSerialize(Ar, Map, bTagsOk);
		bOutSuccess &= bTagsOk;
	}
	else if (Ar.IsLoading())
	{
		AttackTags.Reset();
	}

	return true;
}
