// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "../DojoHelpers.h"
#include "CraftIslandGameInst.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "BaseBlock.h"
#include "BaseObject.h"
#include "BaseWorldStructure.h"
#include "E_Item.h"
#include "PaperSprite.h"
#include "CraftIslandChunks.h"

#include "DojoCraftIslandManager.generated.h"

USTRUCT(BlueprintType)
struct FItemDataRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Index;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    E_Item Enum;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxStackSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UPaperSprite* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Craft;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BuyPrice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SellPrice;
};

USTRUCT(BlueprintType)
struct FSpawnQueueData
{
    GENERATED_BODY()

    UPROPERTY()
    E_Item Item;

    UPROPERTY()
    FIntVector DojoPosition;

    UPROPERTY()
    bool Validated;

    UPROPERTY()
    UDojoModel* DojoModel;

    FSpawnQueueData()
    {
        Item = E_Item::None;
        DojoPosition = FIntVector::ZeroValue;
        Validated = false;
        DojoModel = nullptr;
    }

    FSpawnQueueData(E_Item InItem, const FIntVector& InPosition, bool InValidated, UDojoModel* InModel = nullptr)
    {
        Item = InItem;
        DojoPosition = InPosition;
        Validated = InValidated;
        DojoModel = InModel;
    }
};

USTRUCT(BlueprintType)
struct FProcessingLock
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Player;

    UPROPERTY(BlueprintReadOnly)
    int64 UnlockTime;

    UPROPERTY(BlueprintReadOnly)
    uint8 ProcessType;

    UPROPERTY(BlueprintReadOnly)
    int32 BatchesProcessed;

    FProcessingLock()
    {
        Player = "";
        UnlockTime = 0;
        ProcessType = 0;
        BatchesProcessed = 0;
    }
};

UENUM()
enum class ETransactionType : uint8
{
    PlaceUse,
    Hit,
    SelectHotbar,
    MoveItem,
    Craft,
    Buy,
    Sell,
    StartProcess,
    CancelProcess,
    Visit,
    VisitNewIsland,
    GenerateIsland,
    Other
};

USTRUCT()
struct FTransactionQueueItem
{
    GENERATED_BODY()

    UPROPERTY()
    ETransactionType Type;

    UPROPERTY()
    FIntVector Position;
    
    UPROPERTY()
    E_Item ItemType;

    UPROPERTY()
    int32 IntParam;
    
    UPROPERTY()
    int32 IntParam2;
    
    UPROPERTY()
    int32 IntParam3;
    
    UPROPERTY()
    int32 IntParam4;
    
    UPROPERTY()
    TArray<int32> ItemIds;
    
    UPROPERTY()
    TArray<int32> Quantities;
    
    // Sequence number for tracking order (especially for hotbar selections)
    UPROPERTY()
    int32 SequenceNumber;
    
    FTransactionQueueItem()
    {
        Type = ETransactionType::Other;
        Position = FIntVector::ZeroValue;
        ItemType = E_Item::None;
        IntParam = 0;
        IntParam2 = 0;
        IntParam3 = 0;
        IntParam4 = 0;
        SequenceNumber = 0;
    }
};

UENUM(BlueprintType)
enum class EActorSpawnType : uint8
{
    ChunkBlock,
    GatherableResource,
    WorldStructure
};

USTRUCT(BlueprintType)
struct FActorSpawnInfo
{
    GENERATED_BODY()

    UPROPERTY()
    AActor* Actor;

    UPROPERTY()
    EActorSpawnType SpawnType;

    FActorSpawnInfo()
    {
        Actor = nullptr;
        SpawnType = EActorSpawnType::ChunkBlock;
    }

    FActorSpawnInfo(AActor* InActor, EActorSpawnType InSpawnType)
    {
        Actor = InActor;
        SpawnType = InSpawnType;
    }
};



class DataTableHelpers
{
public:
    template<typename T>
    static const void* FindRowByColumnValue(UDataTable* Table, FName ColumnName, T Value)
    {
        if (!Table) return nullptr;

        for (const auto& Pair : Table->GetRowMap())
        {
            uint8* RowMemory = Pair.Value;
            if (!RowMemory) continue;

            const UScriptStruct* RowStruct = Table->GetRowStruct();
            if (!RowStruct) continue;

            FProperty* Property = RowStruct->FindPropertyByName(ColumnName);
            if (!Property) continue;

            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(RowMemory);

            if constexpr (std::is_same<T, int32>::value)
            {
                if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
                {
                    if (IntProp->GetPropertyValue(ValuePtr) == Value)
                    {
                        return RowMemory;
                    }
                }
            }
            else if constexpr (std::is_same<T, FName>::value)
            {
                if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
                {
                    if (NameProp->GetPropertyValue(ValuePtr) == Value)
                    {
                        return RowMemory;
                    }
                }
            }
            else if constexpr (std::is_same<T, FString>::value)
            {
                if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
                {
                    if (StrProp->GetPropertyValue(ValuePtr) == Value)
                    {
                        return RowMemory;
                    }
                }
            }

            // Add more type branches as needed (float, bool, etc.)
        }

        return nullptr;
    }
};

UCLASS()
class CRAFTISLANDPOCKET3_API ADojoCraftIslandManager : public AActor
{
	GENERATED_BODY()

private:
    // Constants for spawn positions
    static const FVector DEFAULT_OUTDOOR_SPAWN_POS;
    static const FVector DEFAULT_BUILDING_SPAWN_POS;
    static constexpr float CAMERA_LAG_REENABLE_DELAY = 0.1f;

    UFUNCTION()
    void RequestPlaceUse();

    UFUNCTION()
    void RequestSpawn();

    UFUNCTION()
    void InventorySelectHotbar(UObject* Slot);

    UFUNCTION()
    void RequestHit();

    UFUNCTION()
    void RequestInventoryMoveItem(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot);

    UFUNCTION()
    void RequestCraft(int32 Item);

    UFUNCTION()
    void RequestGiveItem();

    UFUNCTION()
    void SetTargetBlock(FVector Location);

    UFUNCTION()
    void RequestSell();

    UFUNCTION()
    void RequestBuy(int32 ItemId, int32 Quantity);

    UFUNCTION()
    void RequestGoBackHome();

    UFUNCTION()
    void RequestStartProcessing(uint8 ProcessType, int32 InputAmount);

    UFUNCTION()
    void RequestCancelProcessing();

public:
	// Sets default values for this actor's properties
	ADojoCraftIslandManager();
    
    // Queue methods for batching
    UFUNCTION(BlueprintCallable, Category = "Batching")
    void QueueInventoryMove(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot);
    
    UFUNCTION(BlueprintCallable, Category = "Batching")
    void QueueCraft(int32 ItemId);
    
    UFUNCTION(BlueprintCallable, Category = "Batching")
    void QueueBuy(const TArray<int32>& ItemIds, const TArray<int32>& Quantities);
    
    UFUNCTION(BlueprintCallable, Category = "Batching")
    void QueueSell();
    
    UFUNCTION(BlueprintCallable, Category = "Batching")
    void QueueVisit(int32 SpaceId);
    
    UFUNCTION(BlueprintCallable, Category = "Batching")
    void FlushActionQueue();
    
    UFUNCTION(BlueprintCallable, Category = "Batching")
    int32 GetPendingActionCount() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void ContinueAfterDelay();

    FTimerHandle DelayTimerHandle;

    // Runtime account returned from CreateBurner
    UPROPERTY()
    FAccount Account;

public:
    // Event delegates for optimistic updates
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOptimisticInventoryMove OnOptimisticInventoryMove;
    
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOptimisticCraft OnOptimisticCraft;
    
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOptimisticSell OnOptimisticSell;
    
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnActionQueueUpdate OnActionQueueUpdate;
    
    // Widgets
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> OnboardingWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> UserInterfaceWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> SellInterfaceWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> StoneCraftInterfaceWidgetClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<APawn> PlayerPawnClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<AActor> ADefaultBlockClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<AActor> ADefaultHarvestableClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<AActor> ADefaultWorldStructureClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<AActor> DefaultBuildingClass;

    UPROPERTY(EditAnywhere, Category = "Spawning")
    TSubclassOf<AActor> FloatingShipClass;

    UPROPERTY()
    UUserWidget* OnboardingUI;

    UPROPERTY()
    UUserWidget* UI;

    UPROPERTY()
    UUserWidget* SellUI;

    UPROPERTY()
    AActor* FloatingShip;

    FIntVector TargetBlock;
    
    // Store rock position for stone crafting when interface is opened
    UPROPERTY()
    FIntVector StoneCraftTargetBlock;

    bool bLoaded = false;

    void InitUIAndActors();

	FIntVector HexToIntVector(const FString& Source);
    int HexToInteger(const FString& Hex);

    void HandleInventory(UDojoModel* Object);

    void HandlePlayerData(UDojoModel* Object);

    void HandleProcessingLock(UDojoModel* Object);

    bool IsCurrentPlayer() const;

    // Assignable in editor or spawned
    UPROPERTY(EditAnywhere, Category = "Config")
    FString WorldAddress;

    // Assignable in editor or spawned
    UPROPERTY(EditAnywhere, Category = "Config")
    FString ToriiUrl;

    UPROPERTY(EditAnywhere, Category = "Config")
    TMap<FString, FString> ContractsAddresses;

    UFUNCTION()
    void HandleDojoModel(UDojoModel* Model);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dojo")
    ADojoHelpers* DojoHelpers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dojo")
    FString RpcUrl;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dojo")
    FString PlayerAddress;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dojo")
    FString PrivateKey;

    void CraftIslandSpawn();

    void OnUIDelayedLoad();

    // UPROPERTYs assumed available:
    FIntVector ActionDojoPosition;

    UPROPERTY()
    TMap<FIntVector, AActor*> Actors;

    UPROPERTY()
    TMap<FIntVector, FActorSpawnInfo> ActorSpawnInfo;

    UPROPERTY()
    TArray<FSpawnQueueData> SpawnQueue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    UDataTable* ItemDataTable;

    AActor* PlaceAssetInWorld(E_Item Item, const FIntVector& DojoPosition, bool Validated, EActorSpawnType SpawnType = EActorSpawnType::ChunkBlock);

    int32 DojoPositionToLocalPosition(const FIntVector& DojoPosition);

    FTransform DojoPositionToTransform(const FIntVector& DojoPosition);

    FIntVector GetWorldPositionFromLocal(int Position, const FIntVector& ChunkOffset);

    FIntVector HexStringToVector(const FString& Source);

    void ConnectGameInstanceEvents();
    int32 CurrentItemId;

    // Chunk caching system
    UPROPERTY()
    TMap<FString, FSpaceChunks> ChunkCache;

    // Helper functions to reduce code duplication
    void QueueSpawnWithOverflowProtection(const FSpawnQueueData& SpawnData);
    void QueueSpawnBatchWithOverflowProtection(const TArray<FSpawnQueueData>& SpawnDataBatch);
    void RemoveActorAtPosition(const FIntVector& DojoPosition, EActorSpawnType RequiredType);
    void ProcessChunkBlock(uint8 Byte, const FIntVector& DojoPosition, E_Item Item, TArray<FSpawnQueueData>& ChunkSpawnData);
    void ProcessGatherableResource(UDojoModelCraftIslandPocketGatherableResource* Gatherable);
    void ProcessWorldStructure(UDojoModelCraftIslandPocketWorldStructure* Structure);
    void ProcessIslandChunk(UDojoModelCraftIslandPocketIslandChunk* Chunk);

    // Get current player's island key for chunk cache
    FString GetCurrentIslandKey() const;

    // Load all chunks from cache
    void LoadAllChunksFromCache();
    void LoadChunkFromCache(const FString& ChunkId);

    // Clear all spawned actors
    void ClearAllSpawnedActors();

    // Set visibility and collision for a group of actors
    void SetActorsVisibilityAndCollision(bool bVisible, bool bEnableCollision);

    // Current space tracking
    UPROPERTY()
    FString CurrentSpaceOwner;

    UPROPERTY()
    int32 CurrentSpaceId;

    // Default building for empty spaces
    UPROPERTY()
    AActor* DefaultBuilding;

    // Sky atmosphere reference
    UPROPERTY()
    AActor* SkyAtmosphere;

    // Track if space 1 actors are hidden
    bool bSpace1ActorsHidden;
    
    // Track if this is the first player data received
    bool bFirstPlayerDataReceived;

    // Store player positions for each space
    UPROPERTY()
    TMap<FString, FVector> SpacePlayerPositions;

    // Current player inventory
    UPROPERTY()
    UDojoModelCraftIslandPocketInventory* CurrentInventory;

    // Get currently selected item ID from inventory
    int32 GetSelectedItemId() const;

    // Track the structure type of the current space (0 if not in a structure)
    UPROPERTY()
    int32 CurrentSpaceStructureType;

    // Processing functions
    UFUNCTION(BlueprintCallable, Category = "Processing")
    void StartProcessing(uint8 ProcessType, int32 InputAmount);

    UFUNCTION(BlueprintCallable, Category = "Processing")
    void CancelProcessing();

    UFUNCTION(BlueprintCallable, Category = "Processing")
    void ToggleProcessingUI(uint8 ProcessType, bool bShow);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ToggleCraftUI(bool bShow);

    UFUNCTION(BlueprintCallable, Category = "Processing")
    bool IsPlayerProcessing() const;

    UFUNCTION(BlueprintCallable, Category = "Processing")
    float GetProcessingTimeRemaining() const;

    UFUNCTION(BlueprintCallable, Category = "Processing")
    FString GetProcessingTypeName(uint8 ProcessType) const;

    // Current processing lock state
    UPROPERTY()
    FProcessingLock CurrentProcessingLock;

    // Material for gold highlighting
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
    UMaterialInterface* GoldMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
    UMaterialInterface* GoldLeavesMaterial;

    // Optimistic rendering materials
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
    UMaterialInterface* PendingMaterial;

    // Map of pending optimistic actions by position with timestamps
    UPROPERTY()
    TMap<FIntVector, AActor*> OptimisticActors;
    
    // Track when optimistic actors were created for cleanup
    TMap<FIntVector, double> OptimisticActorTimestamps;
    FTimerHandle OptimisticCleanupTimerHandle;
    static constexpr float OPTIMISTIC_TIMEOUT_SECONDS = 30.0f; // Rollback after 30 seconds

    // Optimistic inventory changes
    UPROPERTY()
    TMap<int32, int32> OptimisticInventoryChanges; // ItemId -> QuantityChange

    // Track locally selected hotbar slot for optimistic rendering
    UPROPERTY()
    int32 LocalHotbarSelectedSlot;
    
    // Hotbar selection timing and sequence tracking
    double LocalHotbarSelectionTimestamp;
    int32 LocalHotbarSelectionSequence;
    int32 NextHotbarSequence;
    FTimerHandle HotbarTimeoutHandle;
    bool bHotbarSelectionPending; // True when local selection differs from server
    static constexpr float HOTBAR_TIMEOUT_SECONDS = 2.0f; // Reset after 2 seconds

    // Transaction queue for blockchain calls (thread-safe)
    TQueue<FTransactionQueueItem> TransactionQueue;
    mutable FCriticalSection TransactionQueueMutex; // Protect queue access
    bool bIsProcessingTransaction;
    FTimerHandle TransactionTimeoutHandle;
    int32 TransactionQueueCount;
    static constexpr int32 MAX_QUEUE_SIZE = 1000; // Prevent unbounded growth

    private:
    // Transaction queue methods
    void QueueTransaction(const FTransactionQueueItem& Item);
    void ProcessNextTransaction();
    void ProcessSingleAction(const FTransactionQueueItem& Action);
    void OnTransactionComplete();
    
    // Universal action encoder functions
    TArray<FString> EncodePackedActions(const TArray<FTransactionQueueItem>& Actions);
    FString PackType0Actions(const TArray<FTransactionQueueItem>& Actions, int32& Index);
    FString PackType1Actions(const TArray<FTransactionQueueItem>& Actions, int32& Index);
    FString PackType2Actions(const TArray<FTransactionQueueItem>& Actions, int32& Index);
    FString PackType3Action(const FTransactionQueueItem& Action);
    bool CanBatchAction(ETransactionType Type);
    int32 GetActionSize(ETransactionType Type);
    
    // Bit manipulation helpers
    void WriteBits(TArray<uint8>& Bytes, int32& BitOffset, uint64 Value, int32 NumBits);
    FString BytesToHexString(const TArray<uint8>& Bytes);
    // Optimistic rendering methods
    void AddOptimisticPlacement(const FIntVector& Position, E_Item Item);
    void AddOptimisticRemoval(const FIntVector& Position);
    void ConfirmOptimisticAction(const FIntVector& Position);
    void RollbackOptimisticAction(const FIntVector& Position);
    void ApplyPendingVisual(AActor* Actor);
    void RemovePendingVisual(AActor* Actor);

    // Helper methods for space transitions
    FString MakeSpaceKey(const FString& Owner, int32 Id) const;
    void SaveCurrentPlayerPosition();
    void HandleSpaceTransition(UDojoModelCraftIslandPocketPlayerData* PlayerData);
    FVector GetSpawnPositionForSpace(const FString& SpaceKey, bool bHasBlockChunks);
    void TeleportPlayer(const FVector& NewLocation, bool bImmediate = false);

    // Camera utility methods
    void SetCameraLag(APawn* Pawn, bool bEnableLag);
    void DisableCameraLagDuringTeleport(APawn* Pawn);
    
    // Optimistic inventory update methods
    void ApplyOptimisticInventoryMove(const FTransactionQueueItem& Action);
    void RollbackOptimisticInventoryMove(const FTransactionQueueItem& Action);
    void ConfirmOptimisticInventoryActions();
    
    // Optimistic actor cleanup
    void CleanupTimedOutOptimisticActors();
    void StartOptimisticCleanupTimer();
    
    // Hotbar timeout handling
    void ResetLocalHotbarSelection();
    
    // Lazy hotbar update - queues hotbar selection if pending
    void QueuePendingHotbarSelection();
    
    // Batch timing configuration
    static constexpr float BATCH_WAIT_TIME = 0.5f;
    static constexpr int32 MAX_BATCH_SIZE = 10;
    static constexpr int32 FORCE_SEND_SIZE = 8;
    
    // Timer for batching
    FTimerHandle BatchTimerHandle;
    
    // Optimistic inventory state
    struct FOptimisticInventoryState
    {
        TMap<FIntPoint, E_Item> Items; // Key: (InventoryId, Slot)
        TMap<FIntPoint, int32> Quantities;
        TArray<FTransactionQueueItem> PendingActions;
    };
    
    FOptimisticInventoryState OptimisticInventory;
};
