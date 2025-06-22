// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "../DojoHelpers.h"
#include "CraftIslandChunks.generated.h"

USTRUCT(BlueprintType)
struct FSpaceChunks
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, UDojoModelCraftIslandPocketIslandChunk*> Chunks;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, UDojoModelCraftIslandPocketGatherableResource*> Gatherables;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, UDojoModelCraftIslandPocketWorldStructure*> Structures;
};

/**
 *
 */
UCLASS()
class CRAFTISLANDPOCKET3_API UCraftIslandChunks : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable)
    static void HandleCraftIslandModel(UDojoModel* model, UPARAM(ref) TMap<FString, FSpaceChunks>& RawSpaces);
};
