// LOADamageNumberLayer.h
// [배관] 데미지 숫자를 띄우는 "화면 전체 1장짜리" 위젯. 숫자 위젯을 미리 만들어 두고 돌려쓴다. (오브젝트 풀)
//
// ─────────────────────────────────────────────────────────────────────────────
// [구조]
//   HUD
//    └ 데미지 숫자 레이어 (이 클래스, 화면 전체, Overlay보다 아래에 그림)
//        └ Canvas Panel
//            ├ 숫자 위젯 (쉬는 중: Collapsed)
//            ├ 숫자 위젯 (떠 있음: 매 프레임 위치 갱신)
//            └ ...
//
// [흐름]
//   서버(데미지 확정) → ULOACombatFeedbackComponent::ClientShowDamageNumber → HUD::ShowDamageNumber → ShowDamage (여기)
//      ① 쉬는 숫자를 하나 꺼낸다 (없으면 MaxCount까지 새로 만들고, 그래도 없으면 가장 오래된 걸 재사용)
//      ② 대상과 "대상 기준 상대 위치"를 기억하고 숫자를 넣는다 (WBP가 글자 + 애니메이션)
//      ③ NativeTick: 떠 있는 숫자만 (대상 현재 위치 + 상대 위치) → 화면 위치로 옮긴다. 카메라 뒤면 숨긴다
//      ④ LifeTime이 지나면 숨기고 풀에 돌려놓는다
//
// [왜 위젯 컴포넌트(액터)가 아닌가]
// 숫자마다 액터를 만들고 지우면 생성/삭제 비용이 매번 든다. (보스전은 초당 수십 개)
// 위젯 컴포넌트의 깊이/원근 기능도 로아식 숫자에는 필요 없다. 화면 좌표 변환만 하면 된다.
//
// [위치] 뜨는 순간 "대상 기준 상대 위치"를 한 번 정하고, 이후로는 대상을 따라간다.
//   히트 판정 공격 : 서버가 보낸 맞은 지점 (대상 충돌체 표면에서 공격 범위 중심과 가장 가까운 점)
//   도트 등        : 대상 충돌체 표면에서 "내 캐릭터"와 가장 가까운 점 (이 클라가 계산. 순수 연출이라 서버 불필요)
//   둘 다 안 되면  : 대상 충돌체 위쪽 끝 (내 캐릭터가 없을 때 등)
// 상대 위치는 월드 기준이다. 대상이 회전해도 숫자가 대상 주위를 돌지 않고 이동만 따라간다.
// 대상이 사라지면(사망 후 제거 등) 마지막 위치에서 수명까지 마저 뜬다. (막타 숫자가 안 보이는 일 없게)
//
// [수명 - 누가 지우나]
//   - 평소: NativeTick이 LifeTime 지난 숫자를 풀에 반납
//   - 반납을 놓쳐도(틱이 멈춤 등) 숫자가 쌓이지 않는다: MaxCount를 넘으면 가장 오래된 걸 재사용
//   - 레이어 자체는 HUD가 EndPlay에서 지운다
//   - 대상은 약한 참조라 숫자가 대상을 붙잡아 두지 않는다
//
// [네트워크]
// 본인 컴퓨터 화면 전용 연출. 서버는 RPC로 "얼마 때렸다"만 알려 주고, 이 위젯은 판정에 관여하지 않는다.
//
// [마우스]
// 화면 전체를 덮는 위젯이라, 클릭을 막지 않도록 레이어와 숫자 모두 HitTestInvisible로 둔다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [BP에서 설정할 것] WBP_DamageNumberLayer (부모: LOADamageNumberLayer)
//   디자이너는 비워 둔다. (Canvas Panel은 코드가 만든다. 직접 넣고 싶으면 루트를 Canvas Panel로)
//   Class Defaults → Number Widget Class = WBP_DamageNumber, 나머지 수치 조정
//   그리고 BP_LOAHUD → Damage Number Layer Class = WBP_DamageNumberLayer

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Combat/DamageNumber/LOADamageNumberTypes.h"
#include "LOADamageNumberLayer.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class ULOADamageNumberWidget;

/** 풀 안의 숫자 하나 */
USTRUCT()
struct FLOADamageNumberEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULOADamageNumberWidget> Widget;

	/** Canvas 안에서의 자리 (위치, 그리는 순서) */
	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> CanvasSlot;

	/** 따라갈 대상. 약한 참조 (대상이 지워져도 붙잡지 않음) */
	TWeakObjectPtr<AActor> Target;

	/** 대상 위치 기준 상대 위치 (월드 기준). 뜨는 순간 한 번 정한다 */
	FVector Offset = FVector::ZeroVector;

	/** 마지막으로 계산한 월드 위치. 대상이 사라지면 여기에 남는다 */
	FVector WorldLocation = FVector::ZeroVector;

	/** 뜬 뒤 지난 시간 (초). 가장 오래된 숫자를 고를 때도 쓴다 */
	float Age = 0.f;

	/** 지금 화면에 떠 있는가 (false = 풀에서 쉬는 중) */
	bool bActive = false;
};

UCLASS(Abstract)
class LOA_API ULOADamageNumberLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 대상의 맞은 지점에 데미지 숫자를 띄운다. [본인 컴퓨터] HUD가 호출한다. */
	void ShowDamage(AActor* Target, const FLOADamageNumberInfo& Info);

protected:
	//~ Begin UUserWidget Interface
	/** Canvas 준비 + 숫자 PrewarmCount개 미리 만들기 */
	virtual void NativeOnInitialized() override;

	/** 떠 있는 숫자들의 위치 갱신 + 수명 끝난 숫자 반납 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

private:
	/** 숫자 위젯 하나를 새로 만들어 풀에 넣는다. 실패하면 INDEX_NONE */
	int32 CreateEntry();

	/** 쓸 숫자를 고른다: 쉬는 것 → 새로 만들기(MaxCount까지) → 가장 오래된 것 */
	int32 AcquireEntry();

	/** 숨기고 풀에 돌려놓는다 */
	void ReleaseEntry(FLOADamageNumberEntry& Entry);

	/** 대상 기준 상대 위치를 정한다. (서버가 보낸 맞은 지점 → 내 캐릭터와 가장 가까운 점 → 대상 위쪽 끝) */
	FVector CalcOffset(const AActor* Target, const FLOADamageNumberInfo& Info) const;

	/** 대상을 따라 월드 위치를 갱신하고 화면 위치로 옮긴다. 카메라 뒤면 숨긴다 */
	void UpdateEntryPosition(FLOADamageNumberEntry& Entry) const;

	// ---------- 데이터 (WBP Class Defaults에서 지정) ----------
	/** 숫자 하나의 모양 (WBP_DamageNumber). 비어 있으면 숫자를 띄우지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|DamageNumber")
	TSubclassOf<ULOADamageNumberWidget> NumberWidgetClass;

	/** 시작할 때 미리 만들어 둘 개수. 전투 중 첫 생성 비용을 없앤다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|DamageNumber", meta = (ClampMin = "0"))
	int32 PrewarmCount = 16;

	/** 최대 개수. 넘으면 가장 오래된 숫자를 재사용한다 (메모리 상한) */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|DamageNumber", meta = (ClampMin = "1"))
	int32 MaxCount = 64;

	/** 화면에 남아 있는 시간 (초). WBP 애니메이션 길이에 맞춘다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|DamageNumber", meta = (ClampMin = "0.1", Units = "Seconds"))
	float LifeTime = 1.f;

	/** 맞은 지점에서 얼마나 더 위에 띄울지 (cm). 0이면 맞은 지점 그대로 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|DamageNumber", meta = (Units = "Centimeters"))
	float HeightOffset = 0.f;

	/** 맞은 지점 주변으로 흩뿌리는 최대 거리 (cm). 같은 곳을 연타해도 숫자가 한 점에 겹치지 않게 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|DamageNumber", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float RandomOffset = 20.f;

	/** 숫자 위젯의 기준점. (0.5, 1.0) = 가로 가운데, 세로 아래쪽이 월드 위치에 온다 */
	UPROPERTY(EditDefaultsOnly, Category = "LOA|DamageNumber")
	FVector2D NumberAlignment = FVector2D(0.5f, 1.f);

	// ---------- 런타임 ----------
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> NumberCanvas;

	/** 풀. 쉬는 숫자와 떠 있는 숫자가 함께 들어 있다 (bActive로 구분) */
	UPROPERTY(Transient)
	TArray<FLOADamageNumberEntry> Entries;

	/** 떠 있는 숫자 수. 0이면 틱에서 아무것도 안 한다 */
	int32 ActiveCount = 0;

	/** 새 숫자가 위에 그려지도록 늘려 가는 순서 값. 모두 반납되면 0으로 */
	int32 NextZOrder = 0;
};
