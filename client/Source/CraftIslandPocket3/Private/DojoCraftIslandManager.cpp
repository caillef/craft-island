// Fill out your copyright notice in the Description page of Project Settings.


#include "DojoCraftIslandManager.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"

// Define static constants
const FVector ADojoCraftIslandManager::DEFAULT_OUTDOOR_SPAWN_POS(50.0f, 50.0f, 250.0f);
const FVector ADojoCraftIslandManager::DEFAULT_BUILDING_SPAWN_POS(-40.0f, 180.0f, 100.0f);

// Sets default values
ADojoCraftIslandManager::ADojoCraftIslandManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// Initialize ghost preview
	GhostPreviewActor = nullptr;
	CurrentInventory = nullptr;
}

// Called when the game starts or when spawned
void ADojoCraftIslandManager::BeginPlay()
{
	Super::BeginPlay();

    this->InitUIAndActors();

    if (!DojoHelpers) return;

    // Step 1: Connect to Tori
    DojoHelpers->Connect(ToriiUrl, WorldAddress);

    // Step 2: Set contract addresses
    DojoHelpers->SetContractsAddresses(ContractsAddresses);

    // Step 3: Bind custom event to delegate
    DojoHelpers->OnDojoModelUpdated.AddDynamic(this, &ADojoCraftIslandManager::HandleDojoModel);

    // Step 4: Create burner account
    Account = DojoHelpers->CreateBurnerDeprecated(RpcUrl, PlayerAddress, PrivateKey);

    // Initialize current space tracking
    CurrentSpaceOwner = Account.Address;
    CurrentSpaceId = 1;

    // Initialize default building to nullptr
    DefaultBuilding = nullptr;

    // Initialize space 1 actors tracking
    bSpace1ActorsHidden = false;

    // Initialize inventory tracking
    CurrentInventory = nullptr;

    // Initialize structure type tracking
    CurrentSpaceStructureType = 0;

    // Find SkyAtmosphere actor in the scene
    SkyAtmosphere = nullptr;
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        if (Actor && Actor->GetClass()->GetName().Contains(TEXT("SkyAtmosphere")))
        {
            SkyAtmosphere = Actor;
            UE_LOG(LogTemp, Log, TEXT("Found SkyAtmosphere actor in scene"));
            break;
        }
    }

    ConnectGameInstanceEvents();

    DojoHelpers->SubscribeOnDojoModelUpdate();

    // Step 2: Call custom spawn function
    CraftIslandSpawn();

    // Step 3: Delay 1 second before continuing
    GetWorld()->GetTimerManager().SetTimer(
        DelayTimerHandle,
        this,
        &ADojoCraftIslandManager::ContinueAfterDelay,
        1.0f,
        false
    );
}

void ADojoCraftIslandManager::ConnectGameInstanceEvents()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    UCraftIslandGameInst* CraftIslandGI = Cast<UCraftIslandGameInst>(GameInstance);
    if (!CraftIslandGI) return;

    CraftIslandGI->RequestPlaceUse.AddDynamic(this, &ADojoCraftIslandManager::RequestPlaceUse);
    CraftIslandGI->RequestSpawn.AddDynamic(this, &ADojoCraftIslandManager::RequestSpawn);
    CraftIslandGI->InventorySelectHotbar.AddDynamic(this, &ADojoCraftIslandManager::InventorySelectHotbar);
    CraftIslandGI->RequestHit.AddDynamic(this, &ADojoCraftIslandManager::RequestHit);
    CraftIslandGI->RequestInventoryMoveItem.AddDynamic(this, &ADojoCraftIslandManager::RequestInventoryMoveItem);
    CraftIslandGI->RequestCraft.AddDynamic(this, &ADojoCraftIslandManager::RequestCraft);
    //    CraftIslandGI->RequestGiveItem.AddDynamic(this, &ADojoCraftIslandManager::RequestGiveItem);
    CraftIslandGI->SetTargetBlock.AddDynamic(this, &ADojoCraftIslandManager::SetTargetBlock);
    CraftIslandGI->RequestSell.AddDynamic(this, &ADojoCraftIslandManager::RequestSell);
    CraftIslandGI->RequestBuy.AddDynamic(this, &ADojoCraftIslandManager::RequestBuy);
    CraftIslandGI->RequestGoBackHome.AddDynamic(this, &ADojoCraftIslandManager::RequestGoBackHome);
}

void ADojoCraftIslandManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update ghost preview position if it exists and a building is selected
    if (GhostPreviewActor)
    {
        int32 SelectedItemId = GetSelectedItemId();
        if (IsBuildingPattern(SelectedItemId))
        {
            // Get player position and calculate north position
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (APawn* PlayerPawn = PC->GetPawn())
                {
                    FVector PlayerLocation = PlayerPawn->GetActorLocation();
                    
                    // Calculate position one block north of player
                    // In this coordinate system, X is north/south
                    FVector GhostLocation = PlayerLocation;
                    GhostLocation.X -= 50.0f; // One block north (negative X)
                    
                    // Snap to grid
                    GhostLocation.X = FMath::RoundToFloat(GhostLocation.X / 50.0f) * 50.0f;
                    GhostLocation.Y = FMath::RoundToFloat(GhostLocation.Y / 50.0f) * 50.0f;
                    GhostLocation.Z = FMath::RoundToFloat(GhostLocation.Z / 50.0f) * 50.0f;
                    
                    // Ensure it's above ground
                    GhostLocation.Z += 50.0f; // One block above current position
                    
                    // Update ghost position
                    GhostPreviewActor->SetActorLocation(GhostLocation);
                }
            }
        }
        else
        {
            // Remove ghost if no building is selected
            RemoveGhostPreview();
        }
    }
    
    // Original Tick functionality for handling target blocks and spawn queue
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    APawn* PlayerPawn = PC->GetPawn();
    if (!PlayerPawn) return;

    FVector TargetLocation = FVector::ZeroVector;

    FProperty* Property = PlayerPawn->GetClass()->FindPropertyByName(FName("TargetBlock"));
    if (Property)
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);
        if (StructProp && StructProp->Struct == TBaseStructure<FVector>::Get())
        {
            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(PlayerPawn);
            TargetLocation = *static_cast<FVector*>(ValuePtr);
        }
    }

    // Convert to IntVector, truncate, and modify Z
    int32 X = FMath::TruncToInt(TargetLocation.X + 8192);
    int32 Y = FMath::TruncToInt(TargetLocation.Y + 8192);
    int32 Z = FMath::TruncToInt(TargetLocation.Z + 8192);

    FIntVector TestVector(X, Y, Z);

    // Check if in Actors array
    bool bFound = Actors.Contains(TestVector);

    // Select Z value based on presence
    int32 ZValue = bFound ? 0 : -1;

    // Rebuild target vector
    FVector FinalTargetVector(TargetLocation.X, TargetLocation.Y, TargetLocation.Z + ZValue);

    // Set on GameInstance
    if (UGameInstance* GI = GetGameInstance())
    {
        UCraftIslandGameInst* CI = Cast<UCraftIslandGameInst>(GI);
        if (CI)
        {
            CI->SetTargetBlock.Broadcast(FinalTargetVector);
        }
    }

    // Set ActionDojoPosition
    ActionDojoPosition = FIntVector(X, Y, ZValue);

    // Process spawn queue - spawn up to 10 actors per tick
    const int32 MaxSpawnsPerFrame = 10;
    int32 SpawnsProcessed = 0;

    while (!SpawnQueue.IsEmpty() && SpawnsProcessed < MaxSpawnsPerFrame)
    {
        FSpawnQueueData SpawnData = SpawnQueue[0];
        SpawnQueue.RemoveAt(0);
        
        EActorSpawnType SpawnType = EActorSpawnType::ChunkBlock;
        if (SpawnData.DojoModel)
        {
            if (Cast<UDojoModelCraftIslandPocketGatherableResource>(SpawnData.DojoModel))
            {
                SpawnType = EActorSpawnType::GatherableResource;
            }
            else if (Cast<UDojoModelCraftIslandPocketWorldStructure>(SpawnData.DojoModel))
            {
                SpawnType = EActorSpawnType::WorldStructure;
            }
        }
        
        AActor* SpawnedActor = PlaceAssetInWorld(SpawnData.Item, SpawnData.DojoPosition, SpawnData.Validated, SpawnType);
        
        // Handle specific actor types based on the queued data
        if (SpawnedActor && SpawnData.DojoModel)
        {
            if (UDojoModelCraftIslandPocketGatherableResource* Gatherable = Cast<UDojoModelCraftIslandPocketGatherableResource>(SpawnData.DojoModel))
            {
                if (ABaseObject* ActorObject = Cast<ABaseObject>(SpawnedActor))
                {
                    ActorObject->GatherableResourceInfo = Gatherable;
                }
            }
            else if (UDojoModelCraftIslandPocketWorldStructure* Structure = Cast<UDojoModelCraftIslandPocketWorldStructure>(SpawnData.DojoModel))
            {
                if (ABaseWorldStructure* WorldStructureActor = Cast<ABaseWorldStructure>(SpawnedActor))
                {
                    WorldStructureActor->WorldStructure = Structure;
                    if (Structure->Completed)
                    {
                        WorldStructureActor->NotifyConstructionCompleted();
                    }
                }
            }
        }
        
        SpawnsProcessed++;
    }
}

void ADojoCraftIslandManager::ContinueAfterDelay()
{
    if (!DojoHelpers) return;

    // Step 4: Subscribe to dojo model updates (moved here to ensure Torii client is ready)
    UE_LOG(LogTemp, Log, TEXT("ContinueAfterDelay: Subscribing to model updates"));

    // Step 5: Fetch existing models
    //TODO: seems to fetch nothing, check query maybe
    DojoHelpers->FetchExistingModels();
}

void ADojoCraftIslandManager::InitUIAndActors()
{
    // Log ItemDataTable status
    UE_LOG(LogTemp, Warning, TEXT("InitUIAndActors: ItemDataTable = %s"), ItemDataTable ? TEXT("Valid") : TEXT("NULL"));
    // === 1. Create and Add Onboarding Widget ===
//    if (OnboardingWidgetClass)
//    {
//        OnboardingUI = CreateWidget<UUserWidget>(GetWorld(), OnboardingWidgetClass);
//        if (OnboardingUI)
//        {
//            OnboardingUI->AddToViewport();
//        }
//    }

    // === 2. Create and Add User Interface Widget ===
    if (UserInterfaceWidgetClass)
    {
        UI = CreateWidget<UUserWidget>(GetWorld(), UserInterfaceWidgetClass);
        if (UI)
        {
            UI->AddToViewport();
            bLoaded = false; // bool variable
        }
    }

    // === 3. Spawn Dojo Helpers Actor ===
    if (true) {
        FVector Location(0.f, 0.f, 2.f);
        FRotator Rotation(0.f, 0.f, 90.f);
        FVector Scale(1.f, 1.f, 1.f);

        FTransform SpawnTransform(Rotation, Location, Scale);

        DojoHelpers = GetWorld()->SpawnActor<ADojoHelpers>(ADojoHelpers::StaticClass(), SpawnTransform);
    }

    // === 4. Spawn Floating Ship Actor ===
    // if (FloatingShipClass)
    // {
    //     FVector Location(500.f, 3000.f, 2.f);
    //     FRotator Rotation(0.f, 0.f, 90.f);
    //     FVector Scale(1.f, 2.f, 1.f);

    //     FTransform SpawnTransform(Rotation, Location, Scale);

    //     FloatingShip = GetWorld()->SpawnActor<AActor>(FloatingShipClass, SpawnTransform);
    // }
}



int ADojoCraftIslandManager::HexToInteger(const FString& Hex)
{
    int32 OutNumber = 0;
    int32 Length = Hex.Len();

    for (int32 i = 0; i < Length; ++i)
    {
        TCHAR C = Hex[i];
        int32 Value = 0;

        if (C >= '0' && C <= '9') Value = C - '0';
        else if (C >= 'a' && C <= 'f') Value = 10 + (C - 'a');
        else if (C >= 'A' && C <= 'F') Value = 10 + (C - 'A');
        else continue;

        OutNumber = (OutNumber << 4) + Value;
    }

    return OutNumber;
}

FIntVector ADojoCraftIslandManager::HexToIntVector(const FString& Source)
{
    if (Source.Len() < 34) return FIntVector::ZeroValue;

    auto ParseHex = [&](int32 Start) {
        return HexToInteger(Source.Mid(Start, 10)) - 2048 + 1;
    };

    int32 X = ParseHex(4);
    int32 Y = ParseHex(14);
    int32 Z = ParseHex(24);

    return FIntVector(X, Y, Z);
}

void ADojoCraftIslandManager::HandleInventory(UDojoModel* Object)
{
    if (!Object) return;

    // Try casting the object to your inventory type
    UDojoModelCraftIslandPocketInventory* Inventory = Cast<UDojoModelCraftIslandPocketInventory>(Object);
    if (!Inventory) return;

    // Check if this is the current player
    if (IsCurrentPlayer())
    {
        // Store the current inventory
        CurrentInventory = Inventory;
        
        // Update ghost preview when inventory changes
        UpdateGhostPreview();

        // Get GameInstance and cast to your custom subclass
        UGameInstance* GameInstance = GetGameInstance();
        if (!GameInstance) return;

        // Cast directly to the custom GameInstance class
        UCraftIslandGameInst* CraftIslandGI = Cast<UCraftIslandGameInst>(GameInstance);
        if (CraftIslandGI)
        {
            CraftIslandGI->UpdateInventory.Broadcast(Inventory);
        }
    }
}

void ADojoCraftIslandManager::HandlePlayerData(UDojoModel* Object)
{
    if (!Object) return;

    UDojoModelCraftIslandPocketPlayerData* PlayerData = Cast<UDojoModelCraftIslandPocketPlayerData>(Object);
    if (!PlayerData) return;

    UE_LOG(LogTemp, VeryVerbose, TEXT("Received PlayerData - Player: %s, Space: %s:%d"),
        *PlayerData->Player, *PlayerData->CurrentSpaceOwner, PlayerData->CurrentSpaceId);

    // Check if this is the current player
    if (IsCurrentPlayer())
    {
        // Check if space has changed
        bool bSpaceChanged = (CurrentSpaceOwner != PlayerData->CurrentSpaceOwner ||
                             CurrentSpaceId != PlayerData->CurrentSpaceId);

        if (bSpaceChanged)
        {
            // Handle the space transition
            HandleSpaceTransition(PlayerData);
        }

        // Get GameInstance and cast to your custom subclass
        UGameInstance* GameInstance = GetGameInstance();
        if (!GameInstance) return;

        // Cast directly to the custom GameInstance class
        UCraftIslandGameInst* CraftIslandGI = Cast<UCraftIslandGameInst>(GameInstance);
        if (CraftIslandGI)
        {
            CraftIslandGI->UpdatePlayerData.Broadcast(PlayerData);
        }
    }
    else
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("HandlePlayerData: Not current player, ignoring update"));
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("========== HandlePlayerData END =========="));
}

bool ADojoCraftIslandManager::IsCurrentPlayer() const
{
    // Placeholder logic — replace this with your actual check
    return true;
}

FIntVector ADojoCraftIslandManager::GetWorldPositionFromLocal(int Position, const FIntVector& ChunkOffset)
{
    int32 LocalX = (Position % 4) + (ChunkOffset.X * 4) + 8192;
    int32 LocalY = ((Position / 4) % 4) + (ChunkOffset.Y * 4) + 8192;
    int32 LocalZ = ((Position / 16) % 4) + (ChunkOffset.Z * 4) + 8192;

    return FIntVector(LocalX, LocalY, LocalZ);
}

void ADojoCraftIslandManager::HandleDojoModel(UDojoModel* Model)
{
    UE_LOG(LogTemp, VeryVerbose, TEXT("HandleDojoModel: Processing model %s"), *Model->DojoModelType);
    FString Name = Model->DojoModelType;

    // First, update the chunk cache
    UCraftIslandChunks::HandleCraftIslandModel(Model, ChunkCache);

    // Then process for immediate display if it's for the current space
    if (Name == "craft_island_pocket-IslandChunk") {
        UDojoModelCraftIslandPocketIslandChunk* Chunk = Cast<UDojoModelCraftIslandPocketIslandChunk>(Model);
        if (Chunk && Chunk->IslandOwner == CurrentSpaceOwner && Chunk->IslandId == CurrentSpaceId)
        {
            ProcessIslandChunk(Chunk);
        }
    }
    else if (Name == "craft_island_pocket-GatherableResource") {
        UDojoModelCraftIslandPocketGatherableResource* Resource = Cast<UDojoModelCraftIslandPocketGatherableResource>(Model);
        if (Resource && Resource->IslandOwner == CurrentSpaceOwner && Resource->IslandId == CurrentSpaceId)
        {
            ProcessGatherableResource(Resource);
        }
    }
    else if (Name == "craft_island_pocket-WorldStructure") {
        UDojoModelCraftIslandPocketWorldStructure* Structure = Cast<UDojoModelCraftIslandPocketWorldStructure>(Model);
        if (Structure && Structure->IslandOwner == CurrentSpaceOwner && Structure->IslandId == CurrentSpaceId)
        {
            ProcessWorldStructure(Structure);
        }
    }
    else if (Name == "craft_island_pocket-Inventory") {
        HandleInventory(Model);
    }
    else if (Name == "craft_island_pocket-PlayerData") {
        HandlePlayerData(Model);
    }

    if (!bLoaded)
    {
        bLoaded = true;

        // Start 1.5-second delay
        GetWorld()->GetTimerManager().SetTimer(
            DelayTimerHandle,
            this,
            &ADojoCraftIslandManager::OnUIDelayedLoad,
            1.5f,
            false
        );
    }
}

void ADojoCraftIslandManager::OnUIDelayedLoad() {
    if (UI)
    {
        UI->CallFunctionByNameWithArguments(TEXT("Loaded"), *GLog, nullptr, true);
    }
}

void ADojoCraftIslandManager::CraftIslandSpawn()
{
    DojoHelpers->CallCraftIslandPocketActionsSpawn(Account);
}

AActor* ADojoCraftIslandManager::PlaceAssetInWorld(E_Item Item, const FIntVector& DojoPosition, bool Validated, EActorSpawnType SpawnType)
{
    if (Item == E_Item::None) {
        return nullptr;
    }
    // Check if there's already an actor at this position
    if (Actors.Contains(DojoPosition))
    {
        AActor* ExistingActor = Actors[DojoPosition];
        if (ExistingActor)
        {
            // Check if it's the same item
            if (ABaseBlock* ExistingBlock = Cast<ABaseBlock>(ExistingActor))
            {
                if (ExistingBlock->Item == Item)
                {
                    // If we're in space 1 and the actor is hidden, show it
                    if (CurrentSpaceOwner == Account.Address && CurrentSpaceId == 1 && ExistingActor->IsHidden())
                    {
                        ExistingActor->SetActorHiddenInGame(false);
                        ExistingActor->SetActorEnableCollision(true);
                    }
                    // Same item, do nothing else
                    return ExistingActor;
                }
            }

            // Different item, remove the existing actor
            if (IsValid(ExistingActor))
            {
                ExistingActor->Destroy();
            }
            Actors.Remove(DojoPosition);
            ActorSpawnInfo.Remove(DojoPosition);
        }
    }

    TSubclassOf<AActor> SpawnClass = nullptr;

    const void* RowPtr = DataTableHelpers::FindRowByColumnValue<int32>(ItemDataTable, FName("Index"), static_cast<int>(Item));
    if (RowPtr)
    {
        const FItemDataRow* Row = static_cast<const FItemDataRow*>(RowPtr);
        if (Row)
        {
            SpawnClass = Row->ActorClass;
        }
        else
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("No matching row found in DataTable"));
        }
    }

    if (!SpawnClass) return nullptr;

    FTransform SpawnTransform = this->DojoPositionToTransform(DojoPosition);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(SpawnClass, SpawnTransform, SpawnParams);
    if (!SpawnedActor) {
        return nullptr;
    }

    Actors.Add(DojoPosition, SpawnedActor);
    ActorSpawnInfo.Add(DojoPosition, FActorSpawnInfo(SpawnedActor, SpawnType));

    if (ABaseBlock* Block = (Cast<ABaseBlock>(SpawnedActor)))
    {
        Block->DojoPosition = DojoPosition;
        Block->Item = Item;
        Block->IsOnChain = Validated;
    }
    return SpawnedActor;
}

int32 ADojoCraftIslandManager::DojoPositionToLocalPosition(const FIntVector& DojoPosition)
{
    int32 XMod = DojoPosition.X % 4;
    int32 YMod = DojoPosition.Y % 4;
    int32 ZMod = DojoPosition.Z % 4;

    int32 LocalIndex = (XMod * 1) + (YMod * 4) + (ZMod * 16);
    return LocalIndex;
}

FTransform ADojoCraftIslandManager::DojoPositionToTransform(const FIntVector& DojoPosition)
{
    const FVector PosAsFloat = FVector(DojoPosition); // Converts ints to float
    const FVector Location = (PosAsFloat - FVector(8192.0f)) * 50.0f;

    return FTransform(FRotator::ZeroRotator, Location, FVector::OneVector);
}

FIntVector ADojoCraftIslandManager::HexStringToVector(const FString& Source)
{
    FString XStr = Source.Mid(4, 10);
    FString YStr = Source.Mid(14, 10);
    FString ZStr = Source.Mid(24, 10);

    int32 X = FParse::HexNumber(*XStr) - 2048;
    int32 Y = FParse::HexNumber(*YStr) - 2048;
    int32 Z = FParse::HexNumber(*ZStr) - 2048;

    return FIntVector(X, Y, Z);
}

// Craft Island Game Inst functions

void ADojoCraftIslandManager::RequestPlaceUse()
{
    // Check if there's an actor at the target position
    int32 X = FMath::TruncToInt32((float)(TargetBlock.X + 8192));
    int32 Y = FMath::TruncToInt32((float)(TargetBlock.Y + 8192));
    int32 Z = FMath::TruncToInt32((float)(TargetBlock.Z + 8192));
    FIntVector TargetPosition(X, Y, Z);

    bool bActorExists = Actors.Contains(TargetPosition);

    // Check if the actor is a completed world structure
    if (bActorExists)
    {
        AActor* TargetActor = Actors[TargetPosition];
        if (ABaseWorldStructure* WorldStructure = Cast<ABaseWorldStructure>(TargetActor))
        {
            if (WorldStructure->WorldStructure && WorldStructure->WorldStructure->Completed)
            {
                // This is a completed world structure, visit its linked space
                int LinkedSpaceId = WorldStructure->WorldStructure->LinkedSpaceId;
                if (LinkedSpaceId > 0)
                {
                    // Store the structure type before visiting
                    CurrentSpaceStructureType = WorldStructure->WorldStructure->StructureType;
                    UE_LOG(LogTemp, Log, TEXT("RequestPlaceUse: Visiting linked space %d (structure type %d)"),
                        LinkedSpaceId, CurrentSpaceStructureType);
                    DojoHelpers->CallCraftIslandPocketActionsVisit(Account, LinkedSpaceId);
                    return;
                }
            }
        }
    }

    // Get current selected item to check if it's a block or structure
    int32 SelectedItemId = GetSelectedItemId();

    // Determine Z offset based on item type
    int32 ZOffset = 0;
    
    // Calculate the actual Z position we're targeting
    int32 ActualZ = TargetBlock.Z + 8192;
    
    
    // Blocks (1-3) must be at z=0
    // World structures (50, 60-64) should be placed one level up
    // Other items can stack on blocks
    if (SelectedItemId == 50 || (SelectedItemId >= 60 && SelectedItemId <= 64))
    {
        // World structures always go one level up
        // If targeting ground level (z=0), place at z=1
        if (ActualZ == 8192)
        {
            ZOffset = 1;
        }
    }
    else if (SelectedItemId > 3 && TargetBlock.Z == 0 && bActorExists)
    {
        // Other non-block items can stack on existing blocks
        ZOffset = 1;
    }


    DojoHelpers->CallCraftIslandPocketActionsUseItem(
        Account,
        TargetBlock.X + 8192,
        TargetBlock.Y + 8192,
        TargetBlock.Z + 8192 + ZOffset
    );
}

void ADojoCraftIslandManager::RequestSpawn()
{
    DojoHelpers->CallCraftIslandPocketActionsSpawn(Account);
}

void ADojoCraftIslandManager::InventorySelectHotbar(UObject* Slot)
{
    if (!Slot) return;

    int32 SlotIndex = 0;

    UClass* ObjClass = Slot->GetClass();
    FProperty* Prop = ObjClass->FindPropertyByName("Index");

    if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
    {
        SlotIndex = IntProp->GetPropertyValue_InContainer(Slot);
    }

    DojoHelpers->CallCraftIslandPocketActionsSelectHotbarSlot(Account, SlotIndex);
    
    // Update ghost preview when switching items
    UpdateGhostPreview();
}

void ADojoCraftIslandManager::RequestHit()
{
    DojoHelpers->CallCraftIslandPocketActionsHitBlock(Account, TargetBlock.X + 8192, TargetBlock.Y + 8192, TargetBlock.Z + 8192, 1);
}

void ADojoCraftIslandManager::RequestInventoryMoveItem(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot)
{
    DojoHelpers->CallCraftIslandPocketActionsInventoryMoveItem(Account, FromInventory, FromSlot, ToInventory, ToSlot);
}

void ADojoCraftIslandManager::RequestCraft(int32 Item)
{
    DojoHelpers->CallCraftIslandPocketActionsCraft(Account, Item, TargetBlock.X, TargetBlock.Y, TargetBlock.Z);
}

void ADojoCraftIslandManager::RequestGiveItem()
{
    DojoHelpers->CallCraftIslandPocketAdminGiveSelf(Account, CurrentItemId, 1);
}

void ADojoCraftIslandManager::RequestSell()
{
    DojoHelpers->CallCraftIslandPocketActionsSell(Account);
}

void ADojoCraftIslandManager::RequestBuy(int32 ItemId, int32 Quantity)
{
    DojoHelpers->CallCraftIslandPocketActionsBuy(Account, ItemId, Quantity);
}

void ADojoCraftIslandManager::RequestGoBackHome()
{
    UE_LOG(LogTemp, VeryVerbose, TEXT("========== RequestGoBackHome START =========="));
    UE_LOG(LogTemp, VeryVerbose, TEXT("RequestGoBackHome: Current space: %s:%d"),
        *CurrentSpaceOwner, CurrentSpaceId);
    UE_LOG(LogTemp, VeryVerbose, TEXT("RequestGoBackHome: Account.Address = %s"), *Account.Address);
    UE_LOG(LogTemp, VeryVerbose, TEXT("RequestGoBackHome: bSpace1ActorsHidden = %s"), bSpace1ActorsHidden ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, VeryVerbose, TEXT("RequestGoBackHome: Actors.Num() = %d"), Actors.Num());

    if (DojoHelpers)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("RequestGoBackHome: Calling visit with space_id = 1"));
        DojoHelpers->CallCraftIslandPocketActionsVisit(Account, 1);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("RequestGoBackHome: DojoHelpers is null!"));
    }
    UE_LOG(LogTemp, VeryVerbose, TEXT("========== RequestGoBackHome END =========="));
}

void ADojoCraftIslandManager::SetTargetBlock(FVector Location)
{
    // Check if we have a building pattern selected
    int32 SelectedItemId = GetSelectedItemId();
    if (IsBuildingPattern(SelectedItemId))
    {
        // For buildings, always target north of player
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            if (APawn* PlayerPawn = PC->GetPawn())
            {
                FVector PlayerLocation = PlayerPawn->GetActorLocation();
                
                // Calculate position one block north of player
                // In this coordinate system, X is north/south
                Location = PlayerLocation;
                Location.X -= 50.0f; // One block north (negative X)
                
                // Snap to grid
                Location.X = FMath::RoundToFloat(Location.X / 50.0f) * 50.0f;
                Location.Y = FMath::RoundToFloat(Location.Y / 50.0f) * 50.0f;
                Location.Z = 0.0f; // Buildings go on ground level
            }
        }
    }
    
    TargetBlock.X = Location.X;
    TargetBlock.Y = Location.Y;
    TargetBlock.Z = Location.Z;
    
    // Update ghost preview position if it exists
    if (GhostPreviewActor && IsBuildingPattern(SelectedItemId))
    {
        FVector GhostLocation(TargetBlock);
        GhostLocation.Z = 50.0f; // One block above ground for preview
        GhostPreviewActor->SetActorLocation(GhostLocation);
    }
}

// Helper function implementations

FString ADojoCraftIslandManager::GetCurrentIslandKey() const
{
    // Use the tracked current space for the cache key
    return MakeSpaceKey(CurrentSpaceOwner, CurrentSpaceId);
}

FString ADojoCraftIslandManager::MakeSpaceKey(const FString& Owner, int32 Id) const
{
    return Owner + FString::FromInt(Id);
}

void ADojoCraftIslandManager::SaveCurrentPlayerPosition()
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (APawn* PlayerPawn = PC->GetPawn())
        {
            FString CurrentSpaceKey = GetCurrentIslandKey();
            FVector CurrentPos = PlayerPawn->GetActorLocation();
            SpacePlayerPositions.Add(CurrentSpaceKey, CurrentPos);
            UE_LOG(LogTemp, Log, TEXT("Saved position %s for space %s"),
                *CurrentPos.ToString(), *CurrentSpaceKey);
        }
    }
}

FVector ADojoCraftIslandManager::GetSpawnPositionForSpace(const FString& SpaceKey, bool bHasBlockChunks)
{
    // Check if we have a saved position for this space
    if (SpacePlayerPositions.Contains(SpaceKey))
    {
        FVector SavedPos = SpacePlayerPositions[SpaceKey];
        UE_LOG(LogTemp, Log, TEXT("Restoring saved position %s for space %s"),
            *SavedPos.ToString(), *SpaceKey);
        return SavedPos;
    }

    // Return default position based on space type
    FVector DefaultPos = bHasBlockChunks ? DEFAULT_OUTDOOR_SPAWN_POS : DEFAULT_BUILDING_SPAWN_POS;
    UE_LOG(LogTemp, Log, TEXT("Using default position %s for new space %s"),
        *DefaultPos.ToString(), *SpaceKey);
    return DefaultPos;
}

void ADojoCraftIslandManager::SetCameraLag(APawn* Pawn, bool bEnableLag)
{
    if (!Pawn) return;

    // Try to find the SpringArm component
    TArray<UActorComponent*> Components = Pawn->GetComponents().Array();
    for (UActorComponent* Component : Components)
    {
        if (Component && Component->GetName().Contains(TEXT("SpringArm")))
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("Found SpringArm component: %s"), *Component->GetName());

            // Try to set properties using reflection
            UObject* CompObj = Cast<UObject>(Component);
            if (CompObj)
            {
                // Set bEnableCameraLag
                FBoolProperty* CameraLagProp = FindFProperty<FBoolProperty>(CompObj->GetClass(), TEXT("bEnableCameraLag"));
                if (CameraLagProp)
                {
                    CameraLagProp->SetPropertyValue_InContainer(CompObj, bEnableLag);
                }

                // Set bEnableCameraRotationLag
                FBoolProperty* RotationLagProp = FindFProperty<FBoolProperty>(CompObj->GetClass(), TEXT("bEnableCameraRotationLag"));
                if (RotationLagProp)
                {
                    RotationLagProp->SetPropertyValue_InContainer(CompObj, bEnableLag);
                }

                UE_LOG(LogTemp, VeryVerbose, TEXT("Set camera lag to %s"), bEnableLag ? TEXT("enabled") : TEXT("disabled"));
                break;
            }
        }
    }
}

int32 ADojoCraftIslandManager::GetSelectedItemId() const
{
    if (!CurrentInventory) 
    {
        UE_LOG(LogTemp, Warning, TEXT("GetSelectedItemId: CurrentInventory is null"));
        return 0;
    }

    int32 SelectedSlot = CurrentInventory->HotbarSelectedSlot;
    if (SelectedSlot >= 36) return 0; // Max 36 slots (9 per felt252)
    
    UE_LOG(LogTemp, Warning, TEXT("GetSelectedItemId: SelectedSlot=%d"), SelectedSlot);

    // Determine which slot field to use
    int32 FeltIndex = SelectedSlot / 9;
    int32 SlotInFelt = SelectedSlot % 9;

    FString SlotData;
    switch (FeltIndex)
    {
        case 0: SlotData = CurrentInventory->Slots1; break;
        case 1: SlotData = CurrentInventory->Slots2; break;
        case 2: SlotData = CurrentInventory->Slots3; break;
        case 3: SlotData = CurrentInventory->Slots4; break;
        default: return 0;
    }

    // Convert hex string to number
    if (SlotData.StartsWith("0x"))
    {
        SlotData = SlotData.Mid(2);
    }

    // Parse the hex string to a big integer
    // We need to handle this as a 256-bit number
    TArray<uint8> Bytes;
    
    // Convert hex string to bytes
    for (int32 i = 0; i < SlotData.Len(); i += 2)
    {
        uint8 Byte = 0;
        
        // High nibble
        TCHAR C1 = (i < SlotData.Len()) ? SlotData[i] : '0';
        if (C1 >= '0' && C1 <= '9') Byte = (C1 - '0') << 4;
        else if (C1 >= 'a' && C1 <= 'f') Byte = (10 + (C1 - 'a')) << 4;
        else if (C1 >= 'A' && C1 <= 'F') Byte = (10 + (C1 - 'A')) << 4;
        
        // Low nibble
        TCHAR C2 = (i + 1 < SlotData.Len()) ? SlotData[i + 1] : '0';
        if (C2 >= '0' && C2 <= '9') Byte |= (C2 - '0');
        else if (C2 >= 'a' && C2 <= 'f') Byte |= (10 + (C2 - 'a'));
        else if (C2 >= 'A' && C2 <= 'F') Byte |= (10 + (C2 - 'A'));
        
        Bytes.Add(Byte);
    }
    
    // Cairo stores slots packed from LSB to MSB in the felt252
    // Each slot is 28 bits: [10 bits item_type][8 bits quantity][10 bits extra]
    // Slot 0 starts at bit 0, slot 1 at bit 28, etc.
    
    // Pad bytes array to 32 bytes if needed
    while (Bytes.Num() < 32)
    {
        Bytes.Insert(0, 0); // Pad at the beginning since hex is big-endian
    }
    
    // Calculate bit position for the selected slot
    uint32 BitOffset = SlotInFelt * 28;
    
    // Extract 28 bits from the bytes array
    // Since felt252 is big-endian but slots are packed from LSB, we need to
    // work from the right side of the number
    uint64 SlotDataValue = 0;
    
    // Read from the end of the array (LSB) and extract bits
    for (int32 i = 0; i < 5; i++) // Read 5 bytes to ensure we get all 28 bits
    {
        int32 ByteIndex = 31 - (BitOffset / 8) - i;
        if (ByteIndex >= 0 && ByteIndex < Bytes.Num())
        {
            SlotDataValue |= ((uint64)Bytes[ByteIndex]) << (i * 8);
        }
    }
    
    // Shift to align with the bit offset within the bytes
    SlotDataValue >>= (BitOffset % 8);
    
    // Mask to get only 28 bits
    SlotDataValue &= 0x0FFFFFFF;
    
    // Extract item_type (bits 18-27, top 10 bits of the 28-bit value)
    int32 ItemType = (SlotDataValue >> 18) & 0x3FF;
    
    // Extract quantity for debugging
    int32 Quantity = (SlotDataValue >> 10) & 0xFF;
    

    return ItemType;
}

void ADojoCraftIslandManager::DisableCameraLagDuringTeleport(APawn* Pawn)
{
    if (!Pawn) return;

    // Disable camera lag
    SetCameraLag(Pawn, false);

    // Re-enable after a short delay
    FTimerHandle ReenableLagTimer;
    GetWorld()->GetTimerManager().SetTimer(ReenableLagTimer, [this, Pawn]()
    {
        SetCameraLag(Pawn, true);
    }, CAMERA_LAG_REENABLE_DELAY, false);
}

void ADojoCraftIslandManager::TeleportPlayer(const FVector& NewLocation, bool bImmediate)
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (APawn* PlayerPawn = PC->GetPawn())
        {
            DisableCameraLagDuringTeleport(PlayerPawn);
            PlayerPawn->SetActorLocation(NewLocation);
            UE_LOG(LogTemp, Log, TEXT("Teleported player to %s"), *NewLocation.ToString());
        }
    }
}

void ADojoCraftIslandManager::HandleSpaceTransition(UDojoModelCraftIslandPocketPlayerData* PlayerData)
{
    if (!PlayerData) return;

    UE_LOG(LogTemp, Log, TEXT("Space changed from %s:%d to %s:%d"),
        *CurrentSpaceOwner, CurrentSpaceId,
        *PlayerData->CurrentSpaceOwner, PlayerData->CurrentSpaceId);

    // Save current player position before changing spaces
    SaveCurrentPlayerPosition();

    // Check if we're returning to space 1
    bool bReturningToSpace1 = (PlayerData->CurrentSpaceOwner == Account.Address && PlayerData->CurrentSpaceId == 1);

    // Clear all current actors when changing spaces
    ClearAllSpawnedActors();

    // Update current space tracking
    CurrentSpaceOwner = PlayerData->CurrentSpaceOwner;
    CurrentSpaceId = PlayerData->CurrentSpaceId;

    // Reset structure type if returning to main space
    if (bReturningToSpace1)
    {
        CurrentSpaceStructureType = 0;
    }

    // If returning to space 1 and actors are hidden, show them
    if (bReturningToSpace1 && bSpace1ActorsHidden)
    {
        SetActorsVisibilityAndCollision(true, true);
        bSpace1ActorsHidden = false;
        UE_LOG(LogTemp, Log, TEXT("Restored space 1 actors"));
    }

    // Track if current space has block chunks
    bool bHasBlockChunks = false;

    // Only load chunks if we're not returning to space 1 with hidden actors
    if (!(bReturningToSpace1 && !bSpace1ActorsHidden))
    {
        // Load chunks from cache for the new space
        FString NewIslandKey = GetCurrentIslandKey();

        if (ChunkCache.Contains(NewIslandKey))
        {
            FSpaceChunks& SpaceData = ChunkCache[NewIslandKey];
            UE_LOG(LogTemp, Log, TEXT("Found cache for space %s with %d chunks"), *NewIslandKey, SpaceData.Chunks.Num());

            // Check if there are any block chunks
            for (const auto& ChunkPair : SpaceData.Chunks)
            {
                if (ChunkPair.Value && (ChunkPair.Value->Blocks1 != "0x0" || ChunkPair.Value->Blocks2 != "0x0"))
                {
                    bHasBlockChunks = true;
                    break;
                }
            }

            if (bHasBlockChunks)
            {
                UE_LOG(LogTemp, Log, TEXT("Loading cached data for space %s"), *NewIslandKey);
                LoadAllChunksFromCache();
            }
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("No cached data for space %s"), *NewIslandKey);
        }
    }
    else
    {
        // For space 1, check if it has chunks
        bHasBlockChunks = !Actors.IsEmpty();
    }

    // Spawn default building if no block chunks exist
    if (!bHasBlockChunks && DefaultBuildingClass)
    {
        FVector SpawnLocation(0, 0, 0);
        FRotator SpawnRotation(0, 90, 0);
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        DefaultBuilding = GetWorld()->SpawnActor<AActor>(DefaultBuildingClass, SpawnLocation, SpawnRotation, SpawnParams);
        if (DefaultBuilding)
        {
            UE_LOG(LogTemp, Log, TEXT("Spawned default building (structure type %d)"), CurrentSpaceStructureType);

            // Show/hide workshop components based on structure type
            TArray<UActorComponent*> Components = DefaultBuilding->GetComponents().Array();
            for (UActorComponent* Component : Components)
            {
                if (Component)
                {
                    FString ComponentName = Component->GetName();

                    // Check if this is a workshop-specific component
                    if (ComponentName.Contains(TEXT("Workshop")) ||
                        ComponentName.Contains(TEXT("B_WoodWorkshop")) ||
                        ComponentName.Contains(TEXT("B_Workshop")))
                    {
                        // Show if structure type is 60 (Workshop), hide otherwise
                        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
                        if (PrimComp)
                        {
                            bool bShouldBeVisible = (CurrentSpaceStructureType == 60);
                            PrimComp->SetVisibility(bShouldBeVisible);
                            PrimComp->SetCollisionEnabled(bShouldBeVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
                            UE_LOG(LogTemp, Log, TEXT("Component %s visibility=%s, collision=%s"),
                                *ComponentName,
                                bShouldBeVisible ? TEXT("true") : TEXT("false"),
                                bShouldBeVisible ? TEXT("enabled") : TEXT("disabled"));
                        }
                    }
                }
            }
        }
    }

    // Hide or show sky atmosphere based on whether we're in a building
    if (SkyAtmosphere)
    {
        if (bHasBlockChunks)
        {
            SkyAtmosphere->SetActorHiddenInGame(false);
            UE_LOG(LogTemp, Log, TEXT("Showing SkyAtmosphere for outdoor space"));
        }
        else
        {
            SkyAtmosphere->SetActorHiddenInGame(true);
            UE_LOG(LogTemp, Log, TEXT("Hiding SkyAtmosphere for indoor space"));
        }
    }

    // Handle player teleportation
    FString NewSpaceKey = GetCurrentIslandKey();
    FVector NewLocation = GetSpawnPositionForSpace(NewSpaceKey, bHasBlockChunks);
    TeleportPlayer(NewLocation, bReturningToSpace1);
}

void ADojoCraftIslandManager::SetActorsVisibilityAndCollision(bool bVisible, bool bEnableCollision)
{
    for (auto& Pair : Actors)
    {
        if (Pair.Value && IsValid(Pair.Value))
        {
            Pair.Value->SetActorHiddenInGame(!bVisible);
            Pair.Value->SetActorEnableCollision(bEnableCollision);
        }
    }
}

void ADojoCraftIslandManager::ClearAllSpawnedActors()
{
    // Remove ghost preview when clearing actors
    RemoveGhostPreview();
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("========== ClearAllSpawnedActors START =========="));
    UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: CurrentSpaceOwner = %s, CurrentSpaceId = %d"),
        *CurrentSpaceOwner, CurrentSpaceId);
    UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: Account.Address = %s"), *Account.Address);

    // Check if we're currently in space 1
    bool bLeavingSpace1 = (CurrentSpaceOwner == Account.Address && CurrentSpaceId == 1);

    UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: bLeavingSpace1 = %s"), bLeavingSpace1 ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: Owner comparison '%s' == '%s' = %s"),
        *CurrentSpaceOwner, *Account.Address,
        (CurrentSpaceOwner == Account.Address) ? TEXT("true") : TEXT("false"));

    if (bLeavingSpace1)
    {
        // Hide space 1 actors instead of destroying them
        UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: Hiding %d space 1 actors"), Actors.Num());
        SetActorsVisibilityAndCollision(false, false);
        bSpace1ActorsHidden = true;
        UE_LOG(LogTemp, Log, TEXT("ClearAllSpawnedActors: Hiding space 1 actors"));
    }
    else
    {
        // If we have hidden space 1 actors, don't destroy them!
        if (bSpace1ActorsHidden)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: Keeping %d hidden space 1 actors"), Actors.Num());
            // Don't clear Actors or ActorSpawnInfo - they belong to space 1
        }
        else
        {
            // Destroy actors from other spaces
            UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: Destroying %d actors from other space"), Actors.Num());
            for (auto& Pair : Actors)
            {
                if (Pair.Value && IsValid(Pair.Value))
                {
                    Pair.Value->Destroy();
                }
            }
            Actors.Empty();

            // Clear actor spawn info
            ActorSpawnInfo.Empty();
        }
    }

    // Always clear spawn queue
    SpawnQueue.Empty();

    // Destroy default building if it exists
    if (DefaultBuilding && IsValid(DefaultBuilding))
    {
        DefaultBuilding->Destroy();
        DefaultBuilding = nullptr;
        UE_LOG(LogTemp, Log, TEXT("ClearAllSpawnedActors: Destroyed default building"));
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("ClearAllSpawnedActors: After processing - bSpace1ActorsHidden = %s, Actors.Num() = %d"),
        bSpace1ActorsHidden ? TEXT("true") : TEXT("false"), Actors.Num());
    UE_LOG(LogTemp, VeryVerbose, TEXT("========== ClearAllSpawnedActors END =========="));
}

void ADojoCraftIslandManager::QueueSpawnWithOverflowProtection(const FSpawnQueueData& SpawnData)
{
    const int32 MaxSpawnQueueSize = 1000;
    if (SpawnQueue.Num() >= MaxSpawnQueueSize)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Spawn queue full (%d), removing oldest entry"), SpawnQueue.Num());
        SpawnQueue.RemoveAt(0);
    }
    SpawnQueue.Add(SpawnData);
}

void ADojoCraftIslandManager::QueueSpawnBatchWithOverflowProtection(const TArray<FSpawnQueueData>& SpawnDataBatch)
{
    if (SpawnDataBatch.Num() == 0) return;

    const int32 MaxSpawnQueueSize = 1000;
    if (SpawnQueue.Num() + SpawnDataBatch.Num() >= MaxSpawnQueueSize)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Spawn queue approaching limit (%d/%d), removing oldest entries"),
            SpawnQueue.Num(), MaxSpawnQueueSize);

        // Remove oldest entries to make room
        int32 NumToRemove = (SpawnQueue.Num() + SpawnDataBatch.Num()) - MaxSpawnQueueSize + 1;
        SpawnQueue.RemoveAt(0, NumToRemove);
    }
    SpawnQueue.Append(SpawnDataBatch);
}

void ADojoCraftIslandManager::RemoveActorAtPosition(const FIntVector& DojoPosition, EActorSpawnType RequiredType)
{
    if (!Actors.Contains(DojoPosition)) return;

    if (ActorSpawnInfo.Contains(DojoPosition))
    {
        FActorSpawnInfo SpawnInfo = ActorSpawnInfo[DojoPosition];
        if (SpawnInfo.SpawnType == RequiredType)
        {
            AActor* ToRemove = Actors[DojoPosition];
            if (IsValid(ToRemove))
            {
                ToRemove->Destroy();
            }
            Actors.Remove(DojoPosition);
            ActorSpawnInfo.Remove(DojoPosition);
        }
    }
}

void ADojoCraftIslandManager::ProcessChunkBlock(uint8 Byte, const FIntVector& DojoPosition, E_Item Item, TArray<FSpawnQueueData>& ChunkSpawnData)
{
    if (Byte == 0)
    {
        RemoveActorAtPosition(DojoPosition, EActorSpawnType::ChunkBlock);
    }
    else if (Item != E_Item::None)
    {
        ChunkSpawnData.Add(FSpawnQueueData(Item, DojoPosition, false));
    }
}

void ADojoCraftIslandManager::ProcessIslandChunk(UDojoModelCraftIslandPocketIslandChunk* Chunk)
{
    if (!Chunk) return;

    UE_LOG(LogTemp, Log, TEXT("ProcessIslandChunk: Checking chunk %s, owner=%s, currentOwner=%s, account=%s"),
        *Chunk->ChunkId, *Chunk->IslandOwner, *CurrentSpaceOwner, *Account.Address);

    // When loading from cache, we should check against CurrentSpaceOwner instead of Account.Address
    if (Chunk->IslandOwner != CurrentSpaceOwner)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("ProcessIslandChunk: Skipping chunk due to owner mismatch"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("ProcessIslandChunk: Processing chunk %s with blocks1=%s, blocks2=%s"),
        *Chunk->ChunkId, *Chunk->Blocks1, *Chunk->Blocks2);

    FString Blocks = Chunk->Blocks1.Mid(2) + Chunk->Blocks2.Mid(2);

    // Validate chunk data length
    if (Blocks.Len() != 64)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid chunk data length: %d, expected 64 for chunk %s"),
            Blocks.Len(), *Chunk->ChunkId);
        return;
    }

    FString SubStr = Blocks.Reverse();
    FIntVector ChunkOffset = HexStringToVector(Chunk->ChunkId);

    // Process chunk data and batch add to queue
    TArray<FSpawnQueueData> ChunkSpawnData;

    int32 Index = 0;
    for (TCHAR Char : SubStr)
    {
        uint8 Byte = FParse::HexDigit(Char);
        E_Item Item = static_cast<E_Item>(Byte);
        FIntVector DojoPos = GetWorldPositionFromLocal(Index, ChunkOffset);

        ProcessChunkBlock(Byte, DojoPos, Item, ChunkSpawnData);
        Index++;
    }

    QueueSpawnBatchWithOverflowProtection(ChunkSpawnData);
}

void ADojoCraftIslandManager::ProcessGatherableResource(UDojoModelCraftIslandPocketGatherableResource* Gatherable)
{
    if (!Gatherable || Gatherable->IslandOwner != CurrentSpaceOwner) return;

    FIntVector ChunkOffset = HexStringToVector(Gatherable->ChunkId);
    E_Item Item = static_cast<E_Item>(Gatherable->ResourceId);
    FIntVector DojoPos = GetWorldPositionFromLocal(Gatherable->Position, ChunkOffset);

    if (Item == E_Item::None)
    {
        RemoveActorAtPosition(DojoPos, EActorSpawnType::GatherableResource);
    }
    else
    {
        QueueSpawnWithOverflowProtection(FSpawnQueueData(Item, DojoPos, false, Gatherable));
    }
}

void ADojoCraftIslandManager::ProcessWorldStructure(UDojoModelCraftIslandPocketWorldStructure* Structure)
{
    if (!Structure || Structure->IslandOwner != CurrentSpaceOwner) return;

    FIntVector ChunkOffset = HexStringToVector(Structure->ChunkId);
    E_Item Item = static_cast<E_Item>(Structure->StructureType);
    FIntVector DojoPos = GetWorldPositionFromLocal(Structure->Position, ChunkOffset);

    if (Item == E_Item::None)
    {
        RemoveActorAtPosition(DojoPos, EActorSpawnType::WorldStructure);
    }
    else
    {
        // Check if actor already exists at this position (it's an update)
        AActor* ExistingActor = nullptr;
        if (Actors.Contains(DojoPos))
        {
            ExistingActor = Actors[DojoPos];
        }

        if (ExistingActor)
        {
            // This is an update to an existing structure
            ABaseWorldStructure* WorldStructureActor = Cast<ABaseWorldStructure>(ExistingActor);
            if (WorldStructureActor)
            {
                // Update the structure data
                WorldStructureActor->WorldStructure = Structure;

                // Check if it just became completed
                if (Structure->Completed && !WorldStructureActor->Constructed)
                {
                    WorldStructureActor->NotifyConstructionCompleted();
                    UE_LOG(LogTemp, Log, TEXT("ProcessWorldStructure: Structure completed at position (%d, %d, %d), calling OnConstructed"), DojoPos.X, DojoPos.Y, DojoPos.Z);
                }
            }
        }
        else
        {
            // New structure, queue for spawn
            UE_LOG(LogTemp, Log, TEXT("ProcessWorldStructure: Queueing spawn for Item %d at position (%d, %d, %d)"), static_cast<int32>(Item), DojoPos.X, DojoPos.Y, DojoPos.Z);
            QueueSpawnWithOverflowProtection(FSpawnQueueData(Item, DojoPos, false, Structure));
        }
    }
}

void ADojoCraftIslandManager::LoadChunkFromCache(const FString& ChunkId)
{
    FString IslandKey = GetCurrentIslandKey();
    if (!ChunkCache.Contains(IslandKey)) return;

    FSpaceChunks& SpaceData = ChunkCache[IslandKey];

    // Load chunk blocks
    if (SpaceData.Chunks.Contains(ChunkId))
    {
        UE_LOG(LogTemp, Log, TEXT("LoadChunkFromCache: Found chunk %s, processing it"), *ChunkId);
        ProcessIslandChunk(SpaceData.Chunks[ChunkId]);
    }
    else
    {
        // Log the first few attempts to see what we're looking for vs what's in cache
        static int LogCount = 0;
        if (LogCount < 5)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("LoadChunkFromCache: Chunk %s not found in cache. Available chunks:"), *ChunkId);
            for (const auto& ChunkPair : SpaceData.Chunks)
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("  - %s"), *ChunkPair.Key);
            }
            LogCount++;
        }
    }

    // Load gatherables for this chunk
    int32 GatherableCount = 0;
    for (const auto& Pair : SpaceData.Gatherables)
    {
        if (Pair.Value && Pair.Value->ChunkId == ChunkId)
        {
            ProcessGatherableResource(Pair.Value);
            GatherableCount++;
        }
    }
    if (GatherableCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("LoadChunkFromCache: Processed %d gatherables for chunk %s"), GatherableCount, *ChunkId);
    }

    // Load structures for this chunk
    int32 StructureCount = 0;
    for (const auto& Pair : SpaceData.Structures)
    {
        if (Pair.Value && Pair.Value->ChunkId == ChunkId)
        {
            ProcessWorldStructure(Pair.Value);
            StructureCount++;
        }
    }
    if (StructureCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("LoadChunkFromCache: Processed %d structures for chunk %s"), StructureCount, *ChunkId);
    }
}

void ADojoCraftIslandManager::LoadAllChunksFromCache()
{
    FString IslandKey = GetCurrentIslandKey();
    if (!ChunkCache.Contains(IslandKey)) return;

    FSpaceChunks& SpaceData = ChunkCache[IslandKey];

    UE_LOG(LogTemp, Log, TEXT("LoadAllChunksFromCache: Loading all chunks for key %s"), *IslandKey);

    // Load all chunks
    int32 ChunksLoaded = 0;
    for (const auto& ChunkPair : SpaceData.Chunks)
    {
        if (ChunkPair.Value)
        {
            ProcessIslandChunk(ChunkPair.Value);
            ChunksLoaded++;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("LoadAllChunksFromCache: Loaded %d chunks"), ChunksLoaded);

    // Load all gatherables
    int32 GatherablesLoaded = 0;
    for (const auto& Pair : SpaceData.Gatherables)
    {
        if (Pair.Value)
        {
            ProcessGatherableResource(Pair.Value);
            GatherablesLoaded++;
        }
    }
    if (GatherablesLoaded > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("LoadAllChunksFromCache: Loaded %d gatherables"), GatherablesLoaded);
    }

    // Load all structures
    int32 StructuresLoaded = 0;
    for (const auto& Pair : SpaceData.Structures)
    {
        if (Pair.Value)
        {
            ProcessWorldStructure(Pair.Value);
            StructuresLoaded++;
        }
    }
    if (StructuresLoaded > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("LoadAllChunksFromCache: Loaded %d structures"), StructuresLoaded);
    }
}

bool ADojoCraftIslandManager::IsBuildingPattern(int32 ItemId) const
{
    // Building patterns: House (50), Workshop (60), Well (61), Kitchen (62), Warehouse (63), Brewery (64)
    return ItemId == 50 || (ItemId >= 60 && ItemId <= 64);
}

void ADojoCraftIslandManager::UpdateGhostPreview()
{
    // Remove existing ghost if any
    RemoveGhostPreview();
    
    // Get selected item
    int32 SelectedItemId = GetSelectedItemId();
    UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: SelectedItemId = %d"), SelectedItemId);
    
    // Only show ghost for building patterns
    if (!IsBuildingPattern(SelectedItemId))
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Not a building pattern, returning"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Is building pattern, proceeding"));
    
    // Get player position and calculate north position
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (APawn* PlayerPawn = PC->GetPawn())
        {
            FVector PlayerLocation = PlayerPawn->GetActorLocation();
            
            // Calculate position one block north of player
            // In this coordinate system, X is north/south
            FVector GhostLocation = PlayerLocation;
            GhostLocation.X -= 50.0f; // One block north (negative X, 50 units = 1 block)
            
            // Snap to grid
            GhostLocation.X = FMath::RoundToFloat(GhostLocation.X / 50.0f) * 50.0f;
            GhostLocation.Y = FMath::RoundToFloat(GhostLocation.Y / 50.0f) * 50.0f;
            GhostLocation.Z = FMath::RoundToFloat(GhostLocation.Z / 50.0f) * 50.0f;
            
            // Ensure it's above ground
            GhostLocation.Z += 50.0f; // One block above current position
            
            UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Player at %s, Ghost at %s"), 
                   *PlayerLocation.ToString(), *GhostLocation.ToString());
            
            // Get the actor class for this item
            if (!ItemDataTable) 
            {
                UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: ItemDataTable is null"));
                return;
            }
            
            const FItemDataRow* ItemData = reinterpret_cast<const FItemDataRow*>(
                DataTableHelpers::FindRowByColumnValue(ItemDataTable, TEXT("ID"), SelectedItemId)
            );
            
            if (!ItemData || !ItemData->ActorClass) 
            {
                UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: ItemData not found or no ActorClass for ID %d"), SelectedItemId);
                return;
            }
            
            UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Found ActorClass for ID %d"), SelectedItemId);
            
            // Spawn the ghost actor
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            
            GhostPreviewActor = GetWorld()->SpawnActor<AActor>(ItemData->ActorClass, GhostLocation, FRotator::ZeroRotator, SpawnParams);
            
            if (GhostPreviewActor)
            {
                UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Ghost actor spawned at %s"), *GhostLocation.ToString());
                
                // Set the whole actor to be translucent
                GhostPreviewActor->SetActorHiddenInGame(false);
                GhostPreviewActor->SetActorEnableCollision(false);
                
                // Make it semi-transparent and ghostly
                TArray<UPrimitiveComponent*> Components;
                GhostPreviewActor->GetComponents<UPrimitiveComponent>(Components);
                
                UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Found %d components"), Components.Num());
                
                for (UPrimitiveComponent* Component : Components)
                {
                    // Disable collision
                    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    
                    // Make visible but translucent
                    Component->SetVisibility(true);
                    Component->SetHiddenInGame(false);
                    
                    // Make semi-transparent
                    if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Processing StaticMeshComponent"));
                        
                        // Simple approach - just modify render settings
                        MeshComp->SetRenderCustomDepth(true);
                        MeshComp->SetCustomDepthStencilValue(252); // Special value for ghost
                        
                        // Try to make it translucent through actor opacity if available
                        MeshComp->SetScalarParameterValueOnMaterials(TEXT("Opacity"), 0.5f);
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("UpdateGhostPreview: Failed to spawn ghost actor"));
            }
        }
    }
}

void ADojoCraftIslandManager::RemoveGhostPreview()
{
    if (GhostPreviewActor)
    {
        GhostPreviewActor->Destroy();
        GhostPreviewActor = nullptr;
    }
}
