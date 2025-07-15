// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

    UFUNCTION()
    void RequestPlaceUse();

    UFUNCTION()
    void RequestSpawn();

    UFUNCTION()
    void UpdateInventory(UDojoModelCraftIslandPocketInventory* Inventory);

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

public:
	// Sets default values for this actor's properties
	ADojoCraftIslandManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
    
    UFUNCTION()
    void ContinueAfterDelay();

    FTimerHandle DelayTimerHandle;

    // Runtime account returned from CreateBurner
    UPROPERTY()
    FAccount Account;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

    // Widgets
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> OnboardingWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> UserInterfaceWidgetClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<APawn> PlayerPawnClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<AActor> ADefaultBlockClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<AActor> ADefaultHarvestableClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<AActor> ADefaultWorldStructureClass;

    UPROPERTY(EditAnywhere, Category = "Spawning")
    TSubclassOf<AActor> FloatingShipClass;

    UPROPERTY()
    UUserWidget* OnboardingUI;

    UPROPERTY()
    UUserWidget* UI;

    UPROPERTY()
    AActor* FloatingShip;
    
    FIntVector TargetBlock;

    bool bLoaded = false;
    
    void InitUIAndActors();
    
	FIntVector HexToIntVector(const FString& Source);
    int HexToInteger(const FString& Hex);
	
	void HandleInventory(UDojoModel* Object);
    
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
};
