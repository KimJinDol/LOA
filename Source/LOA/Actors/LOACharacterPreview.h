// LOACharacterPreview.h
// [연출] 로비 3D 미리보기. 레벨에 하나 놓으면 고른 직업의 메시가 대기 동작을 하며 선다. (2026-11-14 apply113)
//
// ─────────────────────────────────────────────────────────────────────────────
// [흐름]
//   BeginPlay → 이 컴퓨터의 ULOALocalPlayer를 찾아 OnSelectedClassChanged를 듣는다 (이미 골랐으면 바로 보여 줌)
//   직업이 바뀜 → ShowClass(직업 데이터)
//     ① 메시 + LobbyAnimation을 비동기로 불러온다 (Soft 참조. 화면이 멈칫하지 않게)
//     ② 다 오면, 그사이 또 바뀌지 않았을 때만 씌운다 (빠르게 카드를 여러 번 눌러도 마지막 것만)
//        메시 / MeshRelativeTransform(게임 캐릭터와 같은 값) / LobbyAnimation 반복 재생
//   로비 화면(위젯)은 이 액터를 모르고, 이 액터도 위젯을 모른다. 둘 다 LocalPlayer 알림만 듣는다
//
// [왜 LocalPlayer를 직접 찾나]
//   로비 맵에 하나 놓는 연출 액터라 넘겨줄 주인이 없다. 이 컴퓨터의 첫 번째 플레이어 = 로비를 보는 사람
//
// [네트워크] 없음. 로비는 혼자 쓰는 맵 (복제 안 함)
//
// [BP] BP_CharacterPreview (부모 = 이 클래스). 로비 맵에 놓는다
//   자리: 바닥에서 게임 캐릭터 캡슐 반 높이만큼 위 (MeshRelativeTransform이 캡슐 가운데 기준이라서)
//   카메라: 레벨에 CameraActor를 놓고 Auto Activate for Player = Player 0

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LOACharacterPreview.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class ULOACharacterClassData;
class ULOALocalPlayer;
struct FStreamableHandle;

UCLASS()
class LOA_API ALOACharacterPreview : public AActor
{
	GENERATED_BODY()

public:
	ALOACharacterPreview();

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

private:
	/** 고른 직업이 바뀜 (ULOALocalPlayer 알림) */
	void HandleSelectedClassChanged(const ULOACharacterClassData* ClassData);

	/** 이 직업의 외형을 불러와 보여 준다 */
	void ShowClass(const ULOACharacterClassData* ClassData);

	/** 불러오기 끝 → 그 직업이 아직 지금 직업이면 씌운다 */
	void ApplyLoadedClass(const ULOACharacterClassData* ClassData);

	/** 기준점 (캡슐 가운데 자리) */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Preview")
	TObjectPtr<USceneComponent> PreviewRoot;

	/** 직업 메시를 씌울 곳 */
	UPROPERTY(VisibleAnywhere, Category = "LOA|Preview")
	TObjectPtr<USkeletalMeshComponent> PreviewMesh;

	/** 지금 보여 주려는 직업 (불러오는 중이어도) */
	UPROPERTY(Transient)
	TObjectPtr<const ULOACharacterClassData> ShownClassData;

	/** 듣고 있는 LocalPlayer (EndPlay에서 듣기를 끊는다) */
	TWeakObjectPtr<ULOALocalPlayer> ListenedLocalPlayer;

	/** 불러오는 중인 핸들. 다른 직업을 고르면 취소한다 */
	TSharedPtr<FStreamableHandle> LoadHandle;
};
