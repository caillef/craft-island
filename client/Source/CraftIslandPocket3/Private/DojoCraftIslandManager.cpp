// Fill out your copyright notice in the Description page of Project Settings.


#include "DojoCraftIslandManager.h"

// Sets default values
ADojoCraftIslandManager::ADojoCraftIslandManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

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

    // Step 4: Subscribe to dojo model updates
    DojoHelpers->SubscribeOnDojoModelUpdate();

    // Step 1: Create burner account
    Account = DojoHelpers->CreateBurnerDeprecated(RpcUrl, PlayerAddress, PrivateKey);

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

    ConnectGameInstanceEvents();
}

void ADojoCraftIslandManager::ConnectGameInstanceEvents()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    UCraftIslandGameInst* CraftIslandGI = Cast<UCraftIslandGameInst>(GameInstance);
    if (!CraftIslandGI) return;

    CraftIslandGI->RequestPlaceUse.AddDynamic(this, &ADojoCraftIslandManager::RequestPlaceUse);
    CraftIslandGI->RequestSpawn.AddDynamic(this, &ADojoCraftIslandManager::RequestSpawn);
    CraftIslandGI->UpdateInventory.AddDynamic(this, &ADojoCraftIslandManager::UpdateInventory);
    CraftIslandGI->InventorySelectHotbar.AddDynamic(this, &ADojoCraftIslandManager::InventorySelectHotbar);
    CraftIslandGI->RequestHit.AddDynamic(this, &ADojoCraftIslandManager::RequestHit);
    CraftIslandGI->RequestInventoryMoveItem.AddDynamic(this, &ADojoCraftIslandManager::RequestInventoryMoveItem);
    CraftIslandGI->RequestCraft.AddDynamic(this, &ADojoCraftIslandManager::RequestCraft);
//    CraftIslandGI->RequestGiveItem.AddDynamic(this, &ADojoCraftIslandManager::RequestGiveItem);
    CraftIslandGI->SetTargetBlock.AddDynamic(this, &ADojoCraftIslandManager::SetTargetBlock);
}

void ADojoCraftIslandManager::ContinueAfterDelay()
{
    // Step 4: Fetch existing models
    if (DojoHelpers)
    {
        DojoHelpers->FetchExistingModels();
    }
}

void ADojoCraftIslandManager::InitUIAndActors()
{
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

// Called every frame
void ADojoCraftIslandManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    // Step 1: Get Player Pawn and Cast
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

    // Step 3: Convert to IntVector, truncate, and modify Z
    int32 X = FMath::TruncToInt(TargetLocation.X + 8192);
    int32 Y = FMath::TruncToInt(TargetLocation.Y + 8192);
    int32 Z = FMath::TruncToInt(TargetLocation.Z + 8192);

    FIntVector TestVector(X, Y, Z);

    // Step 4: Check if in Actors array
    bool bFound = Actors.Contains(TestVector);

    // Step 5: Select Z value based on presence
    int32 ZValue = bFound ? 0 : -1;

    // Step 6: Rebuild target vector
    FVector FinalTargetVector(TargetLocation.X, TargetLocation.Y, TargetLocation.Z + ZValue);

    // Step 7: Set on GameInstance
    if (UGameInstance* GI = GetGameInstance())
    {
        UCraftIslandGameInst* CI = Cast<UCraftIslandGameInst>(GI);
        if (CI)
        {
            CI->SetTargetBlock.Broadcast(FinalTargetVector);
        }
    }

    // Step 8: Set ActionDojoPosition
    ActionDojoPosition = FIntVector(X, Y, ZValue);

    // Step 9: Process spawn queue - spawn up to 10 actors per tick
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
                if (ABaseWorldStructure* ActorStructure = Cast<ABaseWorldStructure>(SpawnedActor))
                {
                    ActorStructure->WorldStructure = Structure;
                }
            }
        }

        SpawnsProcessed++;
    }

    if (SpawnsProcessed > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Processed %d spawns this frame, %d items remaining in queue"), SpawnsProcessed, SpawnQueue.Num());
    }
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
    FString Name = Model->DojoModelType;

    // First, update the chunk cache
    UCraftIslandChunks::HandleCraftIslandModel(Model, ChunkCache);

    // Then process for immediate display
    if (Name == "craft_island_pocket-IslandChunk") {
        ProcessIslandChunk(Cast<UDojoModelCraftIslandPocketIslandChunk>(Model));
    }
    else if (Name == "craft_island_pocket-GatherableResource") {
        ProcessGatherableResource(Cast<UDojoModelCraftIslandPocketGatherableResource>(Model));
    }
    else if (Name == "craft_island_pocket-WorldStructure") {
        ProcessWorldStructure(Cast<UDojoModelCraftIslandPocketWorldStructure>(Model));
    }
    else if (Name == "craft_island_pocket-Inventory") {
        HandleInventory(Model);
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
                    // Same item, do nothing
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
            UE_LOG(LogTemp, Warning, TEXT("No matching row found in DataTable"));
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
    DojoHelpers->CallCraftIslandPocketActionsUseItem(Account, TargetBlock.X + 8192, TargetBlock.Y + 8192, TargetBlock.Z + 8192);
}

void ADojoCraftIslandManager::RequestSpawn()
{
    DojoHelpers->CallCraftIslandPocketActionsSpawn(Account);
}

void ADojoCraftIslandManager::UpdateInventory(UDojoModelCraftIslandPocketInventory* Inventory)
{
    // Up to you — usually this is UI-side, no server call needed
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

void ADojoCraftIslandManager::SetTargetBlock(FVector Location)
{
    TargetBlock.X = Location.X;
    TargetBlock.Y = Location.Y;
    TargetBlock.Z = Location.Z;
}

// Helper function implementations

FString ADojoCraftIslandManager::GetCurrentIslandKey() const
{
    // For now, we'll use player address + island id 0
    // You may want to make this configurable or get from game state
    return Account.Address + FString::FromInt(0);
}

void ADojoCraftIslandManager::QueueSpawnWithOverflowProtection(const FSpawnQueueData& SpawnData)
{
    const int32 MaxSpawnQueueSize = 1000;
    if (SpawnQueue.Num() >= MaxSpawnQueueSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("Spawn queue full (%d), removing oldest entry"), SpawnQueue.Num());
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
        UE_LOG(LogTemp, Warning, TEXT("Spawn queue approaching limit (%d/%d), removing oldest entries"), 
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
    if (!Chunk || Chunk->IslandOwner != Account.Address) return;
    
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
    if (!Gatherable || Gatherable->IslandOwner != Account.Address) return;
    
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
    if (!Structure || Structure->IslandOwner != Account.Address) return;
    
    FIntVector ChunkOffset = HexStringToVector(Structure->ChunkId);
    E_Item Item = static_cast<E_Item>(Structure->StructureType);
    FIntVector DojoPos = GetWorldPositionFromLocal(Structure->Position, ChunkOffset);
    
    if (Item == E_Item::None)
    {
        RemoveActorAtPosition(DojoPos, EActorSpawnType::WorldStructure);
    }
    else
    {
        QueueSpawnWithOverflowProtection(FSpawnQueueData(Item, DojoPos, false, Structure));
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
        ProcessIslandChunk(SpaceData.Chunks[ChunkId]);
    }
    
    // Load gatherables for this chunk
    for (const auto& Pair : SpaceData.Gatherables)
    {
        if (Pair.Value && Pair.Value->ChunkId == ChunkId)
        {
            ProcessGatherableResource(Pair.Value);
        }
    }
    
    // Load structures for this chunk
    for (const auto& Pair : SpaceData.Structures)
    {
        if (Pair.Value && Pair.Value->ChunkId == ChunkId)
        {
            ProcessWorldStructure(Pair.Value);
        }
    }
}

void ADojoCraftIslandManager::LoadChunksFromCache(const FIntVector& CenterChunk, int32 Radius)
{
    FString IslandKey = GetCurrentIslandKey();
    if (!ChunkCache.Contains(IslandKey)) return;
    
    // Load all chunks within radius
    for (int32 X = -Radius; X <= Radius; X++)
    {
        for (int32 Y = -Radius; Y <= Radius; Y++)
        {
            for (int32 Z = -Radius; Z <= Radius; Z++)
            {
                FIntVector ChunkPos = CenterChunk + FIntVector(X, Y, Z);
                
                // Convert chunk position to chunk ID format (0x + 10 hex chars for each coordinate)
                FString ChunkId = FString::Printf(TEXT("0x%010x%010x%010x"), 
                    ChunkPos.X + 2048, 
                    ChunkPos.Y + 2048, 
                    ChunkPos.Z + 2048);
                
                LoadChunkFromCache(ChunkId);
            }
        }
    }
}
