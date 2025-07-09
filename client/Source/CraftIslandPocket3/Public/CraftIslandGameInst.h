// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "../DojoHelpers.h"
#include "CraftIslandGameInst.generated.h"

// Event dispatcher declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestPlaceBlock);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestExecute);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestPlace);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestSpawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUpdateInventory, UDojoModelCraftIslandPocketInventory*, Inventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventorySelectHotbar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestPlaceUse);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestStoneDraft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestInventoryMoveItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestCraft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestGiveItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSetTargetBlock, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestVisitNewIsland);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestGoBackHome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestExploreIslandPart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipClicked);

/**
 * 
 */
UCLASS()
class CRAFTISLANDPOCKET3_API UCraftIslandGameInst : public UGameInstance
{
	GENERATED_BODY()

public:
    // Variables
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables")
    int32 EItem;

    // Event Dispatchers
    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestPlaceBlock RequestPlaceBlock;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestExecute RequestExecute;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestPlace RequestPlace;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestSpawn RequestSpawn;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FUpdateInventory UpdateInventory;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FInventorySelectHotbar InventorySelectHotbar;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestHit RequestHit;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestPlaceUse RequestPlaceUse;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestStoneDraft RequestStoneDraft;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestInventoryMoveItem FRequestInventoryMoveItem;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestCraft RequestCraft;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestGiveItem FRequestGiveItem;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FSetTargetBlock SetTargetBlock;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestVisitNewIsland RequestVisitNewIsland;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestGoBackHome RequestGoBackHome;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FRequestExploreIslandPart RequestExploreIslandPart;

    UPROPERTY(BlueprintAssignable, Category = "Event Dispatchers")
    FOnShipClicked OnShipClicked;
	
};
