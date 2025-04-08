//
//  GeneratedModels.h
//  dojo_starter_ue5 (Mac)
//
//  Created by Corentin Cailleaud on 21/10/2024.
//  Copyright © 2024 Epic Games, Inc. All rights reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeneratedModels.generated.h"

USTRUCT(BlueprintType)
struct FModelIslandChunk
{
   GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString island_id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString chunk_id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString blocks;
};
typedef struct FModelIslandChunk FModelIslandChunk;

USTRUCT(BlueprintType)
struct FModelGatherableResource
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString island_id;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString chunk_id;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int position;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int resource_id;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 planted_at;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 next_harvest_at;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 harvested_at;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int max_harvest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int remained_harvest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool destroyed;
};
typedef struct FModelGatherableResource FModelGatherableResource;
