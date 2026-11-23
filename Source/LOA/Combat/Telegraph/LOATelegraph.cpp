// LOATelegraph.cpp

#include "Combat/Telegraph/LOATelegraph.h"
#include "Combat/Telegraph/LOATelegraphStyle.h"
#include "Combat/Telegraph/LOATelegraphTypes.h"

#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOATelegraph)

namespace LOATelegraph
{
	// 머티리얼과의 약속 (칸 이름). LOATelegraphStyle.h [데칼 층과 머티리얼의 약속] 참고
	static const FName Param_ShapeType(TEXT("ShapeType"));
	static const FName Param_InnerRatio(TEXT("InnerRatio"));
	static const FName Param_Angle(TEXT("Angle"));
	static const FName Param_StartTime(TEXT("StartTime"));
	static const FName Param_HitTime(TEXT("HitTime"));

	// ShapeType 칸 값
	static constexpr float ShapeType_Radial = 0.f;	// 원, 부채꼴, 도넛
	static constexpr float ShapeType_Box = 1.f;		// 사각형
}

ALOATelegraph::ALOATelegraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 매 프레임 할 일이 없다. (차오름은 머티리얼이 한다)
	PrimaryActorTick.bCanEverTick = false;

	// 컴퓨터마다 자기 것. 복제하지 않는다.
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 꺼내기 전까지 숨겨 둔다.
	SetActorHiddenInGame(true);
}

void ALOATelegraph::SetupLayers(ULOATelegraphStyle* InStyle)
{
	Style = InStyle;
	if (!Style)
	{
		return;
	}

	// 데칼은 자기 X축으로 투영 → Pitch −90이면 X가 바닥을 향한다. 앞 방향은 루트(액터)의 Yaw가 정한다.
	const FRotator DecalRelativeRotation(-90.f, 0.f, 0.f);
	
	const TArray<TObjectPtr<UMaterialInterface>>& DecalLayers = Style->GetDecalLayers();
	for (int32 Index = 0; Index < DecalLayers.Num(); ++Index)
	{
		UMaterialInterface* LayerMaterial = DecalLayers[Index];
		if (!LayerMaterial)
		{
			continue;
		}

		// 머티리얼 사본은 여기서 한 번만 만든다. 꺼낼 때는 칸 값만 바꾼다.
		UMaterialInstanceDynamic* LayerMID = UMaterialInstanceDynamic::Create(LayerMaterial, this);

		UDecalComponent* Decal = NewObject<UDecalComponent>(this, NAME_None, RF_Transient);
		Decal->SetupAttachment(Root);
		Decal->SetDecalMaterial(LayerMID);
		Decal->SetSortOrder(Index);		// 뒤 칸일수록 위에
		Decal->SetFadeScreenSize(0.f);	// 멀리서 봐도 작아서 사라지지 않게 (기본값은 화면에서 작으면 숨김)
		Decal->SetRelativeRotation(DecalRelativeRotation);
		Decal->RegisterComponent();

		Decals.Add(Decal);
		LayerMaterials.Add(LayerMID);
	}

	HideTelegraph();
}

void ALOATelegraph::ShowTelegraph(const FLOATelegraphData& Data)
{
	if (!Style)
	{
		return;
	}

	// ① 모양 → 데칼 상자와 머티리얼 칸 값
	//    상자 반 크기: 앞뒤(HalfForward) × 좌우(HalfSide). 상자 중심은 기준점에서 앞으로 CenterForward
	//      Sphere : 중심 = 기준점,                 반 크기 = 반지름
	//      Fan    : 중심 = 꼭짓점,                 반 크기 = 길이. 머티리얼이 Angle·InnerRatio로 잘라 그린다
	//      Box    : 중심 = 뒷변 가운데 + 앞으로 길이/2, 반 크기 = 길이/2 × 폭/2
	//    예) 돌진 Box 800 × 200 → 상자를 앞으로 400 밀고, 반 크기 400 × 100
	//    (ForwardDistance는 부르는 쪽이 Location에 이미 반영했다. bHitByBody는 판정 전용이라 보지 않는다)
	const FLOAHitShape& Shape = Data.Shape;
	float HalfForward = 0.f;
	float HalfSide = 0.f;
	float CenterForward = 0.f;
	float ShapeTypeValue = LOATelegraph::ShapeType_Radial;
	float InnerRatio = 0.f;
	float AngleDegrees = 360.f;

	switch (Shape.ShapeType)
	{
	case ELOAHitShapeType::Sphere:
		HalfForward = HalfSide = Shape.Radius;
		break;

	case ELOAHitShapeType::Fan:
		HalfForward = HalfSide = Shape.Radius;
		InnerRatio = Shape.Radius > 0.f ? FMath::Clamp(Shape.InnerRadius / Shape.Radius, 0.f, 1.f) : 0.f;
		AngleDegrees = FMath::Clamp(Shape.Angle, 1.f, 360.f);
		break;

	case ELOAHitShapeType::Box:
		HalfForward = Shape.Length * 0.5f;
		HalfSide = Shape.Width * 0.5f;
		CenterForward = HalfForward;
		ShapeTypeValue = LOATelegraph::ShapeType_Box;
		break;

	default:
		// 새 모양을 Enum에만 추가하고 여기를 빼먹으면 알 수 있게
		ensureMsgf(false, TEXT("ALOATelegraph: unknown shape type %d"), static_cast<int32>(Shape.ShapeType));
		return;
	}

	// ② 자리와 방향. 루트 = 기준점, 루트의 Yaw = 앞 방향. (예고는 뜬 자리에 고정. 아무것도 따라가지 않는다)
	SetActorLocationAndRotation(Data.Location, FRotator(0.f, Data.Yaw, 0.f));

	// ③ 데칼 상자. 데칼은 바닥으로 눕혀 붙어 있어서(SetupLayers) 데칼의 X = 위아래, Y = 좌우, Z = 앞뒤
	//    DecalSize = (위아래 깊이, 좌우 반, 앞뒤 반). 상자 중심은 루트 기준 앞으로 CenterForward
	const FVector DecalSize(Style->GetProjectionDepth(), HalfSide, HalfForward);
	for (UDecalComponent* Decal : Decals)
	{
		if (Decal)
		{
			Decal->SetRelativeLocation(FVector(CenterForward, 0.f, 0.f));
			Decal->DecalSize = DecalSize;
			Decal->MarkRenderStateDirty();	// 크기만 바꾸면 화면에 반영되지 않으므로 다시 그리게 한다
		}
	}

	// ④ 머티리얼 칸 (꺼낼 때 한 번. 차오름은 머티리얼이 MPC_Telegraph의 ServerTime을 보고 스스로 한다)
	//    머티리얼 칸은 float라 시각을 float로 줄여 넣는다. 서버를 24시간 켜 둬도(86400초) 오차는 약 0.008초.
	for (UMaterialInstanceDynamic* LayerMID : LayerMaterials)
	{
		if (!LayerMID)
		{
			continue;
		}
		LayerMID->SetScalarParameterValue(LOATelegraph::Param_ShapeType, ShapeTypeValue);
		LayerMID->SetScalarParameterValue(LOATelegraph::Param_InnerRatio, InnerRatio);
		LayerMID->SetScalarParameterValue(LOATelegraph::Param_Angle, AngleDegrees);
		LayerMID->SetScalarParameterValue(LOATelegraph::Param_StartTime, static_cast<float>(Data.StartServerTime));
		LayerMID->SetScalarParameterValue(LOATelegraph::Param_HitTime, static_cast<float>(Data.FillServerTime));	// 머티리얼의 HitTime = 꽉 차는 시각 (판정은 같거나 뒤)
	}

	SetTelegraphVisible(true);
}

void ALOATelegraph::HideTelegraph()
{
	SetTelegraphVisible(false);
}

void ALOATelegraph::SetTelegraphVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	for (UDecalComponent* Decal : Decals)
	{
		if (Decal)
		{
			Decal->SetVisibility(bVisible);
		}
	}
}
