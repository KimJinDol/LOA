// LOASkillModule_Spawn.h
// [배관] Spawn 모듈. "[언제] 신호가 오면, [어디에], [무엇을] 소환한다. 소환된 액터가 맞히면 [Payloads]를 준다."
//   언제     : 부모(ULOASkillModule)의 Trigger 칸
//   어디에   : Spawn Location (Self / Cursor / Socket / All Enemies / Location) + Offset
//   무엇을   : Actor Class (ALOASkillActor 자식 BP. 예: BP_Projectile_Arrow)
//   맞히면   : Payloads → 소환할 때 액터에 복사해서 넘긴다
//
// ─────────────────────────────────────────────────────────────────────────────
// [디테일 패널]
//
//   ▼ [0]  [ Spawn ▼ ]
//        Trigger            [ On Event ▼ ]          ← 언제 (부모 칸)
//        Trigger Event Tag  [ Event.Montage.Hit ]
//        Actor Class        [ BP_Projectile_Arrow ]  ← 무엇을
//        Spawn Location     [ Socket ▼ ]             ← 어디에
//        Socket Name        [ hand_r ]               ← Socket일 때만 보임
//        Max Range          1200                     ← Cursor일 때만 보임. 0 = 제한 없음
//        Offset             X 50 / Y 0 / Z 0         ← 캐릭터가 보는 방향 기준 (X 앞, Y 오른쪽, Z 위)
//        First Hit Event    [ Event.Montage.Hit ]    ← (범위 액터만, 선택) 첫 판정 시각 = 몽타주의 이 노티파이
//        ▼ Payloads   [ + ]                          ← 맞히면 (Apply와 같은 칸)
//           ▼ [0]
//             Effect   [ GE_Damage_Basic ]
//             Values   [0] SetByCaller.DamageRate = 1.8
//
//   데미지를 액터 BP가 아니라 여기 두는 이유: 같은 화살 BP를 여러 스킬이 다른 배율로 쓴다.
//   "스킬 수치는 GA BP에서" (Apply 모듈과 같은 규칙). 액터 BP에는 속도/크기/관통 같은 동작만 둔다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [어디에 - 위치와 방향]
//   방향은 셋 다 "캐릭터가 지금 보는 방향"(Yaw만). 발동할 때 이미 커서 쪽으로 돌았다.
//     Self   : 캐릭터 위치(캡슐 중심) + Offset
//     Cursor : 이번 발동의 커서 위치(바닥) + Offset.   커서 위치가 없으면(bFaceCursor 꺼짐 등) Self로 + 경고
//              주인을 ULOAPlayerAbility로 Cast해서 GetCursorLocation을 부른다. 플레이어 Ability가 아니면 Self로 + 경고
//              Max Range(최대 거리): 커서가 이보다 멀면 커서 방향으로 Max Range 지점까지 당긴다. (로아의 "최대 12m" = 1200)
//     Socket : 캐릭터 메시의 소켓 위치 + Offset.        소켓이 없으면 Self로 + 경고
//     All Enemies : 살아 있는 적 플레이어 "전원"의 지금 위치에 하나씩 + Offset. (보스 패턴: 전원 발밑 장판)
//              예) 3인 중 1명 사망 → 2개 소환. 아무도 없으면 소환 안 함 + 로그.
//              "살아 있는 적" = ULOACombatLibrary::FindLivingEnemyPlayers (보스 AI의 대상 고르기와 같은 규칙)
//              위치는 소환하는 순간의 위치. 그 뒤로 따라가지 않는다. (노티파이를 4개 찍으면 4번, 매번 그 순간의 발밑)
//     Location : 레벨의 위치 표시(ALOALocationMarker) 중 Location Tag가 같은 것 + Offset. 못 찾으면 Self로 + 경고
//              예) 패턴 D의 쿵: Location.Arena.Center → 보스가 어디 있든 맵 중앙에 큰 원
//   Offset은 위치를 정한 뒤 "보는 방향 기준"으로 더한다. (오른쪽을 보고 있으면 X 50 = 오른쪽으로 50)
//
//   소켓 방향을 쓰지 않는 이유: 애니메이션 중 손 방향은 프레임마다 흔들린다. 투사체는 커서 쪽으로 곧게 나가야 한다.
//   데디케이티드 서버의 소켓: 캐릭터 메시는 기본으로 서버에서도 뼈를 계산한다(ACharacter 기본값
//   AlwaysTickPoseAndRefreshBones) → 서버의 소켓 위치도 클라와 같다.
//
//   Cursor의 한계: 발동할 때 받은 커서 위치 하나뿐이다. 콤보의 "타마다 커서 방향"은 회전만 하고 이 위치를 바꾸지 않는다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//
//   SkillModuleInit      : 설정 누락 경고만 (Actor Class 없음)
//   SkillModuleTriggered : (부모가 Trigger 설정대로 부름) → SpawnSkillActor
//
//   SpawnSkillActor [서버만]
//      위치/방향 계산 (CalculateSpawnTransform)
//      SpawnActorDeferred (BeginPlay 전에 멈춤)
//      → InitSkillActor(주인->MakeSkillModuleContext(), Payloads)   ← 복사
//      → FinishSpawning → 액터 BeginPlay → 혼자 동작 (GA가 끝나도 계속)
//
//   다단: 몽타주에 같은 태그 노티파이를 여러 개 찍으면 찍은 만큼 소환한다. (화살 3연발)
//
// ─────────────────────────────────────────────────────────────────────────────
// [First Hit Event - 보스 장판의 판정 시각] (범위 액터 ALOAAreaActor를 소환할 때만)
//   비워 두면 : 범위 액터 BP의 First Tick Delay(숫자)로 첫 판정 (플레이어 스킬 낫 등, 지금까지와 같음)
//   고르면    : 소환하는 순간 "몽타주에서 이 노티파이까지 남은 초"를 계산해 범위 액터에 넘긴다 (SetFirstHitDelay)
//               → 범위 액터는 그 시각에 판정하고, 예고 데칼도 그 시각에 꽉 찬다.
//   예) 내려찍기: Trigger = On Skill Start, First Hit Event = Event.Montage.Hit (도끼가 닿는 프레임, 몽타주 1.2초)
//       → 시작하자마자 보스 앞에 장판 소환, 1.2초 뒤 (재생 속도 0.8이면 1.5초 뒤) 판정
//   몽타주 속도를 바꾸거나 애니를 갈아끼워도 노티파이만 옮기면 판정이 따라간다. (숫자를 다시 맞추지 않음)
//
//   패턴이 취소되면 (보스 사망, 다른 몽타주에 끊김) 이 방식으로 소환한 장판 중 아직 안 터진 것은 지운다.
//   → 몽타주가 끊겼는데 도끼 없이 장판만 터지는 일이 없다. (First Tick Delay 방식은 지금까지처럼 그대로 둔다)
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
//   조종하는 사람 컴퓨터 : 신호를 받지만 아무것도 안 함. 서버가 만든 액터가 복제되어 보인다. (핑만큼 늦게)
//   서버                 : 신호를 받으면 소환. 판정/데미지는 소환된 액터가 서버에서 한다.
//                          First Hit Event 시각 계산과 취소 시 정리도 서버만.
//   예측 소환(클라가 가짜를 먼저 띄움)은 하지 않는다. 지연이 실제로 거슬리면 그때 추가한다.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SkillModules/LOASkillModule.h"
#include "AbilitySystem/SkillModules/LOASkillModuleDataInfos.h"   // FLOAEffectPayload를 값으로 가지므로 include
#include "LOASkillModule_Spawn.generated.h"

class ALOASkillActor;
class ALOAAreaActor;

/** 어디에 소환하나 */
UENUM(BlueprintType)
enum class ELOASpawnLocation : uint8
{
	/** 캐릭터 위치 (캡슐 중심) */
	Self	UMETA(DisplayName = "Self"),

	/** 이번 발동의 커서 위치 (바닥). 플레이어 Ability만. 없으면 Self */
	Cursor	UMETA(DisplayName = "Cursor"),

	/** 캐릭터 메시의 소켓 (손, 무기 끝). 없으면 Self */
	Socket	UMETA(DisplayName = "Socket"),

	/** 살아 있는 적 플레이어 전원의 지금 위치에 하나씩 (보스 패턴: 전원 발밑 장판). 아무도 없으면 소환 안 함 */
	AllEnemies	UMETA(DisplayName = "All Enemies"),

	/** 레벨의 위치 표시(ALOALocationMarker) 중 Location Tag가 같은 것. 없으면 Self */
	Location	UMETA(DisplayName = "Location"),
};

UCLASS(meta = (DisplayName = "Spawn"))
class LOA_API ULOASkillModule_Spawn : public ULOASkillModule
{
	GENERATED_BODY()

protected:
	//~ Begin ULOASkillModule Interface
	virtual void SkillModuleInit() override;
	virtual void SkillModuleTriggered(const FGameplayEventData* EventData) override;
	virtual void SkillModuleEnd(bool bWasCancelled) override;
	//~ End ULOASkillModule Interface

public:
	/** 대상에게 줄 GE들 (툴팁이 읽음, 2026-11-14) */
	virtual const TArray<FLOAEffectPayload>* GetPayloads() const override { return &Payloads; }

#if WITH_EDITOR
	/**
	 * [에디터 전용] 저장 시 검사. (부모의 Trigger 검사 + 아래)
	 *   오류 : Actor Class 없음 / Spawn Location = Socket인데 Socket Name 없음
	 *   경고 : Payloads 비어 있음 (소환은 되지만 맞혀도 아무 일 없음. 겉모습만 있는 액터면 정상)
	 *   Payload 칸 검사 : Apply와 같은 규칙 (ValidateSkillModulePayloads)
	 *   First Hit Event : Actor Class가 범위 액터가 아니면 오류 / 몽타주에 그 노티파이가 없으면 경고
	 */
	virtual EDataValidationResult ValidateSkillModule(
		class FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const override;
#endif

protected:

	// ---------- 데이터 (Ability BP의 디테일 패널에서) ----------

	/** 무엇을. ALOASkillActor의 자식 BP (예: BP_Projectile_Arrow) */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn")
	TSubclassOf<ALOASkillActor> ActorClass;

	/** 어디에. 방향은 항상 캐릭터가 보는 방향 */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn")
	ELOASpawnLocation SpawnLocation = ELOASpawnLocation::Self;

	/** 캐릭터 메시의 소켓 이름 (Spawn Location = Socket일 때) */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn",
		meta = (EditCondition = "SpawnLocation == ELOASpawnLocation::Socket", EditConditionHides))
	FName SocketName;

	/**
	 * 최대 거리(cm). 캐릭터에서 커서까지 바닥 거리(높이 무시)가 이보다 멀면 커서 방향으로 이 거리 지점에 소환한다.
	 * 0 = 제한 없음. (Spawn Location = Cursor일 때)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn",
		meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "SpawnLocation == ELOASpawnLocation::Cursor", EditConditionHides))
	float MaxRange = 0.f;

	/** 소환할 곳의 이름 (Spawn Location = Location일 때). 레벨에 같은 태그의 ALOALocationMarker를 놓는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn",
		meta = (Categories = "Location", EditCondition = "SpawnLocation == ELOASpawnLocation::Location", EditConditionHides))
	FGameplayTag LocationTag;

	/** 위치를 정한 뒤 더할 거리(cm). 캐릭터가 보는 방향 기준: X 앞, Y 오른쪽, Z 위 */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn")
	FVector Offset = FVector::ZeroVector;

	/**
	 * (범위 액터만, 선택) 첫 판정 시각 = 이 Ability 몽타주의 이 노티파이가 울리는 순간.
	 * 비우면 범위 액터 BP의 First Tick Delay. 보스 장판은 여기에 "도끼가 닿는 프레임" 노티파이 태그를 고른다.
	 * 패턴이 취소되면 이 방식으로 소환한 장판 중 아직 안 터진 것은 지운다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn", meta = (Categories = "Event.Montage"))
	FGameplayTag FirstHitEvent;

	/**
	 * 소환된 액터가 대상을 맞히면 줄 GE들. 소환할 때 액터에 복사해서 넘긴다. (Apply 모듈의 Payloads와 같은 칸)
	 * 비어 있으면 맞혀도 아무 일 없음 (겉모습만 있는 액터).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn")
	TArray<FLOAEffectPayload> Payloads;

private:
	/** [서버] 위치 계산 → 위치마다 소환 → Context/Payloads 넘기기 (+ First Hit Event면 판정 시각 넘기기) */
	void SpawnSkillActor();

	/**
	 * [서버] 이번에 소환할 자리들. 보통 1개, All Enemies면 살아 있는 적 수만큼.
	 * @return 자리 수 (0 = 소환 안 함)
	 */
	int32 GatherSpawnTransforms(const AActor* Avatar, TArray<FTransform>& OutTransforms) const;

	/**
	 * [서버] First Hit Event로 판정 시각을 넘긴 범위 액터. 이번 발동에서 소환한 것만.
	 * 패턴이 취소되면 아직 안 터진 것을 지운다. (SkillModuleEnd)
	 */
	TArray<TWeakObjectPtr<ALOAAreaActor>> TimedAreaActors;

	/**
	 * 소환 위치와 방향 (한 자리). Spawn Location대로 위치를 정하고 Offset을 보는 방향 기준으로 더한다.
	 * Cursor / Socket을 못 구하면 Self로 바꾸고 경고 로그. (All Enemies는 GatherSpawnTransforms가 따로 처리)
	 */
	FTransform CalculateSpawnTransform(const AActor* Avatar) const;
};
