// LOADebugCommands.cpp
// [디버그] 개발 중에만 쓰는 콘솔 명령 모음. 출시 빌드(Shipping)에는 들어가지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [LOA.Boss.Pattern]  보스 패턴을 바로 발동한다. (StateTree, 패턴 테스터 창이 생기기 전 임시 도구)
//
//   ` 키로 콘솔 열기 →
//     LOA.Boss.Pattern                 지금 보스가 가진 패턴 목록을 Output Log에 출력
//     LOA.Boss.Pattern Slam            Ability.Grux.Slam 발동 (태그 마지막 부분만 써도 됨, 대소문자 무시)
//     LOA.Boss.Pattern Ability.Grux.Slam   전체 태그로 써도 됨
//   노릴 대상 = 명령을 친 사람의 캐릭터 (Target Usage가 None인 패턴은 무시)
//
// [LOA.Boss.AI]  보스 AI(StateTree)를 멈추거나 다시 켠다. 패턴을 콘솔로 시험할 때 AI가 끼어들지 않게.
//
//     LOA.Boss.AI Pause                멈춤 (하던 패턴도 취소, 그 자리에 섬)
//     LOA.Boss.AI Resume               다시 켬 (대기부터 다시 시작. 죽은 보스는 안 켜짐)
//     LOA.Boss.AI                      사용법 출력
//   천천히 보기는 엔진 기본 명령 slomo (예: slomo 0.25 → 4배 느리게, slomo 1 → 원래대로)
//
// [LOA.Bench.HitJudge]  판정 방식 두 가지의 서버 비용 비교 (최적화 측정용)
//
//     LOA.Bench.HitJudge [반복 수] [반지름]     기본 1000번, 반지름 500
//   내 캐릭터 위치에 원을 놓고(공격하는 쪽 = 나 → 허수아비·보스가 적), 같은 순간·같은 모양으로 두 방식을 각각 반복 수만큼 돌려 평균 시간을 잰다.
//     Overlap : ULOACombatLibrary::FindTargetsInSphere (물리 엔진 겹침 검사 → ASC·살아 있음·Filter 확인) ← 지금 서버 판정
//     Math    : 후보 목록(레벨에서 ASC가 있는 액터 전부, 재기 전에 한 번 모음)을 돌며 IsTargetable·PassesFilter·IsBodyInShape (물리 엔진을 안 씀)
//   두 방식이 찾은 수도 같이 찍는다. 다르면 비교가 공정하지 않다는 뜻 (예: 높이 차이가 큰 대상).
//   예) [Bench] 100 candidates, radius 500, 1000 runs | Overlap 4.12 us (hit 3) | Math 6.10 us (hit 3)   ← 숫자는 예시
//   허수아비를 레벨에 복사해(Alt+드래그) 10 / 50 / 100개로 늘려 가며 잰다. 스탠드얼론으로 충분.
//   Unreal Insights: Trace(cpu 채널) 켜고 명령을 치면 타임라인에 LOA_Bench_Overlap / LOA_Bench_Math 구간이 보인다.
//
// [LOA.Bench.HitJudgeRPC]  피해자 판정의 "누구에게 물어보나" 세기 (RPC는 보내지 않음)
//
//     LOA.Bench.HitJudgeRPC [반지름] [여유 거리]     기본 반지름 500, 여유 600
//   허수아비를 "가짜 원격 플레이어"로 보고, 내 캐릭터 위치의 원 판정 한 번에 판정 요청을 몇 명에게 보낼지 센다.
//     Now : 지금 규칙. 살아 있는 적이면 거리와 상관없이 전원
//     A   : 모양 + 여유 거리 안만 (여유 = 마감 1초 동안 움직일 수 있는 거리 정도). 밖은 서버가 바로 "안 맞음"
//   예) [BenchRPC] 98 fake players, radius 500, margin 600 | in shape 3 | Now: ask 98 | A: ask 9   ← 숫자는 예시
//   요청 1개 + 보고 1개 = 2 RPC / 명. 실제 바이트는 리슨 + 클라로 Network Insights(net 채널)에서 잰다.
//
// [LOA.Bench.RPCStress]  판정 요청 RPC를 한 번에 많이 보내 비용을 잰다 (Unreal Insights용 부하)
//
//     LOA.Bench.RPCStress [개수]     기본 100 (1~1000)
//   리슨 호스트 창에서 친다. 접속한 클라 1명에게 가짜 판정 요청(ClientRequestHitJudge)을 개수만큼 한 번에 보낸다.
//     = "100명에게 묻는 판정 한 번"과 같은 양을 클라 1명으로 흉내
//   번호는 -1, -2, ... (진짜 판정 번호는 1부터) → 클라가 판정해 보고해도 서버는 "기다리는 번호 없음"으로 버린다. 게임에 영향 없음.
//   Insights: 서버 타임라인의 LOA_Bench_RPCSend 구간 = 보내는 데 쓴 CPU, Networking Insights = RPC 한 개 크기·패킷 수.
//   주의: 지금 RPC는 Reliable. 한 번에 너무 많이(약 250개 넘게) 보내면 대기열이 넘쳐 클라 연결이 끊길 수 있다.
//
// [네트워크]
//   패턴 발동과 AI 멈춤은 서버에서만 할 수 있다. 이 명령은 명령을 친 컴퓨터에서 실행되므로
//     스탠드얼론, 리슨 서버 호스트 창 → 동작
//     클라이언트 창 (데디케이티드 포함) → 경고만 (서버로 보내는 길이 없음)
//   클라이언트에서도 쓰려면 서버로 요청을 보내는 패턴 테스터 창이 필요하다. (패턴 3개를 만든 뒤 반드시 만든다)
//
// [왜 콘솔 명령인가]
//   프로토타입 단계라 가장 빨리 만들 수 있는 방법. 엔진 기능(FAutoConsoleCommandWithWorldAndArgs)이라
//   PlayerController나 HUD를 고치지 않아도 된다. 패턴 테스터 창이 생기면 이 파일은 지워도 된다.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/Abilities/Monster/LOAMonsterAbility.h"
#include "Character/Monster/Boss/LOABossCharacter.h"
#include "AI/LOAMonsterAIController.h"
#include "Game/LOARaidSubsystem.h"
#include "Core/LOALog.h"
#include "Library/LOACombatLibrary.h"
#include "Combat/LOACombatRPCComponent.h"
#include "Core/LOANetTime.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "HAL/PlatformTime.h"
#include "EngineUtils.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace LOADebugCommands
{
	/** 사용자가 친 이름(Slam / ability.grux.slam)을 보스가 가진 패턴 태그 중 하나로 바꾼다. 못 찾으면 빈 태그 */
	static FGameplayTag FindPatternTag(const FGameplayTagContainer& PatternTags, const FString& Input)
	{
		for (const FGameplayTag& Tag : PatternTags)
		{
			const FString TagName = Tag.ToString();

			// 전체 태그가 같거나, 마지막 부분이 같으면 (Ability.Grux.Slam ↔ Slam)
			if (TagName.Equals(Input, ESearchCase::IgnoreCase) ||
				TagName.EndsWith(TEXT(".") + Input, ESearchCase::IgnoreCase))
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	static void BossPattern(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		// 패턴은 서버만 발동할 수 있다. (명령을 친 컴퓨터 = 이 World)
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Boss.Pattern] Works only on the server (standalone or listen server host window)."));
			return;
		}

		ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(World);
		ALOABossCharacter* Boss = Raid ? Raid->GetCurrentBoss() : nullptr;
		UAbilitySystemComponent* ASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Boss.Pattern] No boss in this level."));
			return;
		}

		FGameplayTagContainer PatternTags;
		ULOAMonsterAbility::GetPatternTags(ASC, PatternTags);

		// 이름 없이 치면 목록만
		if (Args.IsEmpty())
		{
			UE_LOG(LogLOA, Display, TEXT("[LOA.Boss.Pattern] %s patterns: %s"),
				*GetNameSafe(Boss), PatternTags.IsEmpty() ? TEXT("(none)") : *PatternTags.ToStringSimple());
			return;
		}

		const FGameplayTag PatternTag = FindPatternTag(PatternTags, Args[0]);
		if (!PatternTag.IsValid())
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Boss.Pattern] '%s' not found. Available: %s"),
				*Args[0], PatternTags.IsEmpty() ? TEXT("(none)") : *PatternTags.ToStringSimple());
			return;
		}

		// 노릴 대상 = 명령을 친 사람의 캐릭터 (이 World의 첫 번째 = 내 PlayerController)
		const APlayerController* PC = World->GetFirstPlayerController();
		AActor* Target = PC ? PC->GetPawn() : nullptr;

		ULOAMonsterAbility::TryActivatePattern(ASC, PatternTag, Target);
	}

	static void BossAI(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		// AI는 서버에만 있다.
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Boss.AI] Works only on the server (standalone or listen server host window)."));
			return;
		}

		const bool bPause = !Args.IsEmpty() && Args[0].Equals(TEXT("Pause"), ESearchCase::IgnoreCase);
		const bool bResume = !Args.IsEmpty() && Args[0].Equals(TEXT("Resume"), ESearchCase::IgnoreCase);
		if (!bPause && !bResume)
		{
			UE_LOG(LogLOA, Display, TEXT("[LOA.Boss.AI] Usage: LOA.Boss.AI Pause | Resume"));
			return;
		}

		ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(World);
		ALOABossCharacter* Boss = Raid ? Raid->GetCurrentBoss() : nullptr;
		ALOAMonsterAIController* BossController = Boss ? Cast<ALOAMonsterAIController>(Boss->GetController()) : nullptr;
		if (!BossController)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Boss.AI] No boss with a LOAMonsterAIController in this level."));
			return;
		}

		BossController->SetBrainPaused(bPause);
		UE_LOG(LogLOA, Display, TEXT("[LOA.Boss.AI] %s %s."), *GetNameSafe(Boss), bPause ? TEXT("paused") : TEXT("resumed"));
	}

	/** 판정 두 방식 비교. 결과는 Output Log 한 줄 */
	static void BenchHitJudge(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Bench.HitJudge] Works only on the server (standalone or listen server host window)."));
			return;
		}

		const int32 Runs = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 1000;
		const float Radius = Args.Num() > 1 ? FMath::Max(1.f, FCString::Atof(*Args[1])) : 500.f;

		// 공격하는 쪽 = 내 캐릭터 → 허수아비·보스가 "적" (플레이어가 아닌 쪽)
		const APlayerController* PC = World->GetFirstPlayerController();
		AActor* Attacker = PC ? PC->GetPawn() : nullptr;
		if (!Attacker)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Bench.HitJudge] No player character to measure from."));
			return;
		}

		const FVector Center = Attacker->GetActorLocation();
		const float Yaw = Attacker->GetActorRotation().Yaw;
		FLOAHitShape Shape;
		Shape.ShapeType = ELOAHitShapeType::Sphere;
		Shape.Radius = Radius;
		Shape.bHitByBody = true;

		// Math가 돌 후보 목록 = 레벨에서 ASC가 있는 액터 전부 (플레이어, 보스, 허수아비). 시간 재기 전에 한 번 모은다.
		// 허수아비는 Pawn이 아니라 Actor라서 Pawn만 모으면 빠진다. (Overlap은 Pawn 채널 "충돌체"를 찾으므로 허수아비 캡슐도 찾음)
		// (실제로 만들면 플레이어 목록처럼 미리 있는 목록을 쓰는 셈이라, 모으는 시간은 재지 않는다)
		struct FBenchCandidate
		{
			const AActor* Actor = nullptr;
			float BodyRadius = 0.f;
		};
		TArray<FBenchCandidate> Candidates;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(*It))
			{
				const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(It->GetRootComponent());
				Candidates.Add({ *It, Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f });
			}
		}

		// ① Overlap: 지금 서버 판정과 같은 함수
		TArray<AActor*> OverlapTargets;
		int32 OverlapHits = 0;
		const double OverlapStart = FPlatformTime::Seconds();
		{
			// Unreal Insights 타임라인에 이 이름의 구간으로 보인다
			TRACE_CPUPROFILER_EVENT_SCOPE(LOA_Bench_Overlap);
			for (int32 Run = 0; Run < Runs; ++Run)
			{
				OverlapTargets.Reset();
				OverlapHits = ULOACombatLibrary::FindTargetsInSphere(
					Attacker, Center, Radius, ELOATargetFilter::Enemy, ECC_Pawn, OverlapTargets, false, Attacker, true);
			}
		}
		const double OverlapUs = (FPlatformTime::Seconds() - OverlapStart) * 1e6 / Runs;

		// ② Math: 후보 목록 + 계산만 (물리 엔진을 안 씀)
		int32 MathHits = 0;
		const double MathStart = FPlatformTime::Seconds();
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LOA_Bench_Math);
			for (int32 Run = 0; Run < Runs; ++Run)
			{
				MathHits = 0;
				for (const FBenchCandidate& Candidate : Candidates)
				{
					if (!ULOACombatLibrary::IsTargetable(Candidate.Actor)
						|| !ULOACombatLibrary::PassesFilter(Attacker, Candidate.Actor, ELOATargetFilter::Enemy))
					{
						continue;
					}

					if (ULOACombatLibrary::IsBodyInShape(Shape, Center, Yaw, Candidate.Actor->GetActorLocation(), Candidate.BodyRadius))
					{
						++MathHits;
					}
				}
			}
		}
		const double MathUs = (FPlatformTime::Seconds() - MathStart) * 1e6 / Runs;

		UE_LOG(LogLOA, Display, TEXT("[Bench] %d candidates, radius %.0f, %d runs | Overlap %.2f us (hit %d) | Math %.2f us (hit %d)%s"),
			Candidates.Num(), Radius, Runs, OverlapUs, OverlapHits, MathUs, MathHits,
			OverlapHits != MathHits ? TEXT(" | hit counts differ (height or non-character targets?)") : TEXT(""));
	}

	/** 피해자 판정 RPC 대상 수 세기 (지금 규칙 vs 근처만). 실제 RPC는 보내지 않는다. */
	static void BenchHitJudgeRPC(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Bench.HitJudgeRPC] Works only on the server (standalone or listen server host window)."));
			return;
		}

		const float Radius = Args.Num() > 0 ? FMath::Max(1.f, FCString::Atof(*Args[0])) : 500.f;
		const float Margin = Args.Num() > 1 ? FMath::Max(0.f, FCString::Atof(*Args[1])) : 600.f;

		const APlayerController* PC = World->GetFirstPlayerController();
		const AActor* Attacker = PC ? PC->GetPawn() : nullptr;
		if (!Attacker)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Bench.HitJudgeRPC] No player character to measure from."));
			return;
		}

		const FVector Center = Attacker->GetActorLocation();
		const float Yaw = Attacker->GetActorRotation().Yaw;

		FLOAHitShape Shape;
		Shape.ShapeType = ELOAHitShapeType::Sphere;
		Shape.Radius = Radius;
		Shape.bHitByBody = true;

		FLOAHitShape ShapeWithMargin = Shape;	// A: 모양을 여유 거리만큼 키운 것
		ShapeWithMargin.Radius = Radius + Margin;

		int32 NumFakePlayers = 0;	// 살아 있는 적 = 지금 규칙이면 전원에게 묻는다
		int32 NumInShape = 0;		// 서버가 보기에 실제로 맞는 수
		int32 NumAskA = 0;			// A 규칙이면 묻는 수
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Candidate = *It;
			if (Candidate == Attacker || !UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate)
				|| !ULOACombatLibrary::IsTargetable(Candidate)
				|| !ULOACombatLibrary::PassesFilter(Attacker, Candidate, ELOATargetFilter::Enemy))
			{
				continue;
			}

			const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Candidate->GetRootComponent());
			const float BodyRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
			const FVector Location = Candidate->GetActorLocation();

			++NumFakePlayers;
			NumInShape += ULOACombatLibrary::IsBodyInShape(Shape, Center, Yaw, Location, BodyRadius) ? 1 : 0;
			NumAskA += ULOACombatLibrary::IsBodyInShape(ShapeWithMargin, Center, Yaw, Location, BodyRadius) ? 1 : 0;
		}

		UE_LOG(LogLOA, Display, TEXT("[BenchRPC] %d fake players, radius %.0f, margin %.0f | in shape %d | Now: ask %d (%d RPC) | A: ask %d (%d RPC)"),
			NumFakePlayers, Radius, Margin, NumInShape, NumFakePlayers, NumFakePlayers * 2, NumAskA, NumAskA * 2);
	}

	/** 접속한 클라 1명에게 가짜 판정 요청을 N개 보낸다. 번호가 음수라 서버는 보고를 버린다. */
	static void BenchRPCStress(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() != NM_ListenServer)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Bench.RPCStress] Works only on the listen server host window with a client connected."));
			return;
		}

		const int32 Count = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 1000) : 100;

		// 받을 클라 = 이 컴퓨터가 아닌 첫 플레이어
		ULOACombatRPCComponent* TargetRPC = nullptr;
		const APlayerController* TargetPC = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Candidate = It->Get();
			if (Candidate && !Candidate->IsLocalController())
			{
				TargetRPC = Candidate->FindComponentByClass<ULOACombatRPCComponent>();
				TargetPC = Candidate;
				break;
			}
		}
		if (!TargetRPC)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Bench.RPCStress] No connected client with a combat RPC component."));
			return;
		}
		if (Count > 200)
		{
			UE_LOG(LogLOA, Warning, TEXT("[LOA.Bench.RPCStress] %d reliable RPCs at once may overflow the reliable buffer and disconnect the client."), Count);
		}

		// 내용은 실제 장판 판정과 같은 크기 (원, 클라 캐릭터 위치)
		const APawn* TargetPawn = TargetPC->GetPawn();
		const FVector Center = TargetPawn ? TargetPawn->GetActorLocation() : FVector::ZeroVector;
		FLOAHitShape Shape;
		Shape.ShapeType = ELOAHitShapeType::Sphere;
		Shape.Radius = 500.f;
		Shape.bHitByBody = true;
		const double Now = LOANetTime::GetServerTimeNow(World);

		const double StartSeconds = FPlatformTime::Seconds();
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LOA_Bench_RPCSend);
			for (int32 Index = 1; Index <= Count; ++Index)
			{
				// bJudgeOnArrival = true → 클라가 기다리지 않고 바로 판정해 보고
				TargetRPC->ClientRequestHitJudge(-Index, Shape, Center, 0.f, Now, true);
			}
		}
		const double SendUs = (FPlatformTime::Seconds() - StartSeconds) * 1e6;

		UE_LOG(LogLOA, Display, TEXT("[BenchRPCStress] sent %d fake judge requests to %s | send %.1f us total, %.2f us each (reports will be ignored)"),
			Count, *GetNameSafe(TargetPC), SendUs, SendUs / Count);
	}

	static FAutoConsoleCommandWithWorldAndArgs BossAICommand(
		TEXT("LOA.Boss.AI"),
		TEXT("Pause or resume the boss AI (StateTree). Example: LOA.Boss.AI Pause"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BossAI));

	static FAutoConsoleCommandWithWorldAndArgs BossPatternCommand(
		TEXT("LOA.Boss.Pattern"),
		TEXT("Activate a boss pattern targeting your character. No argument = list patterns. Example: LOA.Boss.Pattern Slam"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BossPattern));

	static FAutoConsoleCommandWithWorldAndArgs BenchHitJudgeCommand(
		TEXT("LOA.Bench.HitJudge"),
		TEXT("Compare hit judge cost: physics overlap vs math over players. Usage: LOA.Bench.HitJudge [runs=1000] [radius=500]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BenchHitJudge));

	static FAutoConsoleCommandWithWorldAndArgs BenchHitJudgeRPCCommand(
		TEXT("LOA.Bench.HitJudgeRPC"),
		TEXT("Count victim-judge requests: ask everyone (now) vs ask only within shape + margin. No RPC is sent. Usage: LOA.Bench.HitJudgeRPC [radius=500] [margin=600]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BenchHitJudgeRPC));

	static FAutoConsoleCommandWithWorldAndArgs BenchRPCStressCommand(
		TEXT("LOA.Bench.RPCStress"),
		TEXT("Send N fake victim-judge requests to one connected client to measure RPC cost in Insights. Reports are ignored. Usage: LOA.Bench.RPCStress [count=100]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BenchRPCStress));
}

#endif // !UE_BUILD_SHIPPING
