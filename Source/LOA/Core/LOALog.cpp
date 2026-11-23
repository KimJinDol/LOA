// LOALog.cpp

#include "Core/LOALog.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

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
		case NM_Standalone:
		{
			// PIE 여러 창이 모두 스탠드얼론인 로비(파티 찾기)에서 몇 번째 창인지 구분 (2026-11-19 apply131)
			// 창 번호는 월드 패키지에 붙은 PIE 번호로 (창마다 고정). UE::GetPlayInEditorID()는 "지금 처리 중인 창"이라
			// 창 처리 밖(MCP Python 등)에서 찍으면 -1이 된다 (엔진 NetDriver.cpp도 같은 방법으로 창 번호를 얻는다)
#if WITH_EDITOR
			if (World->WorldType == EWorldType::PIE)
			{
				return FString::Printf(TEXT("Standalone %d"), World->GetPackage()->GetPIEInstanceID());
			}
#endif
			return TEXT("Standalone");
		}
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
