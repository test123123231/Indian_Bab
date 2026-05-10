#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "CardData.generated.h"

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
    FCardData() : Value(0), Suit(TEXT("")), CardMesh(nullptr) {}
};


