// LOALocalPlayer.h
// [배관] 이 컴퓨터 앞에 앉은 사람 한 명. 로비에서 고른 것을 기억했다가 접속할 때 서버에 보낸다. (2026-11-14 apply113)
//
// ─────────────────────────────────────────────────────────────────────────────
// [LocalPlayer란]
//   엔진이 "이 컴퓨터의 플레이어 한 명"마다 만드는 객체. 게임을 끌 때까지 남는다.
//   PlayerController는 맵을 옮길 때마다 새로 만들어지지만 LocalPlayer는 그대로다
//   → 로비 맵에서 고른 직업이 레이드 맵으로 가도 남아 있다.
//   데디케이티드 서버에는 없다 (앉은 사람이 없음). 그래서 받는 쪽(GameMode)은 이 클래스를 모른다.
//
// [하는 일]
//   ① 선택 기억 : Selection (보낼 것) + SelectedClassData (로비 화면이 쓰는 불러온 에셋). 둘은 SetSelectedClass에서만 같이 바뀐다
//   ② 바뀜 알림 : OnSelectedClassChanged → 로비 화면(스킬 목록·카드), 미리보기 액터가 듣는다. 서로는 모른다
//   ③ 서버에 보내기 : 엔진이 접속할 때 GetGameLoginOptions()를 부른다 → "Class=DA_Class_Berserker"
//                     이름은 엔진이 GetNickname()으로 ?Name=에 붙인다 (apply131. 정했으면 그 이름, 안 정했으면 엔진 기본)
//
// [하지 않는 일]
//   저장(캐릭터 목록·레벨)은 여기가 아니다. 게임을 끄면 사라진다. 저장은 레벨업을 만들 때 SaveGame으로 따로.
//
// [네트워크] 각자 (이 컴퓨터만). 서버로는 로그인 주소로 한 번 (FLOACharacterSelection)
//
// [설정] DefaultEngine.ini [/Script/Engine.Engine] LocalPlayerClassName=/Script/LOA.LOALocalPlayer

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "Data/LOACharacterSelection.h"
#include "LOALocalPlayer.generated.h"

class ULOACharacterClassData;

/** 고른 직업이 바뀜. 인자 = 새 직업 데이터 (불러온 상태) */
DECLARE_MULTICAST_DELEGATE_OneParam(FLOASelectedClassChangedSignature, const ULOACharacterClassData* /*ClassData*/);

UCLASS()
class LOA_API ULOALocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	/**
	 * 직업을 고른다. 같은 직업이면 아무것도 안 한다. 바뀌면 OnSelectedClassChanged.
	 * 로비 카드 클릭이 부른다.
	 */
	void SetSelectedClass(const ULOACharacterClassData* ClassData);

	/** 고른 직업 데이터. 아직 안 골랐으면 nullptr */
	const ULOACharacterClassData* GetSelectedClassData() const { return SelectedClassData; }

	/** 서버에 보낼 선택 */
	const FLOACharacterSelection& GetSelection() const { return Selection; }

	/**
	 * 이름을 정한다 (규칙에 맞게 고쳐서: FLOACharacterSelection::MakeValidPlayerName).
	 * 고친 결과가 비면 false (이름 그대로). 로비 이름 칸(apply132)·치트 LOA.Party.Name이 부른다
	 */
	bool SetPlayerName(const FString& InName);

	/** 고른 직업이 바뀜 (로비 화면, 미리보기 액터가 듣는다) */
	FLOASelectedClassChangedSignature OnSelectedClassChanged;

	//~ Begin ULocalPlayer Interface
	/** 접속할 때 주소에 붙일 글자. 엔진이 호스트·원격 접속 모두에서 부른다 */
	virtual FString GetGameLoginOptions() const override;
	/** 접속 주소의 ?Name=. 정한 이름이 있으면 그것, 없으면 엔진 기본 (온라인 기능의 이름) */
	virtual FString GetNickname() const override;
	//~ End ULocalPlayer Interface

private:
	/** 서버에 보낼 것 */
	FLOACharacterSelection Selection;

	/** 고른 직업 데이터 (불러온 에셋). 로비 화면이 이름·스킬·외형을 읽는다 */
	UPROPERTY(Transient)
	TObjectPtr<const ULOACharacterClassData> SelectedClassData;
};
