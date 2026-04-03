#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerNameWidget.generated.h"

class UTextBlock;

UCLASS()
class INDIAN_BAB_API UPlayerNameWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPlayerName(const FString& Name);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> PlayerNameText;
};