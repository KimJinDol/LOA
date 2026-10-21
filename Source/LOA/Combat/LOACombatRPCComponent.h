// LOACombatRPCComponent.h
// [배관] 전투 RPC 컴포넌트. 서버와 "이 플레이어 컴퓨터" 사이의 전투 RPC를 한곳에 모은 통로.
// (예전 이름: ULOACombatFeedbackComponent. 피해자 판정 보고가 들어오면서 "서버 → 화면"만이 아니게 되어 이름을 바꿈)
//
// ─────────────────────────────────────────────────────────────────────────────
// [규칙] 통로만 한다.
//   받아서 주인에게 넘기기만 한다. 확인, 계산, 상태(변수·타이머)는 넣지 않는다. 판단은 주인 클래스가 한다.
//   어떤 기능이 자기 상태를 갖게 되면 그 기능만 따로 컴포넌트로 뺀다. (예: 클릭 이동은 경로·Tick이 있어서 따로)
//
// [하는 일]
//   서버 → 이 플레이어 컴퓨터 (Client RPC)
//     - 내가 준 데미지 숫자          ClientShowDamageNumber   → HUD
//     - 보스 패턴 예고 장판          ClientShowTelegraph / ClientHideTelegraph → ULOATelegraphSubsystem
//     - 피해자 판정 요청             ClientRequestHitJudge    → ULOAHitJudgeSubsystem
//   이 플레이어 컴퓨터 → 서버 (Server RPC)
//     - 피해자 판정 보고             ServerReportHitJudgeHit / ServerReportHitJudgeMiss → ULOAHitJudgeSubsystem
//   나중: 받은 데미지, "면역" 같은 상태 글자, 카운터 성공, 무력화 표시 …
//
// [하지 않는 일]
//   - 전투와 무관한 RPC (알림, 채팅 등) → 생기면 그때 따로 만든다. (이 컴포넌트가 잡동사니 통이 되지 않게)
//   - 화면 그리기, 판정, 보고 확인 (HUD, 예고 서브시스템, 판정 서브시스템 담당)
//
// ─────────────────────────────────────────────────────────────────────────────
// [왜 PlayerController에 붙나]
// RPC는 "그 플레이어 연결이 소유한 액터"에서만 오갈 수 있다. (서버가 특정 플레이어 한 명에게 보내는 통로, 그 플레이어가 서버에 보내는 통로)
// HUD와 서브시스템은 RPC를 받을 수 없다. (액터가 아님) → PC에 붙은 이 컴포넌트가 오가며 넘긴다.
// Pawn이 아니라 PC인 이유: 사망/부활로 Pawn이 바뀌어도 PC는 그대로라 그 사이 도트 숫자도 유실되지 않는다.
//
// [왜 PC에 직접 RPC를 두지 않나]
// 전투 RPC가 늘어나면 PC가 비대해진다. 여기에 모아 PC는 입력/HUD 연결만 담당한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   - 컴포넌트에 RPC를 쓰려면 컴포넌트가 복제되도록 켜야 한다. (생성자의 SetIsReplicatedByDefault)
//     안 켜면 RPC가 오류 없이 조용히 안 간다.
//   - 이 컴포넌트를 부르는 쪽은 PC 클래스를 몰라도 된다. PC에서 FindComponentByClass로 찾는다.
//     (데미지 숫자 = ULOACombatLibrary, 예고 = ULOATelegraphSubsystem, 피해자 판정 = ULOAHitJudgeSubsystem)
//   - 데미지 숫자 = Unreliable: 연출용이라 유실돼도 실제 결과(체력 등)에는 영향 없음.
//   - 예고 = Reliable: 예고를 보고 피하는 게임이다. 즉사기 예고가 빠지면 억울한 죽음이 생긴다.
//   - 판정 요청·보고 = Reliable: 빠지면 마감 뒤 서버 판정으로 처리되지만, 피한 사람이 억울하게 맞지 않게 반드시 보낸다.
//     "누가 맞았나"는 인자로 받지 않는다. 보낸 연결의 캐릭터가 곧 맞은 사람 → 남을 대신 보고할 수 없다.
//
// [흐름 - 데미지 숫자]
//   서버: AttributeSet(데미지 확정) → ULOACombatLibrary::NotifyDamageDealt → ClientShowDamageNumber
//   본인 컴퓨터: ClientShowDamageNumber → ALOAHUD::ShowDamageNumber → 데미지 숫자 레이어
//
// [흐름 - 예고]
//   서버: ALOAAreaActor → ULOATelegraphSubsystem::ShowToAll → 플레이어마다 ClientShowTelegraph
//   각 컴퓨터: ClientShowTelegraph → ULOATelegraphSubsystem::ShowTelegraph → 풀에서 꺼내 그림
//   예) 플레이어 3명 → 서버가 3번 보냄 (리슨 호스트 본인은 바로 실행)
//
// [흐름 - 피해자 판정]
//   서버: ULOAHitJudgeSubsystem → 판정 대상인 원격 플레이어에게 ClientRequestHitJudge(번호, 모양, 자리, 방향, 판정 시각)
//   클라: ULOAHitJudgeSubsystem::JudgeLocal → 그 시각의 내 위치로 판정 → 맞음: ServerReportHitJudgeHit(번호) / 안 맞음: ServerReportHitJudgeMiss(번호, 그 위치)
//   서버: 보낸 PC의 캐릭터를 찾아 → ULOAHitJudgeSubsystem::ReceiveReport (받아 줄지는 판정 서브시스템이 정함)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/Combat/DamageNumber/LOADamageNumberTypes.h"   // RPC 인자로 값을 받으므로 전방 선언이 아니라 include
#include "Combat/Telegraph/LOATelegraphTypes.h"             // 같은 이유 (FLOATelegraphData)
#include "Library/LOACombatTypes.h"                         // 같은 이유 (FLOAHitShape)
#include "LOACombatRPCComponent.generated.h"

UCLASS(ClassGroup = (LOA), meta = (BlueprintSpawnableComponent))
class LOA_API ULOACombatRPCComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULOACombatRPCComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------- 서버 → 이 플레이어 컴퓨터 ----------

	/**
	 * [서버 → 이 플레이어 컴퓨터] 내가 때린 데미지 숫자를 띄워라.
	 * Info: 데미지 + 맞은 지점(대상 기준 상대 위치). 항목이 늘어도 이 함수 모양은 그대로.
	 */
	UFUNCTION(Client, Unreliable)
	void ClientShowDamageNumber(AActor* Target, const FLOADamageNumberInfo& Info);

	/**
	 * [서버 → 이 플레이어 컴퓨터] 예고를 그려라. 받은 값을 이 컴퓨터의 ULOATelegraphSubsystem에 넘기기만 한다.
	 * Reliable: 반드시 도착한다. (같은 컴포넌트의 Reliable RPC끼리는 보낸 순서대로 도착 → Hide가 Show보다 먼저 오지 않음)
	 */
	UFUNCTION(Client, Reliable)
	void ClientShowTelegraph(const FLOATelegraphData& Data);

	/** [서버 → 이 플레이어 컴퓨터] 번호의 예고를 지워라. (판정 전에 패턴이 취소됨) */
	UFUNCTION(Client, Reliable)
	void ClientHideTelegraph(int32 TelegraphId);

	/**
	 * [서버 → 이 플레이어 컴퓨터] 피해자 판정: "이 시각, 이 모양에 네가 있었나" 판정해서 보고해라.
	 * 이 컴퓨터의 ULOAHitJudgeSubsystem에 넘기기만 한다.
	 * @param RequestId      판정 번호 (보고할 때 그대로 돌려준다)
	 * @param ShapeCenter    모양 기준점 (Sphere 중심 / Fan 꼭짓점 / Box 뒷변 가운데)
	 * @param Yaw            모양 방향 (도)
	 * @param HitServerTime    서버가 판정한 시각
	 * @param bJudgeOnArrival  true(모션 공격) = 받은 순간의 내 위치 / false(예고 공격) = 기록에서 HitServerTime의 내 위치
	 */
	UFUNCTION(Client, Reliable)
	void ClientRequestHitJudge(int32 RequestId, const FLOAHitShape& Shape, FVector_NetQuantize10 ShapeCenter, float Yaw,
		double HitServerTime, bool bJudgeOnArrival);

	// ---------- 이 플레이어 컴퓨터 → 서버 ----------

	/**
	 * [이 플레이어 컴퓨터 → 서버] 피해자 판정 결과 "맞음"을 보고한다. 검증이 필요 없어 번호만 보낸다.
	 * 서버에서 보낸 플레이어의 캐릭터를 찾아 ULOAHitJudgeSubsystem에 넘기기만 한다.
	 * @param RequestId  ClientRequestHitJudge로 받은 번호
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportHitJudgeHit(int32 RequestId);

	/**
	 * [이 플레이어 컴퓨터 → 서버] 피해자 판정 결과 "안 맞음"을 판정에 쓴 내 위치와 함께 보고한다. (서버가 위치로 검증)
	 * 서버에서 보낸 플레이어의 캐릭터를 찾아 ULOAHitJudgeSubsystem에 넘기기만 한다.
	 * @param RequestId         ClientRequestHitJudge로 받은 번호
	 * @param ReportedLocation  판정에 쓴 내 위치 (1cm 단위로 압축)
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportHitJudgeMiss(int32 RequestId, FVector_NetQuantize ReportedLocation);

private:
	/** [서버] 보고 두 개(맞음 / 안 맞음)의 공통: 보낸 PC의 캐릭터를 찾아 판정 서브시스템에 넘긴다. */
	void ForwardHitJudgeReport(int32 RequestId, bool bHit, const FVector& ReportedLocation) const;
};
