#include "Actor/AnimNotify_PutBackRevolver.h"
#include "Character/LobbyCharacter.h"

void UAnimNotify_PutBackRevolver::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	ALobbyCharacter* Character = Cast<ALobbyCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		Character->ReturnRevolverToDesk();
	}
}

FString UAnimNotify_PutBackRevolver::GetNotifyName_Implementation() const
{
	return TEXT("PutBackRevolver");
}
