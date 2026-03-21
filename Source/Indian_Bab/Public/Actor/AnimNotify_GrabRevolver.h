#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_GrabRevolver.generated.h"

/**
 * AM_AimMyself 몽타주에서 손이 리볼버에 닿는 프레임에 배치하는 AnimNotify.
 * 발동 시 LobbyCharacter::AttachRevolverToSocket() 을 호출하여
 * 책상 위 리볼버를 캐릭터의 Revolver 소켓에 부착한다.
 */
UCLASS()
class INDIAN_BAB_API UAnimNotify_GrabRevolver : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
