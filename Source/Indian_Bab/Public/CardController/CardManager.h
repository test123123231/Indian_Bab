#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "CardManager.generated.h"

// 데이터 테이블의 각 행(Row)으로 사용할 카드 정보 구조체
USTRUCT(BlueprintType)
struct FCardData : public FTableRowBase
{
    GENERATED_BODY()

    // 카드 숫자 (1~13, 0 = 조커)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Info")
    int32 Value;

    // 카드 무늬 (Spade, Heart, Club, Diamond, Joker)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Info")
    FString Suit;

    // 카드별 3D 메시 에셋 (SM_Card_Spade_1 등)
    // TSoftObjectPtr: 게임 시작 시 53장을 한꺼번에 메모리에 올리지 않고 필요할 때만 로딩
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Visual")
    TSoftObjectPtr<UStaticMesh> CardMesh;

    // 기본 생성자: 초기값 설정
    FCardData() : Value(1), Suit(TEXT("Spade")), CardMesh(nullptr) {}
};

UCLASS()
class INDIAN_BAB_API ACardManager : public AActor
{
    GENERATED_BODY()

public:
    ACardManager();

    // 에디터에서 직접 할당할 데이터 테이블
    // 이 테이블에 53장의 카드 정보(숫자, 무늬, 메시)가 들어있음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Logic")
    UDataTable* CardDataTable;

    // 현재 게임에서 실제로 사용 중인 덱 (데이터 테이블의 복사본)
    // 카드를 뽑을 때마다 이 배열에서 제거됨
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card Logic")
    TArray<FCardData> CurrentDeck;

    // 데이터 테이블에서 카드를 읽어와 덱을 구성하고 셔플하는 함수
    // 매 라운드 시작 시 호출
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    void InitializeDeck();

    // 덱 맨 위에서 카드 한 장을 뽑고 덱에서 제거하는 함수
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    FCardData DrawCard();

    // 플레이어 수만큼 카드를 한 장씩 뽑아 배열로 반환하는 함수
    // 반환된 배열의 인덱스 = 플레이어 순서
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    TArray<FCardData> DealCards(int32 PlayerCount);

protected:
    // 게임 시작 시 자동으로 덱 초기화
    virtual void BeginPlay() override;
};