// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "../DojoHelpers.h"
#include "E_Item.h"
#include "CraftIslandGameInst.generated.h"

// Event dispatcher declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestPlaceBlock);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestExecute);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestSpawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUpdateInventory, UDojoModelCraftIslandPocketInventory*, Inventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUpdatePlayerData, UDojoModelCraftIslandPocketPlayerData*, PlayerData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUpdateProcessingLock, UDojoModelCraftIslandPocketProcessingLock*, ProcessingLock);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventorySelectHotbar, UObject*, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestPlaceUse);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FRequestInventoryMoveItem, int32, FromInventory, int32, FromSlot, int32, ToInventory, int32, ToSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRequestCraft, int32, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestGiveItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSetTargetBlock, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestVisitNewIsland);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestGoBackHome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestExploreIslandPart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestSell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FToggleCraftUI, bool, bShow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRequestBuy, int32, ItemId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRequestStartProcessing, uint8, ProcessType, int32, InputAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRequestCancelProcessing);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FToggleLockUI, uint8, ProcessType, bool, bShow);

// Queue-based delegates for batching
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FQueueInventoryMoveItem, int32, FromInventory, int32, FromSlot, int32, ToInventory, int32, ToSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQueueCraft, int32, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQueueSell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQueueBuy, const TArray<int32>&, ItemIds, const TArray<int32>&, Quantities);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQueueVisit, int32, SpaceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFlushActionQueue);

// Optimistic update delegates (forwarded from DojoCraftIslandManager)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnOptimisticInventoryMove, int32, FromInventory, int32, FromSlot, int32, ToInventory, int32, ToSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOptimisticCraft, int32, ItemId, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOptimisticSell, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionQueueUpdate, int32, PendingActionCount);

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
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestPlaceBlock RequestPlaceBlock;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestExecute RequestExecute;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestSpawn RequestSpawn;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FUpdateInventory UpdateInventory;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FUpdatePlayerData UpdatePlayerData;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FUpdateProcessingLock UpdateProcessingLock;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FInventorySelectHotbar InventorySelectHotbar;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestHit RequestHit;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestPlaceUse RequestPlaceUse;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestInventoryMoveItem RequestInventoryMoveItem;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestCraft RequestCraft;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestGiveItem FRequestGiveItem;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FSetTargetBlock SetTargetBlock;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestVisitNewIsland RequestVisitNewIsland;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestGoBackHome RequestGoBackHome;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestExploreIslandPart RequestExploreIslandPart;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FOnShipClicked OnShipClicked;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestSell RequestSell;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestBuy RequestBuy;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FToggleCraftUI ToggleCraftUI;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestStartProcessing RequestStartProcessing;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FRequestCancelProcessing RequestCancelProcessing;

    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Event Dispatchers")
    FToggleLockUI ToggleLockUI;
    
    // Queue-based event dispatchers
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Queue Dispatchers")
    FQueueInventoryMoveItem QueueInventoryMoveItem;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Queue Dispatchers")
    FQueueCraft QueueCraft;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Queue Dispatchers")
    FQueueSell QueueSell;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Queue Dispatchers")
    FQueueBuy QueueBuy;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Queue Dispatchers")
    FQueueVisit QueueVisit;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Queue Dispatchers")
    FFlushActionQueue FlushActionQueue;
    
    // Optimistic update events
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Optimistic Updates")
    FOnOptimisticInventoryMove OnOptimisticInventoryMove;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Optimistic Updates")
    FOnOptimisticCraft OnOptimisticCraft;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Optimistic Updates")
    FOnOptimisticSell OnOptimisticSell;
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable, Category = "Optimistic Updates")
    FOnActionQueueUpdate OnActionQueueUpdate;

    // Debug function to log Dojo memory usage
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void LogDojoMemoryUsage();

    // UI Toggle Helper Functions
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowCraftUI() { ToggleCraftUI.Broadcast(true); }
    
    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideCraftUI() { ToggleCraftUI.Broadcast(false); }
    
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowProcessingUI(uint8 ProcessType) { ToggleLockUI.Broadcast(ProcessType, true); }
    
    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideProcessingUI(uint8 ProcessType) { ToggleLockUI.Broadcast(ProcessType, false); }
    
    // Queue helper functions for Blueprints
    UFUNCTION(BlueprintCallable, Category = "Action Queue")
    void RequestQueueInventoryMove(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot) 
    { 
        QueueInventoryMoveItem.Broadcast(FromInventory, FromSlot, ToInventory, ToSlot); 
    }
    
    UFUNCTION(BlueprintCallable, Category = "Action Queue")
    void RequestQueueCraft(int32 Item) 
    { 
        QueueCraft.Broadcast(Item); 
    }
    
    UFUNCTION(BlueprintCallable, Category = "Action Queue")
    void RequestQueueSell() 
    { 
        QueueSell.Broadcast(); 
    }
    
    UFUNCTION(BlueprintCallable, Category = "Action Queue")
    void RequestQueueBuy(const TArray<int32>& ItemIds, const TArray<int32>& Quantities) 
    { 
        QueueBuy.Broadcast(ItemIds, Quantities); 
    }
    
    UFUNCTION(BlueprintCallable, Category = "Action Queue")
    void RequestQueueVisit(int32 SpaceId) 
    { 
        QueueVisit.Broadcast(SpaceId); 
    }
    
    UFUNCTION(BlueprintCallable, Category = "Action Queue")
    void RequestFlushActionQueue() 
    { 
        FlushActionQueue.Broadcast(); 
    }
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Action Queue")
    int32 GetPendingActionCount();
    
    // Get reference to DojoCraftIslandManager (weak reference to avoid circular dependency)
    class ADojoCraftIslandManager* GetDojoCraftIslandManager() const;
    
protected:
    // Weak reference to avoid circular dependency - found dynamically when needed
    mutable TWeakObjectPtr<class ADojoCraftIslandManager> CachedDojoCraftIslandManagerRef;

    // Override Init
    virtual void Init() override;

private:
    // Internal handlers for queue delegates
    UFUNCTION()
    void OnQueueInventoryMove(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot);
    
    UFUNCTION()
    void OnQueueCraft(int32 Item);
    
    UFUNCTION()
    void OnQueueSell();
    
    UFUNCTION()
    void OnQueueBuy(const TArray<int32>& ItemIds, const TArray<int32>& Quantities);
    
    UFUNCTION()
    void OnQueueVisit(int32 SpaceId);
    
    UFUNCTION()
    void OnFlushActionQueue();

};
