#include "CardController/CardManager.h"

// 생성자: 기본 설정
ACardManager::ACardManager()
{
    PrimaryActorTick.bCanEverTick = false; // 매 프레임 업데이트(Tick)가 필요 없으므로 꺼둠
}

// 게임이 시작될 때 실행
void ACardManager::BeginPlay()
{
    Super::BeginPlay();
    InitializeDeck(); // 덱 초기화 및 셔플 실행
}

// 데이터 테이블에서 정보를 읽어와 덱을 구성하는 핵심 함수
void ACardManager::InitializeDeck()
{
    // 1. 데이터 테이블이 할당되어 있는지 확인
    if (!CardDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("CardDataTable이 할당되지 않았습니다!"));
        return;
    }

    // 2. 이전에 남아있던 덱 정보를 비움
    CurrentDeck.Empty();

    // 3. 데이터 테이블의 모든 행(Row) 데이터를 가져옴
    TArray<FCardData*> AllRows;
    CardDataTable->GetAllRows<FCardData>(TEXT("Context_CardInit"), AllRows);

    // 4. 가져온 포인터 데이터들을 실제 덱 배열에 복사
    for (FCardData* Row : AllRows)
    {
        if (Row)
        {
            CurrentDeck.Add(*Row); // 구조체 복사
        }
    }

    // 5. 덱 섞기
    const int32 NumCards = CurrentDeck.Num();
    for (int32 i = 0; i < NumCards; ++i)
    {
        // 현재 인덱스부터 마지막 인덱스 사이의 무작위 위치 선정
        int32 RandomIndex = FMath::RandRange(i, NumCards - 1);

        // 두 카드의 위치를 바꿈
        if (i != RandomIndex)
        {
            CurrentDeck.Swap(i, RandomIndex);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("덱 초기화 완료: 총 %d장의 카드가 섞였습니다."), CurrentDeck.Num());
}

// 카드 한 장 뽑기 함수
FCardData ACardManager::DrawCard()
{
    // 덱에 카드가 남아있는지 확인
    if (CurrentDeck.Num() > 0)
    {
        FCardData Picked = CurrentDeck[0]; // 가장 위에 있는 카드 복사
        CurrentDeck.RemoveAt(0);           // 뽑은 카드는 덱에서 제거
        return Picked;                     // 결과 반환
    }

    // 카드가 없으면 빈 데이터 반환
    return FCardData();
}