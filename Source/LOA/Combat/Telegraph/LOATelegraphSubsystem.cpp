// LOATelegraphSubsystem.cpp

#include "Combat/Telegraph/LOATelegraphSubsystem.h"
#include "Combat/Telegraph/LOATelegraph.h"
#include "Combat/Telegraph/LOATelegraphStyle.h"
#include "Combat/Telegraph/LOATelegraphTypes.h"
#include "Combat/Telegraph/LOATelegraphSettings.h"
#include "Combat/LOACombatRPCComponent.h"
#include "Core/LOALog.h"
#include "Core/LOANetTime.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Algo/BinarySearch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOATelegraphSubsystem)

namespace LOATelegraphSubsystem
{
	// MPC_Telegraph의 칸 이름 (머티리얼과의 약속). 모든 예고가 같이 보는 값은 이것 하나뿐이다.
	static const FName Param_ServerTime(TEXT("ServerTime"));
}

ULOATelegraphSubsystem* ULOATelegraphSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<ULOATelegraphSubsystem>() : nullptr;
}

// =============================================================================
// 입구 (서버)
// =============================================================================

namespace LOATelegraphSubsystem
{
	/** [서버] 이 월드의 모든 플레이어 전투 RPC 컴포넌트에 Func를 부른다. @return 부른 수 */
	template <typename FuncType>
	int32 ForEachPlayerCombatRPC(UWorld* World, FuncType&& Func)
	{
		int32 NumSent = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			// PC 클래스를 직접 알 필요 없이 전투 RPC 컴포넌트만 찾는다.
			const APlayerController* PC = It->Get();
			if (ULOACombatRPCComponent* CombatRPC = PC ? PC->FindComponentByClass<ULOACombatRPCComponent>() : nullptr)
			{
				Func(*CombatRPC);
				++NumSent;
			}
		}
		return NumSent;
	}

	/** 서버만 보낸다. 클라에서 부르면 nullptr */
	UWorld* GetServerWorld(const UObject* WorldContextObject)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		return (World && World->GetNetMode() != NM_Client) ? World : nullptr;
	}
}

int32 ULOATelegraphSubsystem::ShowToAll(const UObject* WorldContextObject, FLOATelegraphData Data, float SecondsUntilHit)
{
	UWorld* World = LOATelegraphSubsystem::GetServerWorld(WorldContextObject);

	// 겉모습이 없거나 차오를 시간이 없으면(바로 판정) 예고 없음
	if (!World || !Data.Style || SecondsUntilHit <= 0.f)
	{
		return 0;
	}

	// ① 번호: 서버 안에서만 겹치지 않으면 된다. 1부터 하나씩 (0 = 번호 없음)
	static int32 LastTelegraphId = 0;
	LastTelegraphId = (LastTelegraphId == MAX_int32) ? 1 : LastTelegraphId + 1;
	Data.TelegraphId = LastTelegraphId;

	// ② 시각 (서버 시계). 예) 지금 100.0 + 1.2초 → 101.2에 꽉 참
	Data.StartServerTime = LOANetTime::GetServerTimeNow(World);
	Data.HitServerTime = Data.StartServerTime + SecondsUntilHit;

	// ③ 모든 플레이어 화면으로 (Reliable)
	const int32 NumSent = LOATelegraphSubsystem::ForEachPlayerCombatRPC(World,
		[&Data](ULOACombatRPCComponent& CombatRPC) { CombatRPC.ClientShowTelegraph(Data); });

	return NumSent > 0 ? Data.TelegraphId : 0;
}

void ULOATelegraphSubsystem::HideToAll(const UObject* WorldContextObject, int32 TelegraphId)
{
	UWorld* World = LOATelegraphSubsystem::GetServerWorld(WorldContextObject);
	if (!World || TelegraphId == 0)
	{
		return;
	}

	LOATelegraphSubsystem::ForEachPlayerCombatRPC(World,
		[TelegraphId](ULOACombatRPCComponent& CombatRPC) { CombatRPC.ClientHideTelegraph(TelegraphId); });
}

// =============================================================================
// 각 컴퓨터
// =============================================================================

bool ULOATelegraphSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULOATelegraphSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 데디케이티드 서버는 그릴 화면이 없다. 풀도 MPC도 필요 없다.
	if (!CanDraw())
	{
		return;
	}

	const ULOATelegraphSettings* Settings = GetDefault<ULOATelegraphSettings>();

	// ① 서버 시각을 넣을 MPC
	TimeCollection = Settings->TimeCollection.LoadSynchronous();

	// ② 맵 시작 때 풀 채우기 (패턴 중에 만드는 일이 없게)
	for (const TSoftObjectPtr<ULOATelegraphStyle>& SoftStyle : Settings->PrewarmStyles)
	{
		if (ULOATelegraphStyle* Style = SoftStyle.LoadSynchronous())
		{
			FillPool(Style);
		}
	}
}

void ULOATelegraphSubsystem::Deinitialize()
{
	// 맵이 내려가면 예고 액터도 월드와 함께 사라진다. 목록만 비운다.
	Active.Reset();
	Pools.Reset();
	TimeCollection = nullptr;

	Super::Deinitialize();
}

// =============================================================================
// 그리기 / 지우기
// =============================================================================

void ULOATelegraphSubsystem::ShowTelegraph(const FLOATelegraphData& Data)
{
	if (!CanDraw() || !Data.Style)
	{
		return;
	}

	// 받는 사이에 이미 판정 시각이 지났으면(아주 늦게 도착) 그리지 않는다.
	if (Data.HitServerTime <= GetServerTimeNow())
	{
		return;
	}

	// ① 그 Style의 풀에서 꺼내기. 처음 보는 Style이면 풀부터 채운다.
	FLOATelegraphPool* Pool = Pools.Find(Data.Style);
	if (!Pool)
	{
		FillPool(Data.Style);
		Pool = Pools.Find(Data.Style);
	}
	if (!Pool)
	{
		return;
	}

	ALOATelegraph* Telegraph = nullptr;
	while (!IsValid(Telegraph) && !Pool->Free.IsEmpty())
	{
		Telegraph = Pool->Free.Pop(EAllowShrinking::No);
	}

	// ② 모자라면 하나 더 만든다. Pool Size를 넘으면 한 번만 경고 (숫자를 늘리라는 신호)
	if (!IsValid(Telegraph))
	{
		Telegraph = CreateTelegraph(Data.Style);
		if (!Telegraph)
		{
			return;
		}
		Pool = Pools.Find(Data.Style);	// 만들면서 목록이 바뀌었을 수 있어 다시 찾는다
		if (Pool && !Pool->bWarnedGrow && Pool->NumCreated > Data.Style->GetPoolSize())
		{
			Pool->bWarnedGrow = true;
			LOA_LOG(this, Warning, TEXT("[Telegraph] %s pool grew past Pool Size %d (now %d). Raise Pool Size on the style."),
				*GetNameSafe(Data.Style), Data.Style->GetPoolSize(), Pool->NumCreated);
		}
	}

	// ③ 그리기
	Telegraph->ShowTelegraph(Data);

	// ④ 판정 시각이 빠른 순서로 줄 세우기 (Tick은 앞에서부터 지난 것만 본다)
	FLOAActiveTelegraph Entry;
	Entry.Telegraph = Telegraph;
	Entry.TelegraphId = Data.TelegraphId;
	Entry.HitServerTime = Data.HitServerTime;

	const int32 InsertIndex = Algo::UpperBoundBy(Active, Data.HitServerTime, &FLOAActiveTelegraph::HitServerTime);
	Active.Insert(Entry, InsertIndex);

	// 첫 프레임부터 맞게 차오르도록 시각을 바로 한 번 써 둔다.
	WriteServerTime();
}

void ULOATelegraphSubsystem::HideTelegraph(int32 TelegraphId)
{
	if (TelegraphId == 0)
	{
		return;
	}

	const int32 Index = Active.IndexOfByPredicate(
		[TelegraphId](const FLOAActiveTelegraph& Entry) { return Entry.TelegraphId == TelegraphId; });
	if (Index == INDEX_NONE)
	{
		return;	// 이미 판정 시각이 지나 돌아갔거나, 이 컴퓨터에서는 그리지 않았음
	}

	ALOATelegraph* Telegraph = Active[Index].Telegraph;
	Active.RemoveAt(Index);
	ReleaseToPool(Telegraph);
}

// =============================================================================
// 매 프레임
// =============================================================================

bool ULOATelegraphSubsystem::IsTickable() const
{
	// 그리는 중인 예고가 있을 때만
	return !Active.IsEmpty();
}

TStatId ULOATelegraphSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULOATelegraphSubsystem, STATGROUP_Tickables);
}

void ULOATelegraphSubsystem::Tick(float DeltaTime)
{
	// ① 모든 예고가 같이 보는 시계. 예고가 100개여도 여기 한 번
	WriteServerTime();

	// ② 판정 시각이 지난 예고를 풀로. 빠른 순으로 줄 서 있으므로 앞에서부터 지난 것까지만 본다.
	//    예) [101.2, 101.5, 103.0] 지금 101.3 → 앞 1개만 돌려놓고 멈춤
	const double Now = GetServerTimeNow();
	int32 NumExpired = 0;
	while (NumExpired < Active.Num() && Active[NumExpired].HitServerTime <= Now)
	{
		ReleaseToPool(Active[NumExpired].Telegraph);
		++NumExpired;
	}
	if (NumExpired > 0)
	{
		Active.RemoveAt(0, NumExpired, EAllowShrinking::No);
	}
}

// =============================================================================
// 도우미
// =============================================================================

bool ULOATelegraphSubsystem::CanDraw() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

double ULOATelegraphSubsystem::GetServerTimeNow() const
{
	// 클라는 엔진 추정값 + 핑 ÷ 2 (자세한 이유는 LOANetTime.h)
	return LOANetTime::GetServerTimeNow(GetWorld());
}

void ULOATelegraphSubsystem::WriteServerTime()
{
	if (!TimeCollection)
	{
		if (!bWarnedNoTimeCollection)
		{
			bWarnedNoTimeCollection = true;
			LOA_LOG(this, Warning, TEXT("[Telegraph] Time Collection is not set (Project Settings > Game > LOA Telegraph). Telegraphs will not fill."));
		}
		return;
	}

	UWorld* World = GetWorld();
	UMaterialParameterCollectionInstance* Instance = World ? World->GetParameterCollectionInstance(TimeCollection) : nullptr;
	if (Instance)
	{
		Instance->SetScalarParameterValue(LOATelegraphSubsystem::Param_ServerTime, static_cast<float>(GetServerTimeNow()));
	}
}

void ULOATelegraphSubsystem::FillPool(ULOATelegraphStyle* Style)
{
	if (!Style)
	{
		return;
	}

	Pools.FindOrAdd(Style);
	while (Pools[Style].NumCreated < Style->GetPoolSize())
	{
		ALOATelegraph* Telegraph = CreateTelegraph(Style);
		if (!Telegraph)
		{
			break;
		}
		Pools[Style].Free.Add(Telegraph);
	}
}

ALOATelegraph* ULOATelegraphSubsystem::CreateTelegraph(ULOATelegraphStyle* Style)
{
	UWorld* World = GetWorld();
	if (!World || !Style)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ALOATelegraph* Telegraph = World->SpawnActor<ALOATelegraph>(ALOATelegraph::StaticClass(), FTransform::Identity, SpawnParams);
	if (!Telegraph)
	{
		return nullptr;
	}

	Telegraph->SetupLayers(Style);

	// 만든 수만 센다. 쉬는 목록에 넣을지(FillPool), 바로 쓸지(ShowTelegraph)는 부르는 쪽이 정한다.
	++Pools.FindOrAdd(Style).NumCreated;
	return Telegraph;
}

void ULOATelegraphSubsystem::ReleaseToPool(ALOATelegraph* Telegraph)
{
	if (!IsValid(Telegraph))
	{
		return;
	}

	Telegraph->HideTelegraph();

	if (FLOATelegraphPool* Pool = Pools.Find(Telegraph->GetStyle()))
	{
		Pool->Free.Add(Telegraph);
	}
}
