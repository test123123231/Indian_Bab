#include "Widget/PlayerNameWidget.h"
#include "Components/TextBlock.h"

void UPlayerNameWidget::SetPlayerName(const FString& Name)
{
    if (PlayerNameText)
    {
        PlayerNameText->SetText(FText::FromString(Name));
    }
}

void UPlayerNameWidget::SetCardText(const FString& Name)
{
    if (CardText)
    {
        CardText->SetText(FText::FromString(Name));
    }
}
