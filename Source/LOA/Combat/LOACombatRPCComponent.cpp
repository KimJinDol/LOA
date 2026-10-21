// LOACombatRPCComponent.cpp

#include "Combat/LOACombatRPCComponent.h"
#include "UI/LOAHUD.h"
#include "Combat/Telegraph/LOATelegraphSubsystem.h"
#include "Combat/LOAHitJudgeSubsystem.h"

#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOACombatRPCComponent)

ULOACombatRPCComponent::ULOACombatRPCComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 받아서 넘기기만 하므로 매 프레임 할 일이 없다.
	PrimaryComponentTick.bCanEverTick = false;

	// RPC를 쓰려면 복제가 켜져 있어야 한다. (끄면 RPC가 조용히 안 감)
	// 복제할 변수는 없으므로 RPC 통로로만 쓴다.
	SetIsReplicatedByDefault(true);
}

// =============================================================================
// 서버 → 이 플레이어 컴퓨터
// =============================================================================

void ULOACombatRPCComponent::ClientShowDamageNumber_Implementation(AActor* Target, const FLOADamageNumberInfo& Info)
{
	// 숫자를 띄우는 일은 HUD 담당. 여기는 네트워크로 받아서 넘기기만 한다.
	const APlayerController* PC = GetOwner<APlayerController>();
	if (ALOAHUD* LOAHUD = PC ? PC->GetHUD<ALOAHUD>() : nullptr)
	{
		LOAHUD->ShowDamageNumber(Target, Info);
	}
}

void ULOACombatRPCComponent::ClientShowTelegraph_Implementation(const FLOATelegraphData& Data)
{
	// 그리는 일은 이 컴퓨터의 예고 매니저 담당. 여기는 네트워크로 받아서 넘기기만 한다.
	if (ULOATelegraphSubsystem* Telegraphs = ULOATelegraphSubsystem::Get(this))
	{
		Telegraphs->ShowTelegraph(Data);
	}
}

void ULOACombatRPCComponent::ClientHideTelegraph_Implementation(int32 TelegraphId)
{
	if (ULOATelegraphSubsystem* Telegraphs = ULOATelegraphSubsystem::Get(this))
	{
		Telegraphs->HideTelegraph(TelegraphId);
	}
}

void ULOACombatRPCComponent::ClientRequestHitJudge_Implementation(
	int32 RequestId, const FLOAHitShape& Shape, FVector_NetQuantize10 ShapeCenter, float Yaw, double HitServerTime, bool bJudgeOnArrival)
{
	// 판정은 이 컴퓨터의 판정 서브시스템 담당. 여기는 네트워크로 받아서 넘기기만 한다.
	if (ULOAHitJudgeSubsystem* HitJudge = ULOAHitJudgeSubsystem::Get(this))
	{
		HitJudge->JudgeLocal(RequestId, Shape, ShapeCenter, Yaw, HitServerTime, bJudgeOnArrival);
	}
}

// =============================================================================
// 이 플레이어 컴퓨터 → 서버
// =============================================================================

void ULOACombatRPCComponent::ServerReportHitJudgeHit_Implementation(int32 RequestId)
{
	ForwardHitJudgeReport(RequestId, true, FVector::ZeroVector);
}

void ULOACombatRPCComponent::ServerReportHitJudgeMiss_Implementation(int32 RequestId, FVector_NetQuantize ReportedLocation)
{
	ForwardHitJudgeReport(RequestId, false, ReportedLocation);
}

void ULOACombatRPCComponent::ForwardHitJudgeReport(int32 RequestId, bool bHit, const FVector& ReportedLocation) const
{
	// 맞은 사람 = 이 RPC를 보낸 플레이어의 캐릭터 (인자로 받지 않는다 → 남을 대신 보고할 수 없다)
	const APlayerController* PC = GetOwner<APlayerController>();
	APawn* Victim = PC ? PC->GetPawn() : nullptr;
	ULOAHitJudgeSubsystem* HitJudge = ULOAHitJudgeSubsystem::Get(this);
	if (!Victim || !HitJudge)
	{
		return;
	}

	// 받아 줄지(기다리는 중인가, 검증)는 판정 서브시스템이 정한다.
	HitJudge->ReceiveReport(Victim, RequestId, bHit, ReportedLocation);
}
