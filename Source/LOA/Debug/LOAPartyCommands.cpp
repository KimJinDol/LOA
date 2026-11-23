// LOAPartyCommands.cpp
// [디버그] 파티 찾기 콘솔 명령. 모집 창(apply132) 전에 Beacon·세션을 시험한다. 출시 빌드(Shipping)에는 없다. (2026-11-19 apply131)
//
// ─────────────────────────────────────────────────────────────────────────────
// [사용법] 로비(L_Lobby)에서 ` 키로 콘솔 →
//   LOA.Party Name 지혜리          이름 정하기 (파티 만들기·신청 전에 꼭)
//   LOA.Party Create 2수 34파폭    방 만들기 (뒤 글자 전부가 한 줄 글)
//   LOA.Party Find                 목록 검색 (LAN, 1~2초) → 로그 "Found N party(s)"
//   LOA.Party List                 마지막 검색 목록 출력 (번호 = Apply에 쓰는 번호)
//   LOA.Party Apply 0              목록 0번 방에 신청
//   LOA.Party Accept 1 / Reject 1 / Kick 1     파티장: 번호(MemberId)로 수락 / 거절 / 강퇴
//   LOA.Party Leave                나가기 (파티장이면 파티 닫기)
//   LOA.Party Enter                파티장: 입장하기
//   LOA.Party Show                 내 역할 + 파티 정보 (파티원 번호 포함)
//
// [PIE로 시험] Play → Number of Players 3, Net Mode = Play Standalone, 시작 맵 L_Lobby
//   창마다 콘솔이 따로다. 명령은 친 창의 컴퓨터에서 실행된다 (로그 앞 [Standalone N]으로 창 구분)
//   명령은 바로 실행하지 않고 그 창 월드의 다음 프레임에 실행한다. 창 처리 밖(MCP Python 등)에서 쳐도
//   버튼을 누를 때처럼 그 창의 프레임 처리 안에서 돌게 하려는 것 (밖에서 바로 부르면 파티장 → 신청자 RPC가 안 갔다, 11-20)
//
// [네트워크] 각자. 명령은 ULOAPartySubsystem의 입구를 부를 뿐이다 (UI 버튼과 같은 길)

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Party/LOAPartySubsystem.h"
#include "Player/LOALocalPlayer.h"
#include "Core/LOALog.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

namespace LOAPartyCommands
{
	static void PrintUsage()
	{
		UE_LOG(LogLOA, Display, TEXT("[PartyCmd] Usage: LOA.Party Name <name> | Create <title> | Find | List | Apply <index> | Accept <id> | Reject <id> | Kick <id> | Leave | Enter | Show"));
	}

	static void PrintListings(const TArray<FLOAPartyListEntry>& Listings)
	{
		UE_LOG(LogLOA, Display, TEXT("[PartyCmd] %d party(s)"), Listings.Num());
		for (int32 Index = 0; Index < Listings.Num(); ++Index)
		{
			const FLOAPartyListEntry& Entry = Listings[Index];
			UE_LOG(LogLOA, Display, TEXT("[PartyCmd]   %d : [%s] '%s' by %s  %d/%d  (%s)%s"),
				Index, *Entry.RaidId.ToString(), *Entry.Title, *Entry.LeaderName, Entry.NumMembers, Entry.MaxMembers, *Entry.BeaconAddress,
				Entry.bMyParty ? TEXT("  <- mine") : TEXT(""));
		}
	}

	static void PrintMembers(const TCHAR* Label, const TArray<FLOAPartyMember>& Members)
	{
		for (const FLOAPartyMember& Member : Members)
		{
			UE_LOG(LogLOA, Display, TEXT("[PartyCmd]   %s #%d %s%s (%s)"),
				Label, Member.MemberId, *Member.PlayerName, Member.bLeader ? TEXT(" [Leader]") : TEXT(""), *Member.ClassId.ToString());
		}
	}

	static void PrintParty(const ULOAPartySubsystem& Party)
	{
		const FLOAPartyInfo& Info = Party.GetPartyInfo();
		UE_LOG(LogLOA, Display, TEXT("[PartyCmd] Role %s | [%s] '%s' %d/%d | entering raid: %s"),
			*UEnum::GetValueAsString(Party.GetRole()), *Info.RaidId.ToString(), *Info.Title, Info.Members.Num(), Info.MaxMembers,
			Party.IsEnteringRaid() ? TEXT("yes") : TEXT("no"));
		PrintMembers(TEXT("Member   "), Info.Members);
		PrintMembers(TEXT("Applicant"), Info.Applicants);
	}

	static int32 ParseId(const TArray<FString>& Args)
	{
		return Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : INDEX_NONE;
	}

	/** 실제 실행. 다음 프레임에 그 창 월드의 타이머가 부른다 (인자는 복사해 둔 것) */
	static void RunPartyCommand(TArray<FString> Args, TWeakObjectPtr<UWorld> WeakWorld)
	{
		UWorld* World = WeakWorld.Get();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		ULOAPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<ULOAPartySubsystem>() : nullptr;
		if (!Party || Args.Num() == 0)
		{
			PrintUsage();
			return;
		}

		const FString& Verb = Args[0];

		if (Verb.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
		{
			ULOALocalPlayer* LocalPlayer = Cast<ULOALocalPlayer>(GameInstance->GetFirstGamePlayer());
			if (LocalPlayer && Args.Num() >= 2)
			{
				LocalPlayer->SetPlayerName(Args[1]);
			}
			return;
		}

		if (Verb.Equals(TEXT("Create"), ESearchCase::IgnoreCase))
		{
			// "Create 2수 34파폭" → 뒤 글자 전부를 한 줄 글로
			TArray<FString> TitleWords = Args;
			TitleWords.RemoveAt(0);
			Party->CreateParty(FString::Join(TitleWords, TEXT(" ")));
			return;
		}

		if (Verb.Equals(TEXT("Find"), ESearchCase::IgnoreCase))
		{
			// 끝나면 서브시스템이 "Found N party(s)"를 찍는다 → List로 본다
			Party->FindParties();
			return;
		}

		if (Verb.Equals(TEXT("List"), ESearchCase::IgnoreCase))
		{
			PrintListings(Party->GetListings());
			return;
		}

		if (Verb.Equals(TEXT("Apply"), ESearchCase::IgnoreCase))
		{
			Party->ApplyToParty(ParseId(Args));
			return;
		}

		if (Verb.Equals(TEXT("Accept"), ESearchCase::IgnoreCase))
		{
			Party->AcceptApplicant(ParseId(Args));
			return;
		}

		if (Verb.Equals(TEXT("Reject"), ESearchCase::IgnoreCase))
		{
			Party->RejectApplicant(ParseId(Args));
			return;
		}

		if (Verb.Equals(TEXT("Kick"), ESearchCase::IgnoreCase))
		{
			Party->KickMember(ParseId(Args));
			return;
		}

		if (Verb.Equals(TEXT("Leave"), ESearchCase::IgnoreCase))
		{
			Party->LeaveParty();
			return;
		}

		if (Verb.Equals(TEXT("Enter"), ESearchCase::IgnoreCase))
		{
			Party->EnterRaid();
			return;
		}

		if (Verb.Equals(TEXT("Show"), ESearchCase::IgnoreCase))
		{
			PrintParty(*Party);
			return;
		}

		PrintUsage();
	}

	/** 콘솔 입구: 그 창 월드의 다음 프레임으로 미룬다 (창이 사라지면 월드와 함께 타이머도 없어진다) */
	static void PartyCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			PrintUsage();
			return;
		}

		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateStatic(&RunPartyCommand, Args, TWeakObjectPtr<UWorld>(World)));
	}

	static FAutoConsoleCommandWithWorldAndArgs PartyCommandRegistration(
		TEXT("LOA.Party"),
		TEXT("Party finder test commands. Usage: LOA.Party Name <name> | Create <title> | Find | List | Apply <index> | Accept <id> | Reject <id> | Kick <id> | Leave | Enter | Show"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartyCommand));
}

#endif // !UE_BUILD_SHIPPING
