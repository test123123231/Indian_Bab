#include "Widget/ReadyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "PlayerController/MainGamePlayerController.h"

void UReadyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Ready)
	{
		Button_Ready->OnClicked.RemoveAll(this);
		Button_Ready->OnClicked.AddDynamic(this, &UReadyWidget::OnReadyButtonClicked);
	}
}

void UReadyWidget::OnReadyButtonClicked()
{
	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetOwningPlayer());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] OwningPlayer is not MainGamePlayerController"));
		return;
	}

	PC->Server_RequestReady();

	if (Button_Ready)
	{
		Button_Ready->SetIsEnabled(false);
	}

	if (Text_ReadyState)
	{
		Text_ReadyState->SetText(FText::FromString(TEXT("준비 완료")));
	}

	UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] Ready button clicked"));
}