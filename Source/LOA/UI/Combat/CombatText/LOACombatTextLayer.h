// LOACombatTextLayer.h
// [배관] 전투 글자(데미지 숫자 / 회복 숫자 / 버프·디버프·상태 이상 이름)를 띄우는 "화면 전체 1장짜리" 위젯. 글자 위젯을 미리 만들어 두고 돌려쓴다. (오브젝트 풀)
// (2026-11-16 apply116: 옛 ULOADamageNumberLayer. 회복·버프도 같은 레이어·같은 위젯으로. 클래스·칸 이름은 CoreRedirects로 이어짐)
//
// ─────────────────────────────────────────────────────────────────────────────
// [구조]
//   HUD
//    └ 전투 글자 레이어 (이 클래스, 화면 전체, Overlay보다 아래에 그림)
//        └ Canvas Panel
//            ├ 글자 위젯 (쉬는 중: Collapsed)
//            ├ 글자 위젯 (떠 있음: 매 프레임 위치 갱신)
//            └ ...
//
// [흐름]
//   데미지·회복 : 서버 → ULOACombatRPCComponent::ClientShowCombatText → HUD::ShowCombatText → ShowText (여기)
//   버프·디버프·상태 이상 : 내 컴퓨터 HUD (내 캐릭터에 상태 태그가 처음 붙음) → HUD::ShowCombatText → ShowText (여기)
//      ① Buff·Debuff·Status면 상태 정보 표(StatusInfoData)에서 그 태그의 줄을 찾는다. 표에 없는 태그는 띄우지 않는다 (아이콘 줄과 같은 규칙)
//      ② 쉬는 글자를 하나 꺼낸다 (없으면 MaxCount까지 새로 만들고, 그래도 없으면 가장 오래된 걸 재사용)
//      ③ 대상과 "대상 기준 상대 위치"를 기억하고 글자를 넣는다 (WBP가 Kind를 보고 모양 + 애니메이션)
//      ④ NativeTick: 떠 있는 글자만 (대상 현재 위치 + 상대 위치) → 화면 위치로 옮긴다. 카메라 뒤면 숨긴다
//      ⑤ LifeTime이 지나면 숨기고 풀에 돌려놓는다
//
// [왜 위젯 컴포넌트(액터)가 아닌가]
// 글자마다 액터를 만들고 지우면 생성/삭제 비용이 매번 든다. (보스전은 초당 수십 개)
// 위젯 컴포넌트의 깊이/원근 기능도 로아식 숫자에는 필요 없다. 화면 좌표 변환만 하면 된다.
//
// [위치] 뜨는 순간 "대상 기준 상대 위치"를 한 번 정하고, 이후로는 대상을 따라간다.
//   데미지 - 히트 판정 공격 : 서버가 보낸 맞은 지점 (대상 충돌체 표면에서 공격 범위 중심과 가장 가까운 점)
//   데미지 - 도트 등        : 대상 충돌체 표면에서 "내 캐릭터"와 가장 가까운 점 (이 클라가 계산. 순수 연출이라 서버 불필요)
//   데미지 - 둘 다 안 되면  : 대상 충돌체 위쪽 끝 (내 캐릭터가 없을 때 등)
//   회복 · 버프 · 디버프 · 상태 이상 · 받은 피해 : 대상 캡슐 가운데 = 대상 액터 위치 그대로 (사용자 2026-11-16 apply117. 전에는 머리 위)
//     기준점도 글자 가운데 (BodyTextAlignment, apply118). 전에는 글자 아래쪽이 캡슐 가운데에 와서 글자가 위로 떠 보였다
//     맞은 지점이 없는 효과라 HitResult를 쓰지 않는다. 대상 = 내 캐릭터라 "내 캐릭터와 가장 가까운 점"도 뜻이 없다
// 상대 위치는 월드 기준이다. 대상이 회전해도 글자가 대상 주위를 돌지 않고 이동만 따라간다.
// 대상이 사라지면(사망 후 제거 등) 마지막 위치에서 수명까지 마저 뜬다. (막타 숫자가 안 보이는 일 없게)
//
// [수명 - 누가 지우나]
//   - 평소: NativeTick이 LifeTime 지난 글자를 풀에 반납
//   - 반납을 놓쳐도(틱이 멈춤 등) 글자가 쌓이지 않는다: MaxCount를 넘으면 가장 오래된 걸 재사용
//   - 레이어 자체는 HUD가 EndPlay에서 지운다
//   - 대상은 약한 참조라 글자가 대상을 붙잡아 두지 않는다
//
// [가장 최근 데미지만 또렷하게] (사용자 2026-11-16 apply118: 눈이 아프다)
//   내가 때린 데미지 글자가 새로 뜨면, 바로 전 데미지 글자의 명도를 낮춘다 (위젯 전체 색에 FadedBrightness를 곱함 → 색깔은 그대로, 어둡게만)
//   예) 330 → 512(치명타) → 280 연타: 280만 원래 색, 330·512는 절반 밝기. 512의 크기는 그대로
//   받은 피해·회복·버프 글자는 건드리지 않는다. 풀에서 다시 쓸 때 원래 밝기로 되돌린다
//
// [네트워크]
// 본인 컴퓨터 화면 전용 연출. 이 위젯은 판정에 관여하지 않는다.
//
// [마우스]
// 화면 전체를 덮는 위젯이라, 클릭을 막지 않도록 레이어와 글자 모두 HitTestInvisible로 둔다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] WBP_DamageNumberLayer (부모: LOACombatTextLayer. 에셋 이름은 그대로 두거나 WBP_CombatTextLayer로 Rename)
//   디자이너는 비워 둔다. (Canvas Panel은 코드가 만든다. 직접 넣고 싶으면 루트를 Canvas Panel로)
//   Class Defaults → Text Widget Class = WBP_DamageNumber(전투 글자 WBP), Status Info Data = DA_StatusInfo (2026-11-16, 옛 칸 이름 StatusIconData), 나머지 수치 조정
//   그리고 HUD BP → Combat Text Layer Class = 이 WBP

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Combat/CombatText/LOACombatTextTypes.h"
#include "LOACombatTextLayer.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class ULOACombatTextWidget;
class ULOAStatusInfoData;

/** 풀 안의 글자 하나 */
USTRUCT()
struct FLOACombatTextEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULOACombatTextWidget> Widget;

	/** Canvas 안에서의 자리 (위치, 그리는 순서) */
	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> CanvasSlot;

	/** 따라갈 대상. 약한 참조 (대상이 지워져도 붙잡지 않음) */
	TWeakObjectPtr<AActor> Target;

	/** 대상 위치 기준 상대 위치 (월드 기준). 뜨는 순간 한 번 정한다 */
	FVector Offset = FVector::ZeroVector;

	/** 마지막으로 계산한 월드 위치. 대상이 사라지면 여기에 남는다 */
	FVector WorldLocation = FVector::ZeroVector;

	/** 뜬 뒤 지난 시간 (초). 가장 오래된 글자를 고를 때도 쓴다 */
	float Age = 0.f;

	/** 지금 화면에 떠 있는가 (false = 풀에서 쉬는 중) */
	bool bActive = false;

	/** 지금 띄운 글자의 종류. 흐리게 할 대상(내가 때린 데미지)인지 볼 때 (apply118) */
	ELOACombatTextKind Kind = ELOACombatTextKind::Damage;
};

UCLASS(Abstract)
class LOA_API ULOACombatTextLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 대상에 전투 글자 하나를 띄운다. [본인 컴퓨터] HUD가 호출한다. 위치·상태 표는 헤더 [흐름] [위치] */
	void ShowText(AActor* Target, const FLOACombatTextInfo& Info);

protected:
	//~ Begin UUserWidget Interface
	/** Canvas 준비 + 글자 PrewarmCount개 미리 만들기 */
	virtual void NativeOnInitialized() override;

	/** 떠 있는 글자들의 위치 갱신 + 수명 끝난 글자 반납 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

private:
	/** 글자 위젯 하나를 새로 만들어 풀에 넣는다. 실패하면 INDEX_NONE */
	int32 CreateEntry();

	/** 쓸 글자를 고른다: 쉬는 것 → 새로 만들기(MaxCount까지) → 가장 오래된 것 */
	int32 AcquireEntry();

	/** 숨기고 풀에 돌려놓는다 */
	void ReleaseEntry(FLOACombatTextEntry& Entry);

	/** 대상 기준 상대 위치를 정한다. 헤더 [위치] (데미지: 서버가 보낸 맞은 지점 → 내 캐릭터와 가장 가까운 점 → 대상 위쪽 끝 / 그 밖: 캡슐 가운데) */
	FVector CalcOffset(const AActor* Target, const FLOACombatTextInfo& Info) const;

	/** 대상을 따라 월드 위치를 갱신하고 화면 위치로 옮긴다. 카메라 뒤면 숨긴다 */
	void UpdateEntryPosition(FLOACombatTextEntry& Entry) const;

	// ---------- 데이터 (WBP Class Defaults에서 지정) ----------
	/** 글자 하나의 모양 (전투 글자 WBP). 비어 있으면 글자를 띄우지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText")
	TSubclassOf<ULOACombatTextWidget> TextWidgetClass;

	/** 상태 정보 표 (DA_StatusInfo). Buff·Debuff·Status 글자의 이름을 여기서 찾는다. 비어 있으면 그 글자들을 띄우지 않는다 (2026-11-16, 옛 칸 이름 StatusIconData) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText")
	TObjectPtr<const ULOAStatusInfoData> StatusInfoData;

	/** 시작할 때 미리 만들어 둘 개수. 전투 중 첫 생성 비용을 없앤다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText", meta = (ClampMin = "0"))
	int32 PrewarmCount = 16;

	/** 최대 개수. 넘으면 가장 오래된 글자를 재사용한다 (메모리 상한) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText", meta = (ClampMin = "1"))
	int32 MaxCount = 64;

	/** 화면에 남아 있는 시간 (초). WBP 애니메이션 길이에 맞춘다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText", meta = (ClampMin = "0.1", Units = "Seconds"))
	float LifeTime = 1.f;

	/** 맞은 지점에서 얼마나 더 위에 띄울지 (cm). 0이면 맞은 지점 그대로 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText", meta = (Units = "Centimeters"))
	float HeightOffset = 0.f;

	/** 맞은 지점 주변으로 흩뿌리는 최대 거리 (cm). 같은 곳을 연타해도 글자가 한 점에 겹치지 않게 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float RandomOffset = 20.f;

	/** 데미지 글자 위젯의 기준점. (0.5, 1.0) = 가로 가운데, 세로 아래쪽이 맞은 지점에 온다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText")
	FVector2D TextAlignment = FVector2D(0.5f, 1.f);

	/** 캡슐 가운데에 뜨는 글자(회복·버프·디버프·상태 이상·받은 피해)의 기준점. (0.5, 0.5) = 글자 한가운데가 캡슐 가운데에 온다 (apply118) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText")
	FVector2D BodyTextAlignment = FVector2D(0.5f, 0.5f);

	/** 지난 데미지 글자의 밝기 (0~1). 0.5 = 절반 밝기. 헤더 [가장 최근 데미지만 또렷하게] (apply118) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|CombatText", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadedBrightness = 0.5f;

	// ---------- 런타임 ----------
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> TextCanvas;

	/** 풀. 쉬는 글자와 떠 있는 글자가 함께 들어 있다 (bActive로 구분) */
	UPROPERTY(Transient)
	TArray<FLOACombatTextEntry> Entries;

	/** 떠 있는 글자 수. 0이면 틱에서 아무것도 안 한다 */
	int32 ActiveCount = 0;

	/** 새 글자가 위에 그려지도록 늘려 가는 순서 값. 모두 반납되면 0으로 */
	int32 NextZOrder = 0;

	/** 가장 최근에 띄운 내 데미지 글자의 풀 번호. 없으면 INDEX_NONE (apply118) */
	int32 LatestDamageIndex = INDEX_NONE;

	/** 가장 최근 데미지 글자를 어둡게 한다 (새 데미지 글자 NewIndex가 뜰 때). 헤더 [가장 최근 데미지만 또렷하게] */
	void FadeLatestDamage(int32 NewIndex);
};
