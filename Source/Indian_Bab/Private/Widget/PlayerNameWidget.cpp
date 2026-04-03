#include "Widget/PlayerNameWidget.h"
#include "Components/TextBlock.h"

void UPlayerNameWidget::SetPlayerName(const FString& Name)
{
    if (PlayerNameText)
    {
        PlayerNameText->SetText(FText::FromString(Name));
    }
}

