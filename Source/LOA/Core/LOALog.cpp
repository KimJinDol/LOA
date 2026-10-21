// LOALog.cpp

#include "Core/LOALog.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogLOA);

namespace LOALog
{
	FString GetNetModeString(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;

		if (!World)
		{
			return TEXT("NoWorld");
		}

		switch (World->GetNetMode())
		{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_Client:
		{
			// PIE로 여러 창을 띄웠을 때 몇 번째 클라이언트인지 구분하기 위함
#if WITH_EDITOR
			return FString::Printf(TEXT("Client %d"), UE::GetPlayInEditorID());
#else
			return TEXT("Client");
#endif
		}
		default:                 return TEXT("Unknown");
		}
	}
}
