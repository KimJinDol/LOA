// LOAGE_InfiniteTags.h
// [배관] 공용 "떼어 낼 때까지 태그 부여" GE. 값을 바꾸지 않고 태그만 붙인다. 시간으로 사라지지 않는다.
//
//   규칙(이 클래스) : Infinite / Modifier 없음
//   값(쓰는 쪽)     : 태그 → Spec의 DynamicGrantedTags에 추가 (붙어 있는 동안 ASC가 이 태그를 가짐)
//
// 쓰는 곳: ULOAGA_Death (State.Body.Dead)
//   나중에: "풀릴 때까지" 유지되는 상태 (예: 기믹 중 무적, 그로기처럼 끝이 시간이 아닌 것)
//
// [ULOAGE_TimedTags와의 차이 = 수명]
//   TimedTags    : 시간이 지나면 엔진이 지운다. (몽타주 구간. 끝 신호를 놓쳐도 안전)
//   InfiniteTags : 누군가 지워야 한다. → 쓰는 쪽이 "누가 지우는지"를 반드시 정해 둔다.
//                  사망: 부활이 지운다 (아직 없음. 보스는 부활이 없으므로 영구)
//
// [네트워크]
// - 서버에서만 적용한다. (예측 키 없이) → 붙은 태그는 ASC 복제로 모든 컴퓨터에 간다.
//   Minimal/Mixed 모드에서도 GE가 준 태그는 모든 클라이언트에 복제된다. (GE 상세 정보는 안 감)
// - 같은 GE 클래스를 다른 태그로 여러 번 적용해도 된다. Spec마다 태그가 따로다. (TimedTags와 같음)
//
// C++ 클래스로 만든 이유는 ULOAGE_TimedTags와 같다. (조정할 숫자가 없는 배관용 GE)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LOAGE_InfiniteTags.generated.h"

UCLASS()
class LOA_API ULOAGE_InfiniteTags : public UGameplayEffect
{
	GENERATED_BODY()

public:
	ULOAGE_InfiniteTags();
};
