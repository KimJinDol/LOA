// LOACoopJustGuardResultWidget.h
// [화면] 협동 저가 결과. 보스 앞(캡슐 가운데 + 앞 Forward Offset)에 등급 글자 "PERFECT" + "+5초"를 띄우고 보스를 따라간다. (2026-11-18 apply128, 자리 apply129)
// (사용자 11-18: 대난투 UI(ULOABrawlBar)와 별개로 보스 몸에서. 그림 대신 입체 폰트 글자 + 뒤에 빛 장식 이펙트)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름] (각 컴퓨터, 3명 모두 같은 결과를 본다)
//   HUD가 BeginPlay에서 만들어 화면에 붙인다 (ALOAHUD의 Coop Just Guard Result Class)
//   NativeTick: 레이드 상황판에서 경기장(ALOABrawlArena)을 찾아 복제된 협동 저가 결과 번호(ResultId)를 본다
//     번호가 바뀜 → 등급 글자(Grade Texts) + 추가 초 글자 → ResultPanel 보임 → BP 이벤트 On Show Result (애니·이펙트·소리)
//     Show Seconds가 지나면 → ResultPanel 숨김
//   처음 경기장을 찾았을 때의 번호는 띄우지 않는다 (지난 대난투 결과가 다시 뜨지 않게)
//   예) 2명 성공 → 경기장 Great, +3초 복제 → 보스 앞에 "GREAT" + "+3초" 2초
//       Bad, +0초 → "BAD"만 (BonusText 숨김)
//
// [위치] 보이는 동안 매 프레임 보스 위치(= 캡슐 가운데) + 보스 앞 방향 × Forward Offset → 화면 위치 → ResultPanel의 캔버스 칸 위치
//   (2026-11-19 apply129 사용자: 보스 위치 + Forward Vector × 400. 전에는 캡슐 가운데 그대로)
//   예) 보스 (1690, 1770, 20), 앞 = +X → (2090, 1770, 20)에 뜬다. 보스가 돌면 그 앞으로 같이 돈다
//   보스 = 레이드 상황판의 지금 보스 (모든 컴퓨터에 등록돼 있다. 경기장의 대난투 보스는 서버만 안다)
//   카메라 뒤면 Hidden (애니는 계속). 보스가 없어지면 마지막 위치 그대로
//   전투 글자 레이어(ULOACombatTextLayer)와 같은 방식. 레이어에 넣지 않은 이유: 글자 위젯 풀(WBP_DamageNumber) 모양과 달라서 (입체 폰트 + 빛 장식 + 애니)
//
// [WBP] WBP_CoopJustGuardResult (부모 = 이 클래스)
//   루트 Canvas Panel (화면 전체). 루트는 숨기지 않는다 (숨기면 Tick이 멈춘다. ULOAJustGuardHoldBar와 같음)
//   ResultPanel (아무 위젯, 필수) : 루트 캔버스의 바로 아래. 숨기고 보일 묶음 + 보스를 따라 움직인다
//                                   캔버스 칸 Alignment (0.5, 0.5) → 묶음 가운데가 그 자리에. Size To Content 권장
//   GradeText   (Text Block, 선택): 등급 글자. 입체감은 WBP의 폰트·외곽선·그림자·폰트 재질로
//   BonusText   (Text Block, 선택): 추가 초 ("+5초"). 0초면 숨김
//   빛 장식 (Image + UI 재질 등): WBP에만 둔다. C++은 모른다 (ResultPanel 안에 있으면 같이 보이고 움직인다)
//   BP 기본값: Grade Texts (PERFECT / GREAT / BAD), Bonus Format, Show Seconds, Forward Offset (apply129)
//   BP 이벤트 On Show Result(Grade, BonusSeconds): 위젯 애니메이션 · 빛 장식 · 소리, 등급별 색·재질도 여기서 바꾼다
//     보스 몸에 월드 이펙트(나이아가라)를 쓰려면 Get Result Target으로 보스를 받아 Spawn System Attached (내 컴퓨터만)
//
// [네트워크] 없음. 이 컴퓨터에 복제된 경기장 결과를 읽기만 한다 (RPC 없음)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Library/LOACombatTypes.h"   // ELOAJustGuardGrade
#include "LOACoopJustGuardResultWidget.generated.h"

class UTextBlock;
class UCanvasPanelSlot;
class ALOABrawlArena;
struct FLOACoopJustGuardResult;

UCLASS(Abstract)
class LOA_API ULOACoopJustGuardResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULOACoopJustGuardResultWidget(const FObjectInitializer& ObjectInitializer);

	/** 결과를 띄운 대상 (보스). 보이는 중이 아니거나 보스가 없어졌으면 nullptr. BP에서 월드 이펙트를 붙일 때 */
	UFUNCTION(BlueprintPure, Category = "LOA|UI|Brawl")
	AActor* GetResultTarget() const { return Target.Get(); }

protected:
	//~ Begin UUserWidget Interface
	/** 처음엔 숨김 + 캔버스 칸 확인 */
	virtual void NativeConstruct() override;
	/** 결과 번호를 보고 띄우고, 보이는 동안 보스를 따라간다 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	/** 숨기고 보일 묶음. 루트 캔버스 바로 아래 (헤더 [WBP]) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> ResultPanel;

	/** 등급 글자 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GradeText;

	/** 추가 초 글자 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BonusText;

	/** 등급 글자. 기본 PERFECT / GREAT / BAD */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl")
	TMap<ELOAJustGuardGrade, FText> GradeTexts;

	/** 추가 초 글자. {Seconds} = 추가 초 (소수 없음). 0초면 BonusText 숨김 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl")
	FText BonusFormat;

	/** 결과를 보여 주는 시간 (초). 애니메이션 길이에 맞춘다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl", meta = (ClampMin = "0.1", Units = "Seconds"))
	float ShowSeconds = 2.f;

	/** 보스 앞으로 띄울 거리 (cm). 0 = 캡슐 가운데. 음수 = 뒤 (헤더 [위치], apply129) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|UI|Brawl", meta = (Units = "Centimeters"))
	float ForwardOffset = 400.f;

	/** 새 협동 저가 결과 (글자는 C++이 채움, 패널 보임 뒤). 애니·빛 장식·소리, 등급별 색·재질을 WBP에서 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LOA|UI|Brawl")
	void OnShowResult(ELOAJustGuardGrade Grade, float BonusSeconds);

private:
	/** 새 결과: 글자 채우고 보임 + BP 이벤트 */
	void ShowResult(const FLOACoopJustGuardResult& Result);

	/** 숨김 (보이는 중일 때만) */
	void HideResult();

	/** 대상 위치 → 화면 위치. 카메라 뒤면 Hidden (헤더 [위치]) */
	void UpdatePosition();

	/** 마지막으로 본 결과 번호. 음수 = 아직 경기장을 못 봄 (처음 본 번호는 띄우지 않는다) */
	int32 LastSeenResultId = -1;

	/** 숨길 시각 (이 컴퓨터의 월드 시각). 0 = 안 보이는 중 */
	double HideTime = 0.0;

	/** 따라갈 대상 (보스) / 마지막 위치 (대상이 없어지면 그대로) */
	TWeakObjectPtr<AActor> Target;
	FVector LastWorldLocation = FVector::ZeroVector;

	/** ResultPanel의 캔버스 칸. 없으면(루트가 캔버스가 아님) 움직이지 않는다 */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PanelSlot;
};
