// LOAStatusInfoData.h
// [콘텐츠] 상태 정보 표: 상태(버프·디버프·상태 이상) 태그 → 아이콘 · 이름 · 툴팁 설명. (2026-11-10)
// (2026-11-16 apply117: 옛 LOAStatusIconData / FLOAStatusIconInfo / Icons / DA_StatusIcons.
//  아이콘만이 아니라 이름·설명·전투 글자까지 맡게 되어 "상태 정보"로. 클래스·구조체·칸 이름은 CoreRedirects로 이어짐)
//
// [쓰는 곳]
//   상태 아이콘 줄 (파티 틀, 내 버프 줄) : 아이콘 + 툴팁 (ULOAStatusIconRow)
//   전투 글자 (내 캐릭터에 처음 붙음)    : 이름만 (ULOACombatTextLayer). 아이콘은 안 씀 (사용자 11-16)
//
//   DA_StatusInfo
//     State.Buff.AttackPower  → T_Passive_AttackPowerBuff  "공격력 증가"
//     State.Buff.DamageDealt  → T_Passive_DamageDealtBuff  "주는 피해 증가"
//     State.Buff.Shield       → (보호막 스킬 아이콘, 임시)   "보호막"
//     State.CrowdControl.Status.Stunned → (스턴 아이콘)     "기절"   (2026-11-10: 상태 이상도 표에 넣는다)
//
// [툴팁] (2026-11-14) 이름 + 설명. 남은 시간은 넣지 않는다 (사용자 11-13)
//   설명의 % 숫자는 Values 칸에서 (2026-11-14 apply110, 사용자 A: 숫자는 커브 테이블 한 곳)
//     {Value0} {Value1} … = Values 순서. {Value} = {Value0}
//       Values[i] = FScalableFloat → GE(또는 스킬 Payload)가 가리키는 것과 **같은 커브 테이블 행**을 가리킨다
//         → 계수를 커브에서 고치면 게임과 툴팁이 같이 바뀐다 (숫자를 두 군데 적지 않음)
//       bMultiply = 곱하기 값 (사용자 a) → (값 − 1) × 100. 아니면 값 × 100. 부호는 뗀다 (증가/감소는 글이 말함)
//       예) GE_AttackBuff [0] Coefficient → CT_SkillValues "Serath.Buff.AttackFromCaster" 0.15
//                         [1] 곱하기     → CT_SkillValues "Serath.Buff.AttackMultiply" 1.06
//           공격력 증가 줄 Values = [ {같은 행 0.15}, {같은 행 1.06, bMultiply} ] → {Value0} = 15, {Value1} = 6
//           "버프 시전자의 공격력의 {Value0}%만큼 공격력이 증가하고, 추가로 전체 공격력이 {Value1}% 증가한다." (사용자 11-14)
//       GE의 계산 타입(Scalable Float / Attribute Based / SetByCaller)과 상관없다. 낙인(SetByCaller)도 스킬 Payload가 가리키는 행을 같이 가리키면 됨
//       커브가 아닌 숫자를 직접 적으면 저장할 때 경고 (ULOAStatusInfoData::IsDataValid)
//     {Duration} = Effect(그 상태를 주는 GE, 선택)의 고정 지속 시간 (초). SetByCaller 시간이면 빈칸
//   값이 없는 자리표시는 빈칸
//
// [규칙]
//   표에 없는 태그는 화면에 안 나온다 (경고도 안 함: 보이지 않아도 되는 상태도 있다)
//   줄 순서 = 화면 순서 (버프를 먼저 두는 식으로 여기서 정한다)
//   버프 GE가 State.Buff.* 하나를 주는 규칙(LOAGameplayTags.h [버프 / 디버프])에 기대고 있다
//
// [네트워크] 없음. 모든 컴퓨터에 똑같이 들어 있는 읽기 전용 데이터

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "LOAStatusInfoData.generated.h"

class UTexture2D;
class UGameplayEffect;

/** 툴팁 % 숫자 하나 ({Value0} {Value1} …). 헤더 [툴팁] */
USTRUCT(BlueprintType)
struct FLOATooltipValue
{
	GENERATED_BODY()

	/** 숫자. GE·스킬이 가리키는 것과 같은 커브 테이블 행을 가리킨다 (레벨 1로 읽음) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status")
	FScalableFloat Value;

	/** 곱하기 값이면 켠다 → (값 − 1) × 100. 예) 1.06 → 6 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status")
	bool bMultiply = false;
};

/** 상태 한 줄: 어떤 태그를 어떤 아이콘 · 이름 · 설명으로 (옛 FLOAStatusIconInfo) */
USTRUCT(BlueprintType)
struct FLOAStatusInfo
{
	GENERATED_BODY()

	/** State.Buff.* / State.Debuff.* / State.CrowdControl.Status.* */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status", meta = (Categories = "State.Buff,State.Debuff,State.CrowdControl.Status"))
	FGameplayTag StatusTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status")
	TObjectPtr<UTexture2D> Icon;

	/** 이름 (툴팁 제목, 전투 글자) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status")
	FText DisplayName;

	/** 툴팁 설명. 자리표시 {Value0} {Value1} … {Value} {Duration} (헤더 [툴팁]) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status", meta = (MultiLine = "true"))
	FText Description;

	/** 설명의 % 숫자 ({Value0}부터). 커브 테이블 행을 가리킨다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status")
	TArray<FLOATooltipValue> Values;

	/** (선택) 이 상태를 주는 GE. {Duration}(고정 지속 시간)만 여기서 읽는다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status")
	TSubclassOf<UGameplayEffect> Effect;

	/** 디버프·상태 이상이면 테두리를 붉게 (위젯이 정함) */
	bool IsDebuff() const;

	/** Description의 자리표시를 Values·Effect 숫자로 채운 글 (헤더 [툴팁]) */
	FText MakeDescription() const;
};

UCLASS(BlueprintType)
class LOA_API ULOAStatusInfoData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 이 태그의 줄. 없으면 nullptr (옛 FindIcon) */
	const FLOAStatusInfo* FindRow(const FGameplayTag& StatusTag) const;

#if WITH_EDITOR
	/** [에디터 전용] 저장 시 검사. 경고: Values에 커브 테이블이 아닌 숫자를 직접 적은 칸 (헤더 [툴팁]) */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** 화면 순서 = 이 순서 (옛 칸 이름 Icons, CoreRedirects) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Status", meta = (TitleProperty = "StatusTag"))
	TArray<FLOAStatusInfo> Rows;
};
