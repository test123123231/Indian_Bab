#include "Actor/AnimNotify_GrabRevolver.h"
#include "Character/LobbyCharacter.h"

void UAnimNotify_GrabRevolver::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	ALobbyCharacter* Character = Cast<ALobbyCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		Character->AttachRevolverToSocket();
	}
}

FString UAnimNotify_GrabRevolver::GetNotifyName_Implementation() const
{
	return TEXT("GrabRevolver");
}
