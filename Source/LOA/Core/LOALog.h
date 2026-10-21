// LOALog.h
// 프로젝트 전용 로그 카테고리와, 멀티플레이 디버깅용 로그 매크로.
//
// [왜 필요한가]
// 멀티플레이에서는 같은 코드가 서버와 여러 클라이언트에서 동시에 실행된다.
// 그냥 UE_LOG를 찍으면 "이 로그가 누구 컴퓨터에서 찍힌 건지" 알 수 없다.
// LOA_LOG는 로그 앞에 [DedicatedServer] / [ListenServer] / [Client] 를 자동으로 붙여준다.
//
// [사용 예]
//   LOA_LOG(this, Log, TEXT("HP changed: %f"), NewHP);
//   → 출력: LogLOA: [ListenServer] BP_LOAHero_C_0 : HP changed: 80.0

#pragma once

#include "CoreMinimal.h"

LOA_API DECLARE_LOG_CATEGORY_EXTERN(LogLOA, Log, All);

namespace LOALog
{
	/** WorldContext가 속한 월드의 네트워크 역할을 문자열로 반환 */
	LOA_API FString GetNetModeString(const UObject* WorldContextObject);
}

/**
 * 네트워크 역할이 앞에 붙는 로그.
 * @param WorldContext  로그를 찍는 객체 (보통 this)
 * @param Verbosity     Log / Warning / Error 등
 */
#define LOA_LOG(WorldContext, Verbosity, Format, ...) \
	UE_LOG(LogLOA, Verbosity, TEXT("[%s] %s : %s"), \
		*LOALog::GetNetModeString(WorldContext), \
		*GetNameSafe(WorldContext), \
		*FString::Printf(Format, ##__VA_ARGS__))
