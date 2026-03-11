#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "CardManager.generated.h"

// 데이터 테이블에서 사용할 카드의 상세 정보 구조체
USTRUCT(BlueprintType)
struct FCardData : public FTableRowBase // DataTable 행으로 쓰기 위해 상속 필수
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Info")
    int32 Value; // 카드 숫자 (1~13)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Info")
    FString Suit; // 카드 무늬 (Spade, Heart 등)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Visual")
    class UTexture2D* CardTexture; // 나중에 연결할 실제 이미지 에셋 포인터

    // 기본 생성자
    FCardData() : Value(1), Suit(TEXT("Spade")), CardTexture(nullptr) {}
};

UCLASS()
class INDIAN_BAB_API ACardManager : public AActor
{
    GENERATED_BODY()

public:
    ACardManager();

    // 에디터에서 생성한 데이터 테이블을 할당할 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Logic")
    UDataTable* CardDataTable;

    // 현재 게임에서 사용할 실시간 덱 (데이터 테이블의 복사본)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card Logic")
    TArray<FCardData> CurrentDeck;

    // 게임 시작 시 덱을 초기화하는 함수
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    void InitializeDeck();

    // 카드 한 장을 뽑고 덱에서 제거하는 함수
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    FCardData DrawCard();

protected:
    virtual void BeginPlay() override;
};