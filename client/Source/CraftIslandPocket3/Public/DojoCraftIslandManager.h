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
#include "DojoCraftIslandManager.generated.h"

class ABaseBlock;

UENUM(BlueprintType)
enum class E_Item : uint8
{
    None UMETA(DisplayName = "None"),
    Grass UMETA(DisplayName = "Grass"),
    Dirt UMETA(DisplayName = "Dirt"),
    Stone UMETA(DisplayName = "Stone"),
    
    NewEnumerator4,
    NewEnumerator5,
    NewEnumerator6,
    NewEnumerator7,
    NewEnumerator8,
    NewEnumerator9,
    NewEnumerator10,
    NewEnumerator11,
    NewEnumerator12,
    NewEnumerator13,
    NewEnumerator14,
    NewEnumerator15,
    NewEnumerator16,
    NewEnumerator17,
    NewEnumerator18,
    NewEnumerator19,
    NewEnumerator20,
    NewEnumerator21,
    NewEnumerator22,
    NewEnumerator23,
    NewEnumerator24,
    NewEnumerator25,
    NewEnumerator26,
    NewEnumerator27,
    NewEnumerator28,
    NewEnumerator29,

    House UMETA(DisplayName = "House"),           // Index 30
    OakTree UMETA(DisplayName = "Oak Tree"),      // Index 31
    WoodenStick2 UMETA(DisplayName = "Wooden Stick"), // Index 32
    Rock UMETA(DisplayName = "Rock"),
    StoneAxeHead UMETA(DisplayName = "Stone Axe Head"),
    StoneAxe UMETA(DisplayName = "Stone Axe"),
    StonePickaxeHead UMETA(DisplayName = "Stone Pickaxe Head"),
    StonePickaxe UMETA(DisplayName = "Stone Pickaxe"),
    StoneShovelHead UMETA(DisplayName = "Stone Shovel Head"),
    StoneShovel UMETA(DisplayName = "Stone Shovel"),
    StoneHoeHead UMETA(DisplayName = "Stone Hoe Head"),
    StoneHoe UMETA(DisplayName = "Stone Hoe"),
    StoneHammerHead UMETA(DisplayName = "Stone Hammer Head"),
    StoneHammer UMETA(DisplayName = "Stone Hammer"),
    OakLog UMETA(DisplayName = "Oak Log"),
    OakPlank UMETA(DisplayName = "Oak Plank"),
    OakSapling UMETA(DisplayName = "Oak Sapling"),
    WheatSeed UMETA(DisplayName = "Wheat Seed"),
    Wheat UMETA(DisplayName = "Wheat"),
    Boulder UMETA(DisplayName = "Boulder"),
    HousePattern UMETA(DisplayName = "House Pattern"),
};

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
    TSoftObjectPtr<UTexture2D> Icon;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Craft;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AActor> ActorClass;
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
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    UDataTable* ItemDataTable;
    
    AActor* PlaceAssetInWorld(E_Item Item, const FIntVector& DojoPosition, bool Validated);

    int32 DojoPositionToLocalPosition(const FIntVector& DojoPosition);

    FTransform DojoPositionToTransform(const FIntVector& DojoPosition);
    
    FIntVector GetWorldPositionFromLocal(int Position, const FIntVector& ChunkOffset);
    
    FIntVector HexStringToVector(const FString& Source);
};
