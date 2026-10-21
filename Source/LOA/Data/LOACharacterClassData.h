// LOACharacterClassData.h
// [콘텐츠] 직업 데이터. "이 직업은 무엇으로 이루어져 있는가"를 한 장에 모은 데이터 에셋.
//
// ─────────────────────────────────────────────────────────────────────────────
// [한 장에 들어가는 것]
//
//   DA_Class_Berserker
//    ├─ 직업 태그 / 이름 / 아이콘        : UI 표시, 직업 구분
//    ├─ 외형 (메시, 애님 BP, 메시 위치)   : 캐릭터 몸에 적용
//    ├─ 항상 장착 AbilityDataSet 목록     : 회피, 기본 공격, 일반 스킬, 아이덴티티, 초기 스탯
//    └─ 직업 전용 컴포넌트 목록 (선택)    : 직업만의 특수 기능이 필요할 때
//
// 새 직업 추가 = 이 에셋을 하나 더 만드는 것. C++ 수정 없음.
//
// ─────────────────────────────────────────────────────────────────────────────
// [여기 없는 것]
// - 키 입력 설정: 모든 직업이 같은 키 배치를 쓰므로 PlayerController의 InputConfig가 담당한다.
// - 수치(체력, 공격력 등): AbilityDataSet 안의 초기화 Effect → 커브 테이블이 담당한다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [UPrimaryDataAsset인 이유]
// 직업 데이터는 "직업 선택 화면에서 목록 보여주기", "필요할 때만 로드하기" 처럼
// 에셋 매니저로 찾고 관리할 대상이다. 그래서 UDataAsset이 아니라 UPrimaryDataAsset으로 만든다.
// → 프로젝트 설정 > Asset Manager > Primary Asset Types to Scan 에 "CharacterClass" 등록 필요 (아래 GetPrimaryAssetId 참고)
//
// ─────────────────────────────────────────────────────────────────────────────
// [외형 에셋을 Soft 참조로 두는 이유]
// 하드 참조(TObjectPtr)로 두면, 직업 데이터 목록만 읽어도 모든 직업의 메시/애님이 메모리에 올라온다.
// Soft 참조는 "경로만" 들고 있다가 실제로 필요할 때 로드한다.
// AssetBundles = "Avatar" 표시는 나중에 에셋 매니저로 "외형만 미리 비동기 로드"할 때 쓰는 이름표다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// - 이 에셋은 모든 컴퓨터에 똑같이 들어 있는 "읽기 전용" 데이터다. 에셋 자체를 전송하지 않는다.
// - 서버가 캐릭터에 "어떤 직업 데이터인지"를 정하면, 캐릭터가 에셋 "경로"만 복제한다. (ALOACharacter::CharacterClassData)
// - AbilityDataSet 부여는 서버에서만 일어난다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "LOACharacterClassData.generated.h"

class ULOAPlayerAbilityDataSet;
class USkeletalMesh;
class UAnimInstance;
class UTexture2D;
class UActorComponent;

// =============================================================================
// 직업 전용 컴포넌트 한 개
// =============================================================================

/**
 * 직업 전용 컴포넌트를 "어느 컴퓨터에서" 만들지.
 * 컴포넌트마다 필요한 곳이 다르기 때문에 데이터에서 고른다.
 */
UENUM(BlueprintType)
enum class ELOAClassComponentNetPolicy : uint8
{
	/** 모든 컴퓨터 (서버 + 모든 클라이언트). 예: 외형 연출용 컴포넌트 */
	AllMachines,

	/** 서버에서만. 예: 판정/로직 전용 컴포넌트 */
	ServerOnly,

	/** 이 캐릭터를 조종하는 사람의 컴퓨터에서만. 예: 조준 보조, 로컬 UI 연동 */
	OwningClientOnly,
};

USTRUCT(BlueprintType)
struct FLOAClassComponentEntry
{
	GENERATED_BODY()

	/** 캐릭터에 붙일 컴포넌트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|ClassComponent")
	TSoftClassPtr<UActorComponent> ComponentClass;

	/** 어느 컴퓨터에서 만들지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|ClassComponent")
	ELOAClassComponentNetPolicy NetPolicy = ELOAClassComponentNetPolicy::AllMachines;
};

// =============================================================================
// 직업 데이터
// =============================================================================

/**
 * 직업 데이터 에셋.
 * 에디터에서 우클릭 → 기타 → 데이터 에셋 → LOACharacterClassData 로 생성한다. (예: DA_Class_Berserker)
 */
UCLASS(BlueprintType, Const)
class LOA_API ULOACharacterClassData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 에셋 매니저가 이 에셋을 찾을 때 쓰는 ID. 종류 이름 = "CharacterClass" */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	/** 저장 / 데이터 검증 시 빈 칸 같은 설정 실수를 잡아낸다. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// ---------------------------------------------------------------------
	// 기본 정보
	// ---------------------------------------------------------------------

	/** 직업 구분 태그 (예: Class.Berserker) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Class", meta = (Categories = "Class"))
	FGameplayTag ClassTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Class")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Class", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Class")
	TSoftObjectPtr<UTexture2D> Icon;

	// ---------------------------------------------------------------------
	// 외형 (캐릭터 몸에 적용, 모든 컴퓨터)
	// ---------------------------------------------------------------------

	/** 캐릭터 메시 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Avatar", meta = (AssetBundles = "Avatar"))
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** 애니메이션 BP 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Avatar", meta = (AssetBundles = "Avatar"))
	TSoftClassPtr<UAnimInstance> AnimClass;

	/**
	 * 캡슐 기준 메시 위치/회전.
	 * 메시마다 원점과 방향이 달라서(예: 마네킹은 발밑 원점, 옆을 보고 있음) 직업마다 맞춰야 한다.
	 * 코드에 숫자를 쓰지 않고 여기서 조정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Avatar")
	FTransform MeshRelativeTransform = FTransform::Identity;

	// ---------------------------------------------------------------------
	// 능력 (서버에서 부여)
	// ---------------------------------------------------------------------

	/**
	 * 항상 장착하는 AbilityDataSet 목록. 순서대로 부여된다.
	 * 예: [ DA_AbilityDataSet_Common, DA_AbilityDataSet_Berserker, DA_AbilityDataSet_Berserker_Identity ]
	 * 아이덴티티도 여기 넣는 꾸러미 하나다. (직업당 아이덴티티 하나. Z/X 스킬, 전용 게이지 스탯창, 패시브 효과)
	 * 플레이어 꾸러미(키 칸이 있는 것)만 고를 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|Abilities")
	TArray<TObjectPtr<const ULOAPlayerAbilityDataSet>> DefaultAbilityDataSets;

	// ---------------------------------------------------------------------
	// 직업 전용 컴포넌트 (선택)
	// ---------------------------------------------------------------------

	/** 직업만의 특수 기능이 필요할 때 캐릭터에 붙일 컴포넌트. 대부분의 직업은 비워 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOA|ClassComponent")
	TArray<FLOAClassComponentEntry> ClassComponents;
};
