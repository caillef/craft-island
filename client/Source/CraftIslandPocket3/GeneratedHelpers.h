//
//  GeneratedHelpers.hpp
//  dojo_starter_ue5 (Mac)
//
//  Created by Corentin Cailleaud on 21/10/2024.
//  Copyright © 2024 Epic Games, Inc. All rights reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DojoModule.h"
#include "Account.h"
#include "GeneratedModels.h"
#include "GeneratedHelpers.generated.h"

UCLASS()
class AGeneratedHelpers : public AActor
{
    GENERATED_BODY()

private:
    ToriiClient *toriiClient;
    
    Account *activeBurner;

public:
    AGeneratedHelpers();

    UFUNCTION(BlueprintCallable)
    void Connect(const FString& torii_url, const FString& rpc_url, const FString& world);

    UFUNCTION(BlueprintCallable)
    void GetAllEntities();

//    UFUNCTION(BlueprintCallable)
//    void SubscribeOnEntityPositionUpdate();
//  
    UFUNCTION(BlueprintCallable)
    void ExecuteRaw(const FAccount& account, const FString& to, const FString& selector, const FString& calldataParameter);

    UFUNCTION(BlueprintCallable)
    FAccount CreateAccount(const FString& rpc_url, const FString& address, const FString& private_key);

    UFUNCTION(BlueprintCallable)
    FAccount CreateBurner(const FString& rpc_url, const FString& address, const FString& private_key);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIslandChunkUpdated, FModelIslandChunk, IslandChunk);

    UPROPERTY(BlueprintAssignable)
    FOnIslandChunkUpdated OnIslandChunkUpdated;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGatherableResourceUpdated, FModelGatherableResource, GatherableResource);

    UPROPERTY(BlueprintAssignable)
    FOnGatherableResourceUpdated OnGatherableResourceUpdated;
};
