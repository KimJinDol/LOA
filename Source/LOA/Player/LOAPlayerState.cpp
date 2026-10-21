// LOAPlayerState.cpp

#include "Player/LOAPlayerState.h"
#include "AbilitySystem/LOAAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Common.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Mana.h"
#include "AbilitySystem/Attributes/LOAAttributeSet_Combat.h"
#include "Data/LOACharacterClassData.h"
#include "Data/LOAAbilityDataSet.h"
#include "Core/LOALog.h"

ALOAPlayerState::ALOAPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ---------- ASC ----------
	AbilitySystemComponent = CreateDefaultSubobject<ULOAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// ASC 자체도 네트워크로 복제되어야 스킬/버프 정보가 클라이언트에 전달된다.
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed : 본인에게는 버프/디버프 상세 정보까지, 다른 플레이어에게는 최소 정보만.
	// 플레이어 캐릭터에 권장되는 모드. (보스는 보스 쪽에서 Minimal로 설정)
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// ---------- 공통 스탯창 ----------
	// PlayerState의 하위 객체로 만들어 두면, ASC가 초기화될 때 자동으로 찾아서 등록한다.
	// (별도로 "등록" 코드를 쓸 필요 없음)
	CommonAttributeSet = CreateDefaultSubobject<ULOAAttributeSet_Common>(TEXT("CommonAttributeSet"));

	// ---------- 마나 스탯창 ----------
	// 모든 직업이 마나를 쓰므로 Common과 같은 방식으로 기본으로 만든다. (ASC가 하위 객체로 자동 등록)
	// Common에서 분리한 이유: 보스/허수아비는 마나가 없다. (LOAAttributeSet_Common.h [스탯창 나누는 기준])
	ManaAttributeSet = CreateDefaultSubobject<ULOAAttributeSet_Mana>(TEXT("ManaAttributeSet"));

	// ---------- 전투 스탯창 ----------
	// 모든 직업이 공격하므로 Common과 같은 방식으로 기본으로 만든다. (ASC가 하위 객체로 자동 등록)
	CombatAttributeSet = CreateDefaultSubobject<ULOAAttributeSet_Combat>(TEXT("CombatAttributeSet"));

	// 복제 빈도(Net Update Frequency)는 여기서 숫자로 정하지 않는다.
	// BP_LOAPlayerState의 Class Defaults → Replication 항목에서 조정한다.
}

UAbilitySystemComponent* ALOAPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ALOAPlayerState::GrantClassAbilityDataSets(const ULOACharacterClassData* ClassData)
{
	// [서버 전용] 부여는 서버만 할 수 있다.
	if (!HasAuthority() || !ClassData || !AbilitySystemComponent)
	{
		return;
	}

	// 이미 부여했다
	if (GrantedClassData)
	{
		if (GrantedClassData != ClassData)
		{
			// 맵 안에서 직업은 바뀌지 않는다는 규칙 위반
			LOA_LOG(this, Warning, TEXT("Class already granted as [%s]. Request for [%s] ignored."),
				*GetNameSafe(GrantedClassData), *GetNameSafe(ClassData));
		}
		// 같은 직업이면 부활한 캐릭터의 요청 → ASC에 이미 남아 있으므로 조용히 건너뜀
		return;
	}

	// 목록 순서대로 부여한다. 회수할 일이 없으므로 번호표는 받지 않는다(nullptr).
	// SourceObject = 직업 데이터 → "이 Spec/Effect는 버서커 직업 데이터에서 왔다"는 출처 표시
	for (const ULOAPlayerAbilityDataSet* DataSet : ClassData->DefaultAbilityDataSets)
	{
		if (DataSet)
		{
			DataSet->GiveToAbilitySystem(AbilitySystemComponent, nullptr, const_cast<ULOACharacterClassData*>(ClassData));
		}
	}

	GrantedClassData = ClassData;

	LOA_LOG(this, Log, TEXT("Class [%s] granted."), *GetNameSafe(ClassData));
}