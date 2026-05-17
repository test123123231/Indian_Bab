#include "Actor/AnimNotify_GrabRevolver.h"
#include "Character/LobbyCharacter.h"
#include "Character/LobbyVRCharacter.h"

void UAnimNotify_GrabRevolver::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	ALobbyCharacter* Character = Cast<ALobbyCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		Character->AttachRevolverToSocket();
		return;
	}

	ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(MeshComp->GetOwner());
	if (VRCharacter)
	{
		VRCharacter->AttachRevolverToSocket();
	}
}

FString UAnimNotify_GrabRevolver::GetNotifyName_Implementation() const
{
	return TEXT("GrabRevolver");
}
