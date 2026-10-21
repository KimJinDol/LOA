// LOACursorLibrary.h
// [배관] 마우스 커서 → 월드 위치 함수 모음. 커서를 쓰는 모든 곳(스킬 조준, 클릭 이동)이 같은 규칙을 쓴다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [커서 위치 구하는 규칙] GetCursorHitResult
//
//   ① 커서 아래 트레이스 (TraceChannel). 땅/보스 등 무언가에 맞으면 그 위치
//   ② ①이 실패하면(맵 밖, 하늘, 낭떠러지) 커서 광선과 "조종 중인 캐릭터 발밑 높이의 가상 바닥 평면"이 만나는 점
//
//        카메라
//          ╲  커서 광선
//           ╲
//   ─────────●───────────  ← 캐릭터 발밑 높이의 수평 평면 (무한히 넓음)
//        캐릭터
//
//   평면은 무한하므로 화면 안을 가리키는 한 ②는 거의 항상 성공한다.
//   → 로아처럼 맵 끝 바깥을 가리켜도 그쪽을 바라보거나 그쪽으로 걸어간다.
//
//   ②로 얻은 결과는 HitResult.bBlockingHit = false 로 표시한다. (실제로 맞은 물체가 없다는 뜻)
//   방향만 쓰는 곳(회전, 회피)은 구분할 필요 없다. 실제 바닥이 필요한 곳(장판 설치 등)만 확인하면 된다.
//
// ─────────────────────────────────────────────────────────────────────────────
// [네트워크]
// 커서는 조종하는 사람 컴퓨터에만 있다. 이 함수들도 그 컴퓨터에서만 의미가 있다.
// 서버로 보내야 하면 ULOAAbilityTask_WaitCursorTarget처럼 TargetData로 보낸다.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "LOACursorLibrary.generated.h"

class APlayerController;
class AActor;

UCLASS()
class LOA_API ULOACursorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 커서가 가리키는 월드 위치. ① 트레이스 → 실패하면 ② 가상 바닥 평면. (위 설명 참고)
	 * @param PC            커서를 가진 PlayerController (조종하는 사람 컴퓨터의 것)
	 * @param TraceChannel  ① 트레이스 채널
	 * @param OutHit        결과. ②로 얻었으면 Location/ImpactPoint만 채워지고 bBlockingHit = false
	 * @return ①② 모두 실패하면 false (카메라가 위를 보는 등 거의 없는 경우)
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Cursor")
	static bool GetCursorHitResult(const APlayerController* PC, ECollisionChannel TraceChannel, FHitResult& OutHit);

	/**
	 * 커서 광선과 높이 PlaneZ인 수평 평면이 만나는 점. (② 단계만 따로 쓰고 싶을 때)
	 * 광선이 아래를 향하지 않으면(평면과 안 만남) false.
	 */
	UFUNCTION(BlueprintCallable, Category = "LOA|Cursor")
	static bool GetCursorLocationOnPlane(const APlayerController* PC, float PlaneZ, FVector& OutLocation);

	/**
	 * 액터의 발밑 높이. 캐릭터면 캡슐 중심에서 캡슐 절반 높이만큼 내린 값, 아니면 액터 위치 높이.
	 * 가상 바닥 평면의 높이로 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "LOA|Cursor")
	static float GetFootZ(const AActor* Actor);
};
