// LOASkillModule_Spawn.cpp

#include "AbilitySystem/SkillModules/LOASkillModule_Spawn.h"
#include "AbilitySystem/Abilities/LOAGameplayAbility.h"
#include "AbilitySystem/Abilities/Player/LOAPlayerAbility.h"
#include "Actors/Skill/LOASkillActor.h"
#include "Actors/Skill/LOAAreaActor.h"
#include "Animation/LOAAnimNotify_SendGameplayEvent.h"
#include "Core/LOALog.h"
#include "Library/LOACombatLibrary.h"
#include "Game/LOARaidSubsystem.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LOASkillModule_Spawn)

void ULOASkillModule_Spawn::SkillModuleInit()
{
	// 이전 발동의 기록은 지난 일이다. (모듈은 Ability와 함께 재사용된다)
	TimedAreaActors.Reset();

	ULOAGameplayAbility* Owner = GetOwnerAbility();
	if (!Owner)
	{
		return;
	}

	// BP 설정 누락 알림. (스킬은 그대로 진행된다. 몽타주는 나가고 소환만 없음)
	if (!ActorClass)
	{
		LOA_LOG(Owner->GetAvatarActorFromActorInfo(), Warning,
			TEXT("[%s] Spawn module: Actor Class is not set."), *GetNameSafe(Owner->GetClass()));
	}
}

void ULOASkillModule_Spawn::SkillModuleTriggered(const FGameplayEventData* EventData)
{
	// 조종하는 사람 컴퓨터와 서버에서 각각 호출된다. 소환은 SpawnSkillActor 안에서 서버만 한다.
	SpawnSkillActor();
}

void ULOASkillModule_Spawn::SkillModuleEnd(bool bWasCancelled)
{
	// 패턴이 취소됨 (보스 사망, 다른 몽타주에 끊김) → 몽타주 시각에 맞춰 터질 예정이던 장판을 지운다.
	// 정상 종료면 장판은 그대로 두고 혼자 판정한다. (몽타주가 판정 시각까지 갔다는 뜻)
	if (bWasCancelled && HasSkillModuleAuthority())
	{
		for (const TWeakObjectPtr<ALOAAreaActor>& AreaActor : TimedAreaActors)
		{
			if (AreaActor.IsValid() && !AreaActor->HasStartedHitting())
			{
				LOA_LOG(AreaActor.Get(), Log, TEXT("[Spawn] Ability cancelled before the hit. Remove %s."), *GetNameSafe(AreaActor.Get()));
				AreaActor->Destroy();
			}
		}
	}

	TimedAreaActors.Reset();
}

void ULOASkillModule_Spawn::SpawnSkillActor()
{
	// 액터 생성은 서버만. (클라가 만들면 그 클라에만 있는 가짜가 된다) 서버가 만든 액터는 복제되어 모두에게 보인다.
	if (!HasSkillModuleAuthority())
	{
		return;
	}

	ULOAGameplayAbility* Owner = GetOwnerAbility();
	AActor* Avatar = Owner ? Owner->GetAvatarActorFromActorInfo() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || !ActorClass)
	{
		return;
	}

	// ① 누가, 몇 레벨로, 차지 배율 (지금 값. 액터에 복사된다)
	const FLOASkillModuleContext Context = Owner->MakeSkillModuleContext();
	if (!Context.IsValid())
	{
		return;
	}

	// ② 어디에. 보통 1자리, All Enemies면 살아 있는 적 수만큼.
	TArray<FTransform> SpawnTransforms;
	if (GatherSpawnTransforms(Avatar, SpawnTransforms) == 0)
	{
		LOA_LOG(Avatar, Log, TEXT("[%s] Spawn module: no living enemies. Nothing spawned."), *GetNameSafe(Owner->GetClass()));
		return;
	}

	// ③ (선택) 첫 판정 시각 = 몽타주 노티파이. 범위 액터에만 의미가 있다. 자리가 여러 개여도 같은 시각이라 한 번만 계산.
	float SecondsUntilHit = 0.f;
	bool bUseHitTime = false;
	if (FirstHitEvent.IsValid())
	{
		if (!ActorClass->IsChildOf(ALOAAreaActor::StaticClass()))
		{
			LOA_LOG(Avatar, Warning, TEXT("[%s] Spawn module: First Hit Event is set but %s is not an area actor. Ignored."),
				*GetNameSafe(Owner->GetClass()), *GetNameSafe(ActorClass));
		}
		else if (!Owner->GetTimeUntilMontageEvent(FirstHitEvent, SecondsUntilHit))
		{
			LOA_LOG(Avatar, Warning, TEXT("[%s] Spawn module: no '%s' notify left in the montage. Using First Tick Delay."),
				*GetNameSafe(Owner->GetClass()), *FirstHitEvent.ToString());
		}
		else
		{
			bUseHitTime = true;
		}
	}

	for (const FTransform& SpawnTransform : SpawnTransforms)
	{
		// ④ 소환. Deferred = 만들기만 하고 BeginPlay 전에 멈춘다. → 값을 넘긴 뒤 FinishSpawning
		//    Owner     = 캐릭터 (누구 것인지. 네트워크 관련성도 따라간다)
		//    Instigator = 캐릭터 (엔진 쪽 "누가 일으켰나")
		//    AlwaysSpawn : 소환 위치가 다른 물체와 겹쳐도 옮기지 않고 그 자리에 만든다. (바로 앞의 적에게 겹쳐 나와도 맞아야 함)
		ALOASkillActor* SpawnedActor = World->SpawnActorDeferred<ALOASkillActor>(
			ActorClass, SpawnTransform, Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!SpawnedActor)
		{
			continue;
		}

		// ⑤ 맞을 때 쓸 값 넘기기 (복사). BeginPlay 전이라 액터의 BeginPlay에서 이미 쓸 수 있다.
		//    자리마다 따로 복사한다. (장판마다 자기 Context·Payloads를 가진다)
		SpawnedActor->InitSkillActor(Context, Payloads);

		// ⑥ 판정 시각 넘기기 + 패턴이 취소되면 지울 목록에
		if (bUseHitTime)
		{
			if (ALOAAreaActor* AreaActor = Cast<ALOAAreaActor>(SpawnedActor))
			{
				AreaActor->SetFirstHitDelay(SecondsUntilHit);
				TimedAreaActors.Add(AreaActor);
			}
		}

		SpawnedActor->FinishSpawning(SpawnTransform);

		LOA_LOG(Avatar, Log, TEXT("[Spawn] %s : %s at %s"),
			*GetNameSafe(Owner->GetClass()), *GetNameSafe(SpawnedActor), *SpawnTransform.GetLocation().ToCompactString());
	}
}

int32 ULOASkillModule_Spawn::GatherSpawnTransforms(const AActor* Avatar, TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset();

	if (SpawnLocation != ELOASpawnLocation::AllEnemies)
	{
		OutTransforms.Add(CalculateSpawnTransform(Avatar));
		return OutTransforms.Num();
	}

	// 살아 있는 적 플레이어 전원의 "지금" 위치. 방향은 다른 위치와 같이 캐릭터(보스)가 보는 방향.
	// 예) 플레이어 A (100, 0), B (0, 300), 보스는 +X를 봄 → A 위치, B 위치에 하나씩 (둘 다 +X 방향)
	const FRotator Facing(0.f, Avatar->GetActorRotation().Yaw, 0.f);
	TArray<AActor*> Enemies;
	ULOACombatLibrary::FindLivingEnemyPlayers(Avatar, Enemies);
	for (const AActor* Enemy : Enemies)
	{
		const FVector Location = Enemy->GetActorLocation() + Facing.RotateVector(Offset);
		OutTransforms.Add(FTransform(Facing, Location));
	}
	return OutTransforms.Num();
}

FTransform ULOASkillModule_Spawn::CalculateSpawnTransform(const AActor* Avatar) const
{
	const ULOAGameplayAbility* Owner = GetOwnerAbility();

	// 방향 = 캐릭터가 보는 방향, Yaw만. (위아래로 기울지 않게)
	const FRotator Facing(0.f, Avatar->GetActorRotation().Yaw, 0.f);

	// 기본 위치 = 캐릭터 위치. Cursor / Socket을 못 구하면 이 값을 그대로 쓴다.
	FVector BaseLocation = Avatar->GetActorLocation();

	switch (SpawnLocation)
	{
	case ELOASpawnLocation::Self:
		break;

	case ELOASpawnLocation::Cursor:
	{
		// 커서는 플레이어 Ability에만 있다. 플레이어 Ability가 아니면(나중에 보스 GA 등) Cast가 실패 → Self.
		// (보스용 "노리는 위치"가 필요해지면 Context.TargetLocation으로 바꾼다. LOASkillModuleDataInfos.h [나중에 늘어날 칸])
		const ULOAPlayerAbility* PlayerAbility = Cast<ULOAPlayerAbility>(Owner);
		FVector CursorLocation;
		if (PlayerAbility && PlayerAbility->GetCursorLocation(CursorLocation))
		{
			BaseLocation = CursorLocation;

			// 최대 거리: 캐릭터에서 커서까지 바닥 거리(높이 무시)가 MaxRange보다 멀면, 커서 방향으로 MaxRange 지점까지 당긴다.
			// 높이(Z)는 커서 위치(바닥) 그대로. 0이면 제한 없음.
			const FVector AvatarLocation = Avatar->GetActorLocation();
			const FVector ToCursor2D(CursorLocation.X - AvatarLocation.X, CursorLocation.Y - AvatarLocation.Y, 0.f);
			if (MaxRange > 0.f && ToCursor2D.SizeSquared() > FMath::Square(MaxRange))
			{
				const FVector Clamped2D = ToCursor2D.GetSafeNormal() * MaxRange;
				BaseLocation = FVector(AvatarLocation.X + Clamped2D.X, AvatarLocation.Y + Clamped2D.Y, CursorLocation.Z);
			}
		}
		else
		{
			LOA_LOG(Avatar, Warning, TEXT("[%s] Spawn module: no cursor location (not a player ability, or cursor not found). Spawning at Self."),
				*GetNameSafe(Owner ? Owner->GetClass() : nullptr));
		}
		break;
	}

	case ELOASpawnLocation::Socket:
	{
		const ACharacter* Character = Cast<ACharacter>(Avatar);
		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		if (Mesh && Mesh->DoesSocketExist(SocketName))
		{
			BaseLocation = Mesh->GetSocketLocation(SocketName);
		}
		else
		{
			LOA_LOG(Avatar, Warning, TEXT("[%s] Spawn module: socket '%s' not found on the character mesh. Spawning at Self."),
				*GetNameSafe(Owner ? Owner->GetClass() : nullptr), *SocketName.ToString());
		}
		break;
	}

	case ELOASpawnLocation::AllEnemies:
		// 여러 자리라 GatherSpawnTransforms가 따로 처리한다. 여기 오면 Self.
		break;

	case ELOASpawnLocation::Location:
	{
		// 레벨의 위치 표시 (예: Location.Arena.Center). 방향은 다른 위치와 같이 캐릭터가 보는 방향.
		const ULOARaidSubsystem* Raid = ULOARaidSubsystem::Get(Avatar);
		FVector MarkerLocation;
		if (Raid && Raid->FindLocation(LocationTag, MarkerLocation))
		{
			BaseLocation = MarkerLocation;
		}
		else
		{
			LOA_LOG(Avatar, Warning, TEXT("[%s] Spawn module: no location marker '%s' in this level. Spawning at Self."),
				*GetNameSafe(Owner ? Owner->GetClass() : nullptr), *LocationTag.ToString());
		}
		break;
	}

	default:
		break;
	}

	// Offset을 보는 방향 기준으로 돌려서 더한다. (X 앞, Y 오른쪽, Z 위)
	const FVector Location = BaseLocation + Facing.RotateVector(Offset);
	return FTransform(Facing, Location);
}

// =============================================================================
// 저장 시 검사 (에디터 전용)
// =============================================================================

#if WITH_EDITOR
EDataValidationResult ULOASkillModule_Spawn::ValidateSkillModule(
	FDataValidationContext& Context, const UAnimMontage* OwnerMontage, const FString& Prefix) const
{
	// 부모: Trigger / 노티파이 검사
	EDataValidationResult Result = Super::ValidateSkillModule(Context, OwnerMontage, Prefix);

	const FString Name = FString::Printf(TEXT("%s Spawn"), *Prefix);

	// ① 무엇을
	if (!ActorClass)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s : Actor Class is not set."), *Name)));
		Result = EDataValidationResult::Invalid;
	}

	// ② 어디에. 소켓이 메시에 실제로 있는지는 여기서 모른다. (어느 캐릭터가 쓸지 GA는 모름) → 실행 중 경고로 확인
	if (SpawnLocation == ELOASpawnLocation::Socket && SocketName.IsNone())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Spawn Location is Socket but Socket Name is empty."), *Name)));
		Result = EDataValidationResult::Invalid;
	}

	if (SpawnLocation == ELOASpawnLocation::Location && !LocationTag.IsValid())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s : Spawn Location is Location but Location Tag is empty."), *Name)));
		Result = EDataValidationResult::Invalid;
	}

	// ③ 맞히면. 비어 있어도 오류는 아니다. (겉모습만 있는 액터)
	if (Payloads.IsEmpty())
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s : Payloads is empty. The spawned actor will do nothing when it hits."), *Name)));
	}

	// ④ Payload 칸마다. Apply와 같은 규칙 (부모에 한 곳)
	Result = CombineDataValidationResults(Result, ValidateSkillModulePayloads(Context, Payloads, Name));

	// ⑤ First Hit Event: 범위 액터만 / 몽타주에 그 노티파이가 있어야 함
	if (FirstHitEvent.IsValid())
	{
		if (ActorClass && !ActorClass->IsChildOf(ALOAAreaActor::StaticClass()))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("%s : First Hit Event is set but Actor Class %s is not an area actor (ALOAAreaActor)."), *Name, *GetNameSafe(ActorClass))));
			Result = EDataValidationResult::Invalid;
		}

		float EventTime = 0.f;
		if (OwnerMontage && !ULOAAnimNotify_SendGameplayEvent::FindEventTimeInMontage(OwnerMontage, FirstHitEvent, 0.f, EventTime))
		{
			// 경고: 몽타주를 나중에 고칠 수도 있으니 저장은 막지 않는다. (실행 중에는 First Tick Delay로 대신함)
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s : Montage %s has no '%s' notify (LOA Send Gameplay Event). First Tick Delay will be used."),
				*Name, *GetNameSafe(OwnerMontage), *FirstHitEvent.ToString())));
		}
	}

	return Result;
}
#endif
