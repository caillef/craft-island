// Fill out your copyright notice in the Description page of Project Settings.


#include "DojoCraftIslandManager.h"
#include "EngineUtils.h"
#include "BaseObject.h"
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

	// Initialize local hotbar selection to -1 (unset)
	LocalHotbarSelectedSlot = -1;
	LocalHotbarSelectionTimestamp = 0.0;
	LocalHotbarSelectionSequence = 0;
	NextHotbarSequence = 1;
	bHotbarSelectionPending = false;

	// Initialize transaction queue
	bIsProcessingTransaction = false;
	TransactionQueueCount = 0;
	
	// Start optimistic cleanup timer
	StartOptimisticCleanupTimer();
}

// Static function to get DojoManager instance from Blueprint
ADojoCraftIslandManager* ADojoCraftIslandManager::GetDojoManager(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetDojoManager: WorldContext is null"));
        return nullptr;
    }

    UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetDojoManager: Could not get World from WorldContext"));
        return nullptr;
    }

    // Find the DojoCraftIslandManager in the world
    UE_LOG(LogTemp, Log, TEXT("GetDojoManager: Searching for DojoManager in world"));
    for (TActorIterator<ADojoCraftIslandManager> ActorItr(World); ActorItr; ++ActorItr)
    {
        ADojoCraftIslandManager* DojoManager = *ActorItr;
        if (DojoManager)
        {
            UE_LOG(LogTemp, Log, TEXT("GetDojoManager: Found DojoManager, DojoHelpers available: %s"), 
                DojoManager->DojoHelpers ? TEXT("Yes") : TEXT("No"));
            return DojoManager;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("GetDojoManager: No DojoCraftIslandManager found in world"));
    return nullptr;
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
    Account = DojoHelpers->CreateAccountDeprecated(RpcUrl, PlayerAddress, PrivateKey);

    // Initialize current space tracking
    CurrentSpaceOwner = Account.Address;
    CurrentSpaceId = 1;
    
    // Set player address in UI if it exists
    if (UI)
    {
        // Call the Blueprint function SetPlayerAddress on the UI widget
        UFunction* SetPlayerAddressFunc = UI->FindFunction(FName("SetPlayerAddress"));
        if (SetPlayerAddressFunc)
        {
            struct FSetPlayerAddressParams
            {
                FString Address;
            };
            
            FSetPlayerAddressParams Params;
            Params.Address = Account.Address;
            
            UI->ProcessEvent(SetPlayerAddressFunc, &Params);
            UE_LOG(LogTemp, Log, TEXT("Called SetPlayerAddress on UI with address: %s"), *Account.Address);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SetPlayerAddress function not found on UI widget"));
        }
    }

    // Initialize default building to nullptr
    DefaultBuilding = nullptr;

    // Initialize space 1 actors tracking
    bSpace1ActorsHidden = false;
    bFirstPlayerDataReceived = false;

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
    CraftIslandGI->RequestStartProcessing.AddDynamic(this, &ADojoCraftIslandManager::RequestStartProcessing);
    CraftIslandGI->RequestCancelProcessing.AddDynamic(this, &ADojoCraftIslandManager::RequestCancelProcessing);
    
    // Note: Queue delegates are already bound in GameInstance::Init() - no double binding needed
}

void ADojoCraftIslandManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);


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

    if (!SpawnQueue.IsEmpty() && SpawnsProcessed == 0)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("=== Processing Spawn Queue: %d items ==="), SpawnQueue.Num());
    }

    while (!SpawnQueue.IsEmpty() && SpawnsProcessed < MaxSpawnsPerFrame)
    {
        FSpawnQueueData SpawnData = SpawnQueue[0];
        SpawnQueue.RemoveAt(0);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("Processing spawn: Item=%d, Position=(%d,%d,%d)"), 
            (int32)SpawnData.Item, 
            SpawnData.DojoPosition.X, SpawnData.DojoPosition.Y, SpawnData.DojoPosition.Z);

        EActorSpawnType SpawnType = EActorSpawnType::ChunkBlock;
        if (SpawnData.DojoModel)
        {
            if (Cast<UDojoModelCraftIslandPocketGatherableResource>(SpawnData.DojoModel))
            {
                SpawnType = EActorSpawnType::GatherableResource;
                UE_LOG(LogTemp, VeryVerbose, TEXT("Spawn type: GatherableResource"));
            }
            else if (Cast<UDojoModelCraftIslandPocketWorldStructure>(SpawnData.DojoModel))
            {
                SpawnType = EActorSpawnType::WorldStructure;
                UE_LOG(LogTemp, VeryVerbose, TEXT("Spawn type: WorldStructure"));
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
                    if (Gatherable->Tier == 1)
                    {
                        // Change materials to gold for tier 1 resources
                        TArray<UActorComponent*> Components = ActorObject->GetComponents().Array();
                        for (UActorComponent* Component : Components)
                        {
                            if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Component))
                            {
                                for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
                                {
                                    UMaterialInterface* CurrentMaterial = MeshComp->GetMaterial(i);
                                    if (CurrentMaterial)
                                    {
                                        FString MaterialName = CurrentMaterial->GetName();
                                        if (MaterialName.Contains(TEXT("Leaves")) && GoldLeavesMaterial)
                                        {
                                            MeshComp->SetMaterial(i, GoldLeavesMaterial);
                                        }
                                        else if (GoldMaterial)
                                        {
                                            MeshComp->SetMaterial(i, GoldMaterial);
                                        }
                                    }
                                }
                            }
                        }

                        // Also check child components
                        TArray<USceneComponent*> ChildComponents;
                        ActorObject->GetRootComponent()->GetChildrenComponents(true, ChildComponents);
                        for (USceneComponent* ChildComp : ChildComponents)
                        {
                            if (UMeshComponent* MeshComp = Cast<UMeshComponent>(ChildComp))
                            {
                                for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
                                {
                                    UMaterialInterface* CurrentMaterial = MeshComp->GetMaterial(i);
                                    if (CurrentMaterial)
                                    {
                                        FString MaterialName = CurrentMaterial->GetName();
                                        if (MaterialName.Contains(TEXT("Leaves")) && GoldLeavesMaterial)
                                        {
                                            MeshComp->SetMaterial(i, GoldLeavesMaterial);
                                        }
                                        else if (GoldMaterial)
                                        {
                                            MeshComp->SetMaterial(i, GoldMaterial);
                                        }
                                    }
                                }
                            }
                        }
                    }
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
        // Store the hotbar inventory (Id == 0) for getting selected item
        if (Inventory->Id == 0)
        {
            CurrentInventory = Inventory;
            // Simplified synchronization with lazy hotbar system
            if (LocalHotbarSelectedSlot == -1)
            {
                // No local selection - sync with server
                LocalHotbarSelectedSlot = Inventory->HotbarSelectedSlot;
                LocalHotbarSelectionTimestamp = GetWorld()->GetTimeSeconds();
                bHotbarSelectionPending = false;
                UE_LOG(LogTemp, Warning, TEXT("OnInventoryUpdated: Syncing LocalHotbarSelectedSlot to server value: %d"), 
                       Inventory->HotbarSelectedSlot);
            }
            else if (LocalHotbarSelectedSlot == Inventory->HotbarSelectedSlot)
            {
                // Server confirmed our selection - clear pending flag
                bHotbarSelectionPending = false;
                GetWorld()->GetTimerManager().ClearTimer(HotbarTimeoutHandle);
                UE_LOG(LogTemp, Warning, TEXT("OnInventoryUpdated: Server confirmed slot %d"), 
                       LocalHotbarSelectedSlot);
            }
            else
            {
                // Keep local state and pending flag - will be sent with next action
                UE_LOG(LogTemp, Warning, TEXT("OnInventoryUpdated: Local slot %d still pending (server has %d)"), 
                       LocalHotbarSelectedSlot, Inventory->HotbarSelectedSlot);
                // bHotbarSelectionPending remains true
            }
        }

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
        // Check if space has changed - normalize addresses to handle leading zeros
        auto GetHexPart = [](const FString& Address) -> FString {
            FString Hex = Address.StartsWith("0x") ? Address.Mid(2) : Address;
            return Hex.ToLower();
        };
        
        FString CurrentOwnerHex = GetHexPart(CurrentSpaceOwner);
        FString PlayerDataOwnerHex = GetHexPart(PlayerData->CurrentSpaceOwner);
        
        bool bOwnerChanged = !(CurrentOwnerHex.EndsWith(PlayerDataOwnerHex) || PlayerDataOwnerHex.EndsWith(CurrentOwnerHex));
        bool bSpaceChanged = (bOwnerChanged || CurrentSpaceId != PlayerData->CurrentSpaceId);

        // For the first player data, force initial loading
        if (!bFirstPlayerDataReceived)
        {
            bFirstPlayerDataReceived = true;
            UE_LOG(LogTemp, Log, TEXT("First PlayerData: force loading space %s:%d"), 
                   *PlayerData->CurrentSpaceOwner, PlayerData->CurrentSpaceId);
            
            // Update current space and force initial load
            CurrentSpaceOwner = PlayerData->CurrentSpaceOwner;
            CurrentSpaceId = PlayerData->CurrentSpaceId;
            
            // Force initial chunk loading for starting space
            LoadAllChunksFromCache();
        }
        else if (bSpaceChanged)
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

void ADojoCraftIslandManager::HandleProcessingLock(UDojoModel* Object)
{
    if (!Object) return;

    UDojoModelCraftIslandPocketProcessingLock* ProcessingLock = Cast<UDojoModelCraftIslandPocketProcessingLock>(Object);
    if (!ProcessingLock) return;

    UE_LOG(LogTemp, Log, TEXT("Received ProcessingLock - Player: %s, UnlockTime: %lld, ProcessType: %d"),
        *ProcessingLock->Player, ProcessingLock->UnlockTime, ProcessingLock->ProcessType);

    // Check if this is the current player
    if (ProcessingLock->Player == Account.Address)
    {
        // Update our local processing lock state
        CurrentProcessingLock.Player = ProcessingLock->Player;
        CurrentProcessingLock.UnlockTime = ProcessingLock->UnlockTime;
        CurrentProcessingLock.ProcessType = ProcessingLock->ProcessType;
        CurrentProcessingLock.BatchesProcessed = ProcessingLock->BatchesProcessed;

        // Broadcast update for UI
        UCraftIslandGameInst* CraftIslandGI = Cast<UCraftIslandGameInst>(GetGameInstance());
        if (CraftIslandGI)
        {
            CraftIslandGI->UpdateProcessingLock.Broadcast(ProcessingLock);
        }

        UE_LOG(LogTemp, Log, TEXT("Updated CurrentProcessingLock for player"));
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
    UE_LOG(LogTemp, VeryVerbose, TEXT("=== HandleDojoModel START ==="));
    UE_LOG(LogTemp, VeryVerbose, TEXT("Model Type: %s"), *Model->DojoModelType);
    FString Name = Model->DojoModelType;

    // When we receive any model update, it means a transaction was processed
    // Continue processing the queue
    OnTransactionComplete();

    // First, update the chunk cache
    UCraftIslandChunks::HandleCraftIslandModel(Model, ChunkCache);

    // Then process for immediate display if it's for the current space
    if (Name == "craft_island_pocket-IslandChunk") {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Processing IslandChunk model"));
        UDojoModelCraftIslandPocketIslandChunk* Chunk = Cast<UDojoModelCraftIslandPocketIslandChunk>(Model);
        if (Chunk)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("Chunk Owner: %s, Id: %d | Current Space: %s, Id: %d"), 
                *Chunk->IslandOwner, Chunk->IslandId, *CurrentSpaceOwner, CurrentSpaceId);
            if (Chunk->IslandOwner == CurrentSpaceOwner && Chunk->IslandId == CurrentSpaceId)
            {
                ProcessIslandChunk(Chunk);
            }
        }
    }
    else if (Name == "craft_island_pocket-GatherableResource") {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Processing GatherableResource model"));
        UDojoModelCraftIslandPocketGatherableResource* Resource = Cast<UDojoModelCraftIslandPocketGatherableResource>(Model);
        if (Resource)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("Resource Owner: %s, Id: %d, Position: %d, ResourceId: %d, Tier: %d"), 
                *Resource->IslandOwner, Resource->IslandId, 
                Resource->Position, Resource->ResourceId, Resource->Tier);
            UE_LOG(LogTemp, VeryVerbose, TEXT("Current Space: %s, Id: %d"), *CurrentSpaceOwner, CurrentSpaceId);
            
            if (Resource->IslandOwner == CurrentSpaceOwner && Resource->IslandId == CurrentSpaceId)
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Resource is in current space, processing..."));
                ProcessGatherableResource(Resource);
            }
            else
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Resource is NOT in current space, skipping"));
            }
        }
    }
    else if (Name == "craft_island_pocket-WorldStructure") {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Processing WorldStructure model"));
        UDojoModelCraftIslandPocketWorldStructure* Structure = Cast<UDojoModelCraftIslandPocketWorldStructure>(Model);
        if (Structure)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("Structure Owner: %s, Id: %d | Current Space: %s, Id: %d"), 
                *Structure->IslandOwner, Structure->IslandId, *CurrentSpaceOwner, CurrentSpaceId);
            if (Structure->IslandOwner == CurrentSpaceOwner && Structure->IslandId == CurrentSpaceId)
            {
                ProcessWorldStructure(Structure);
            }
        }
    }
    else if (Name == "craft_island_pocket-Inventory") {
        HandleInventory(Model);
    }
    else if (Name == "craft_island_pocket-PlayerData") {
        HandlePlayerData(Model);
    }
    else if (Name == "craft_island_pocket-ProcessingLock") {
        HandleProcessingLock(Model);
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
    // TODO: Set player name after spawning (disabled for now due to string_to_bytes issue)
    // DojoHelpers->CallCraftIslandPocketActionsSetName(Account, TEXT("caillef"));
}

AActor* ADojoCraftIslandManager::PlaceAssetInWorld(E_Item Item, const FIntVector& DojoPosition, bool Validated, EActorSpawnType SpawnType)
{
    UE_LOG(LogTemp, VeryVerbose, TEXT("=== PlaceAssetInWorld START ==="));
    UE_LOG(LogTemp, VeryVerbose, TEXT("Item: %d, Position: (%d,%d,%d), Validated: %d, SpawnType: %d"), 
        (int32)Item, DojoPosition.X, DojoPosition.Y, DojoPosition.Z, Validated, (int32)SpawnType);
    
    if (Item == E_Item::None) {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Item is None, returning nullptr"));
        return nullptr;
    }

    // Check if there's an optimistic actor at this position that we need to confirm
    if (OptimisticActors.Contains(DojoPosition))
    {
        AActor* OptimisticActor = OptimisticActors[DojoPosition];
        if (OptimisticActor && IsValid(OptimisticActor))
        {
            // Check if it's the same item being confirmed
            bool bSameItem = false;
            if (ABaseBlock* OptimisticBlock = Cast<ABaseBlock>(OptimisticActor))
            {
                bSameItem = (OptimisticBlock->Item == Item);
            }
            else if (ABaseObject* OptimisticObject = Cast<ABaseObject>(OptimisticActor))
            {
                bSameItem = (OptimisticObject->Item == Item);
            }

            if (bSameItem)
            {
                // Confirm the optimistic placement
                RemovePendingVisual(OptimisticActor);
                OptimisticActors.Remove(DojoPosition);
                OptimisticActorTimestamps.Remove(DojoPosition);

                // Move from optimistic to confirmed
                if (!Actors.Contains(DojoPosition))
                {
                    Actors.Add(DojoPosition, OptimisticActor);
                    ActorSpawnInfo.Add(DojoPosition, FActorSpawnInfo(OptimisticActor, SpawnType));
                }

                UE_LOG(LogTemp, Log, TEXT("Confirmed optimistic placement at (%d, %d, %d)"),
                    DojoPosition.X, DojoPosition.Y, DojoPosition.Z);
                return OptimisticActor;
            }
            else
            {
                // Wrong item - rollback optimistic and place correct one
                RollbackOptimisticAction(DojoPosition);
            }
        }
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
            UE_LOG(LogTemp, VeryVerbose, TEXT("Found actor class for item %d"), (int32)Item);
        }
        else
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("No matching row found in DataTable"));
        }
    }
    else
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("DataTableHelpers::FindRowByColumnValue returned nullptr"));
    }

    if (!SpawnClass)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnClass is null, cannot spawn actor"));
        return nullptr;
    }

    FTransform SpawnTransform = this->DojoPositionToTransform(DojoPosition);
    UE_LOG(LogTemp, VeryVerbose, TEXT("Spawn transform: Location=(%f,%f,%f)"), 
        SpawnTransform.GetLocation().X, SpawnTransform.GetLocation().Y, SpawnTransform.GetLocation().Z);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(SpawnClass, SpawnTransform, SpawnParams);
    if (!SpawnedActor) {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn actor"));
        return nullptr;
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("Successfully spawned actor: %s"), *SpawnedActor->GetName());
    
    Actors.Add(DojoPosition, SpawnedActor);
    ActorSpawnInfo.Add(DojoPosition, FActorSpawnInfo(SpawnedActor, SpawnType));

    if (ABaseBlock* Block = (Cast<ABaseBlock>(SpawnedActor)))
    {
        Block->DojoPosition = DojoPosition;
        Block->Item = Item;
        Block->IsOnChain = Validated;
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("=== PlaceAssetInWorld END ==="));
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
    // Queue pending hotbar selection first (lazy evaluation)
    QueuePendingHotbarSelection();
    
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
    
    // If hoe is selected, only allow on grass blocks
    if (SelectedItemId == 41) // Stone Hoe
    {
        if (bActorExists)
        {
            AActor* TargetActor = Actors[TargetPosition];
            if (ABaseBlock* Block = Cast<ABaseBlock>(TargetActor))
            {
                if (Block->Item != E_Item::Grass) // Not grass
                {
                    UE_LOG(LogTemp, Warning, TEXT("Cannot use hoe on %d - only works on grass blocks"), (int32)Block->Item);
                    return;
                }
            }
            else
            {
                // Not a block at all (could be tree, boulder, etc.)
                UE_LOG(LogTemp, Warning, TEXT("Cannot use hoe on non-block object"));
                return;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot use hoe on empty space"));
            return;
        }
    }
    
    // If hammer is selected, only allow on world structures
    if (SelectedItemId == 43) // Stone Hammer
    {
        if (bActorExists)
        {
            AActor* TargetActor = Actors[TargetPosition];
            if (ABaseWorldStructure* WorldStructure = Cast<ABaseWorldStructure>(TargetActor))
            {
                // Allow hammer on world structures (completed or not)
                UE_LOG(LogTemp, Warning, TEXT("Hammer can be used on world structure"));
            }
            else
            {
                // Not a world structure
                UE_LOG(LogTemp, Warning, TEXT("Cannot use hammer on non-structure object"));
                return;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot use hammer on empty space"));
            return;
        }
    }
    
    int32 ZOffset = 0;

    // Calculate the actual Z position we're targeting
    int32 ActualZ = TargetBlock.Z + 8192;

    // Check if using rock (33) on another rock (33)
    if (SelectedItemId == 33 && bActorExists)
    {
        AActor* TargetActor = Actors[TargetPosition];
        if (ABaseObject* Object = Cast<ABaseObject>(TargetActor))
        {
            // Check if it's a rock (E_Item::Rock = 33)
            if (Object->Item == E_Item::Rock)
            {
                // Store the rock's position for stone crafting
                StoneCraftTargetBlock = TargetPosition;
                UE_LOG(LogTemp, Warning, TEXT("Stone craft: Stored rock position (%d,%d,%d)"), 
                    StoneCraftTargetBlock.X, StoneCraftTargetBlock.Y, StoneCraftTargetBlock.Z);
                
                // Open stone craft interface
                if (StoneCraftInterfaceWidgetClass)
                {
                    UUserWidget* StoneCraftUI = CreateWidget<UUserWidget>(GetWorld(), StoneCraftInterfaceWidgetClass);
                    if (StoneCraftUI)
                    {
                        StoneCraftUI->AddToViewport();
                        UE_LOG(LogTemp, Log, TEXT("Opened Stone Craft Interface"));
                    }
                }
                return;
            }
        }
    }

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

    // Check if this is a tool that modifies blocks rather than places them
    bool bIsTool = false;
    int32 ResultItemId = SelectedItemId; // What should appear after using the tool
    
    // Hoe (41) tills grass (1) into dirt (2)
    if (SelectedItemId == 41) // Stone Hoe
    {
        bIsTool = true;
        // Check if targeting grass block at ground level
        if (bActorExists && TargetBlock.Z == 0)
        {
            AActor* TargetActor = Actors[TargetPosition];
            if (ABaseBlock* Block = Cast<ABaseBlock>(TargetActor))
            {
                if (Block->Item == E_Item::Grass) // Grass = 1
                {
                    ResultItemId = 2; // Dirt
                    ZOffset = 0; // Replace at same position, not above
                    UE_LOG(LogTemp, Warning, TEXT("Hoe will till grass into dirt at same position"));
                }
                else
                {
                    // Can't till non-grass blocks
                    bIsTool = false;
                    ResultItemId = 0;
                    UE_LOG(LogTemp, Warning, TEXT("Cannot till block type %d with hoe"), (int32)Block->Item);
                }
            }
        }
        else
        {
            // Can't use hoe on empty space or above ground
            bIsTool = false;
            ResultItemId = 0;
            UE_LOG(LogTemp, Warning, TEXT("Cannot use hoe here - no grass block at ground level"));
        }
    }
    
    // If tool failed (ResultItemId = 0), don't proceed with any actions
    if (bIsTool && ResultItemId == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Tool operation failed - not proceeding"));
        return;
    }
    
    // Optimistic rendering: Place the item/result immediately with visual feedback
    if ((SelectedItemId > 0 && !bIsTool) || (bIsTool && ResultItemId > 0))
    {
        FIntVector OptimisticPosition(
            TargetBlock.X + 8192,
            TargetBlock.Y + 8192,
            TargetBlock.Z + 8192 + ZOffset
        );

        // Don't add optimistic if there's already something pending at this position
        if (!OptimisticActors.Contains(OptimisticPosition))
        {
            // For tools, place the result item, not the tool itself
            int32 ItemToPlace = bIsTool ? ResultItemId : SelectedItemId;
            
            // Check if placing a seed - seeds can only be placed on dirt blocks
            bool bIsSeed = (ItemToPlace == 47 || ItemToPlace == 51 || ItemToPlace == 53); // Wheat, Carrot, Potato seeds
            if (bIsSeed)
            {
                // Check what's at the target position below
                FIntVector GroundPosition(
                    TargetBlock.X + 8192,
                    TargetBlock.Y + 8192,
                    TargetBlock.Z + 8192
                );
                
                if (Actors.Contains(GroundPosition))
                {
                    AActor* GroundActor = Actors[GroundPosition];
                    if (ABaseBlock* GroundBlock = Cast<ABaseBlock>(GroundActor))
                    {
                        if (GroundBlock->Item != E_Item::Dirt) // Only allow on dirt (ID 2)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Cannot place seeds on %d - only on dirt blocks"), (int32)GroundBlock->Item);
                            return; // Don't place optimistically and don't queue transaction
                        }
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Cannot place seeds - no ground block found"));
                    return; // Don't place optimistically and don't queue transaction
                }
            }
            
            // For tools that replace blocks (like hoe), remove the existing actor first
            if (bIsTool && ResultItemId > 0 && ZOffset == 0 && Actors.Contains(OptimisticPosition))
            {
                AActor* ExistingActor = Actors[OptimisticPosition];
                if (ExistingActor && IsValid(ExistingActor))
                {
                    ExistingActor->Destroy();
                    Actors.Remove(OptimisticPosition);
                    ActorSpawnInfo.Remove(OptimisticPosition);
                }
            }
            
            // Spawn the actor immediately (optimistically)
            AActor* OptimisticActor = PlaceAssetInWorld(
                static_cast<E_Item>(ItemToPlace),
                OptimisticPosition,
                false // Not validated yet
            );

            if (OptimisticActor)
            {
                // Store as optimistic with timestamp
                OptimisticActors.Add(OptimisticPosition, OptimisticActor);
                OptimisticActorTimestamps.Add(OptimisticPosition, GetWorld()->GetTimeSeconds());

                // Apply pending visual feedback
                ApplyPendingVisual(OptimisticActor);

                UE_LOG(LogTemp, Log, TEXT("Optimistic placement at (%d, %d, %d) for item %d"),
                    OptimisticPosition.X, OptimisticPosition.Y, OptimisticPosition.Z, ItemToPlace);
            }
        }
    }

    // Queue the transaction instead of calling directly
    FTransactionQueueItem Item;
    Item.Type = ETransactionType::PlaceUse;
    Item.Position = FIntVector(
        TargetBlock.X + 8192,
        TargetBlock.Y + 8192,
        TargetBlock.Z + 8192 + ZOffset
    );
    QueueTransaction(Item);
}

void ADojoCraftIslandManager::RequestSpawn()
{
    DojoHelpers->CallCraftIslandPocketActionsSpawn(Account);
    // TODO: Set player name after spawning (disabled for now due to string_to_bytes issue)
    // DojoHelpers->CallCraftIslandPocketActionsSetName(Account, TEXT("caillef"));
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

    // Store selected slot locally for lazy evaluation - NO immediate blockchain transaction
    LocalHotbarSelectedSlot = SlotIndex;
    LocalHotbarSelectionTimestamp = GetWorld()->GetTimeSeconds();
    
    // Check if this differs from server state
    if (CurrentInventory && LocalHotbarSelectedSlot != CurrentInventory->HotbarSelectedSlot)
    {
        bHotbarSelectionPending = true;
        UE_LOG(LogTemp, Warning, TEXT("Hotbar selection pending: slot %d (server has %d)"), 
               LocalHotbarSelectedSlot, CurrentInventory->HotbarSelectedSlot);
    }
    else
    {
        bHotbarSelectionPending = false;
        UE_LOG(LogTemp, Warning, TEXT("Hotbar selection matches server: slot %d"), LocalHotbarSelectedSlot);
    }
    
    // Log the slot change for debugging
    UE_LOG(LogTemp, Warning, TEXT("=== HOTBAR SELECTION (LAZY) ==="));
    UE_LOG(LogTemp, Warning, TEXT("Selected slot %d, pending=%d, LocalHotbarSelectedSlot now = %d"), 
           SlotIndex, bHotbarSelectionPending, LocalHotbarSelectedSlot);
    
    // Force immediate item ID calculation to verify correct selection
    int32 NewItemId = GetSelectedItemId();
    UE_LOG(LogTemp, Warning, TEXT("Item in slot %d has ID: %d"), SlotIndex, NewItemId);
    
    // NO QueueTransaction() here - will be sent with next action!
}

void ADojoCraftIslandManager::RequestHit()
{
    UE_LOG(LogTemp, Warning, TEXT("=== RequestHit START ==="));
    UE_LOG(LogTemp, Warning, TEXT("TargetBlock: (%d,%d,%d)"), TargetBlock.X, TargetBlock.Y, TargetBlock.Z);
    
    // Check if we have a valid item selected
    int32 SelectedItemId = GetSelectedItemId();
    
    // Hoe (41) should only be used with PlaceUse, not Hit
    if (SelectedItemId == 41)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot use hoe for hitting - use PlaceUse on grass instead"));
        return;
    }
    
    // Queue pending hotbar selection first (lazy evaluation)
    QueuePendingHotbarSelection();
    
    // Optimistic rendering for block/resource removal
    FIntVector HitPosition(
        TargetBlock.X + 8192,
        TargetBlock.Y + 8192,
        TargetBlock.Z + 8192
    );
    
    UE_LOG(LogTemp, Warning, TEXT("HitPosition: (%d,%d,%d)"), HitPosition.X, HitPosition.Y, HitPosition.Z);

    // Check if there's an actor at the hit position
    if (Actors.Contains(HitPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("Found actor at hit position"));
        AActor* ActorToRemove = Actors[HitPosition];
        if (ActorToRemove && IsValid(ActorToRemove))
        {
            // Check if it's a block (BaseBlock) and if player has shovel
            bool bShouldApplyOptimistic = true;
            int32 BlockId = -1; // Default value for non-blocks
            
            if (ABaseBlock* Block = Cast<ABaseBlock>(ActorToRemove))
            {
                // Get the block ID from the E_Item enum
                BlockId = static_cast<int32>(Block->Item);
                UE_LOG(LogTemp, Warning, TEXT("Hit target is a block with ID: %d"), BlockId);
                
                // If it's a block (ID < 16), check if player has shovel
                if (BlockId < 16)
                {
                    // Shovel has ID 39 (Stone Shovel)
                    if (SelectedItemId != 39)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Cannot mine block without shovel. Selected item ID: %d"), SelectedItemId);
                        bShouldApplyOptimistic = false;
                    }
                }
                // If it's a boulder (ID 49), check if player has pickaxe
                else if (BlockId == 49) // Boulder
                {
                    // Stone Pickaxe has ID 37
                    if (SelectedItemId != 37)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Cannot mine boulder without pickaxe. Selected item ID: %d"), SelectedItemId);
                        bShouldApplyOptimistic = false;
                    }
                }
            }
            
            if (bShouldApplyOptimistic)
            {
                // Don't actually remove it yet, just make it appear as if it's being removed
                // Apply a "removal pending" visual effect
                TArray<UActorComponent*> MeshComponents = ActorToRemove->GetComponents().Array();
                for (UActorComponent* Component : MeshComponents)
                {
                    if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component))
                    {
                        // Make it semi-transparent and red-tinted to show it's being removed
                        for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
                        {
                            UMaterialInstanceDynamic* DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
                            if (DynMaterial)
                            {
                                DynMaterial->SetScalarParameterValue(FName("Opacity"), 0.3f);
                                DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor(1.0f, 0.0f, 0.0f)); // Red tint
                            }
                        }

                        // Disable collision immediately
                        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

                        // Scale down slightly (except for boulders)
                        if (BlockId != 49) // Don't scale down boulders
                        {
                            ActorToRemove->SetActorScale3D(FVector(0.9f, 0.9f, 0.9f));
                        }
                    }
                }

                // Tag it as pending removal
                ActorToRemove->Tags.Add(FName("OptimisticRemoval"));

                // Store in optimistic actors for potential rollback with timestamp
                OptimisticActors.Add(HitPosition, ActorToRemove);
                OptimisticActorTimestamps.Add(HitPosition, GetWorld()->GetTimeSeconds());

                UE_LOG(LogTemp, Log, TEXT("Optimistic removal at (%d, %d, %d)"),
                    HitPosition.X, HitPosition.Y, HitPosition.Z);
            }
            else
            {
                // If we don't have the right tool, don't send the hit action at all
                // It would fail on the blockchain anyway, so save the gas
                UE_LOG(LogTemp, Warning, TEXT("Hit blocked: Wrong tool for target. Not sending transaction."));
                return;
            }
        }
    }

    // Queue the transaction instead of calling directly
    FTransactionQueueItem Item;
    Item.Type = ETransactionType::Hit;
    Item.Position = HitPosition;
    
    UE_LOG(LogTemp, Warning, TEXT("Queuing Hit transaction"));
    QueueTransaction(Item);
    
    UE_LOG(LogTemp, Warning, TEXT("=== RequestHit END ==="));
}

void ADojoCraftIslandManager::RequestInventoryMoveItem(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot)
{
    DojoHelpers->CallCraftIslandPocketActionsInventoryMoveItem(Account, FromInventory, FromSlot, ToInventory, ToSlot);
}

void ADojoCraftIslandManager::RequestCraft(int32 Item)
{
    // Queue pending hotbar selection first (lazy evaluation)
    QueuePendingHotbarSelection();
    
    UE_LOG(LogTemp, Warning, TEXT("=== RequestCraft START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Item: %d"), Item);
    UE_LOG(LogTemp, Warning, TEXT("StoneCraftTargetBlock: (%d,%d,%d)"), StoneCraftTargetBlock.X, StoneCraftTargetBlock.Y, StoneCraftTargetBlock.Z);
    UE_LOG(LogTemp, Warning, TEXT("Selected hotbar item: %d"), GetSelectedItemId());
    
    DojoHelpers->CallCraftIslandPocketActionsCraft(Account, Item, StoneCraftTargetBlock.X, StoneCraftTargetBlock.Y, StoneCraftTargetBlock.Z);
    
    UE_LOG(LogTemp, Warning, TEXT("=== RequestCraft END ==="));
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

void ADojoCraftIslandManager::RequestStartProcessing(uint8 ProcessType, int32 InputAmount)
{
    UE_LOG(LogTemp, Log, TEXT("RequestStartProcessing: ProcessType=%d, InputAmount=%d"), ProcessType, InputAmount);

    if (DojoHelpers)
    {
        DojoHelpers->CallCraftIslandPocketActionsStartProcessing(Account, ProcessType, InputAmount);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("RequestStartProcessing: DojoHelpers is null!"));
    }
}

void ADojoCraftIslandManager::RequestCancelProcessing()
{
    UE_LOG(LogTemp, Log, TEXT("RequestCancelProcessing"));

    if (DojoHelpers)
    {
        DojoHelpers->CallCraftIslandPocketActionsCancelProcessing(Account);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("RequestCancelProcessing: DojoHelpers is null!"));
    }
}

void ADojoCraftIslandManager::StartProcessing(uint8 ProcessType, int32 InputAmount)
{
    RequestStartProcessing(ProcessType, InputAmount);
}

void ADojoCraftIslandManager::CancelProcessing()
{
    RequestCancelProcessing();
}

void ADojoCraftIslandManager::ToggleProcessingUI(uint8 ProcessType, bool bShow)
{
    UE_LOG(LogTemp, Log, TEXT("ToggleProcessingUI: ProcessType=%d, bShow=%s"), ProcessType, bShow ? TEXT("true") : TEXT("false"));

    UCraftIslandGameInst* GameInst = Cast<UCraftIslandGameInst>(GetGameInstance());
    if (GameInst)
    {
        GameInst->ToggleLockUI.Broadcast(ProcessType, bShow);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ToggleProcessingUI: GameInstance is null or not UCraftIslandGameInst"));
    }
}

void ADojoCraftIslandManager::ToggleCraftUI(bool bShow)
{
    UE_LOG(LogTemp, Log, TEXT("ToggleCraftUI: bShow=%s"), bShow ? TEXT("true") : TEXT("false"));

    UCraftIslandGameInst* GameInst = Cast<UCraftIslandGameInst>(GetGameInstance());
    if (GameInst)
    {
        GameInst->ToggleCraftUI.Broadcast(bShow);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ToggleCraftUI: GameInstance is null or not UCraftIslandGameInst"));
    }
}

bool ADojoCraftIslandManager::IsPlayerProcessing() const
{
    // Get current timestamp (this would need to be synced with blockchain time)
    int64 CurrentTime = FDateTime::UtcNow().ToUnixTimestamp();
    return CurrentProcessingLock.UnlockTime > CurrentTime;
}

float ADojoCraftIslandManager::GetProcessingTimeRemaining() const
{
    if (!IsPlayerProcessing())
    {
        return 0.0f;
    }

    int64 CurrentTime = FDateTime::UtcNow().ToUnixTimestamp();
    int64 TimeRemaining = CurrentProcessingLock.UnlockTime - CurrentTime;

    return FMath::Max(0.0f, static_cast<float>(TimeRemaining));
}

FString ADojoCraftIslandManager::GetProcessingTypeName(uint8 ProcessType) const
{
    switch (ProcessType)
    {
        case 1: return TEXT("Grinding Wheat to Flour");
        case 2: return TEXT("Cutting Logs to Planks");
        case 3: return TEXT("Smelting Ore to Ingots");
        case 4: return TEXT("Crushing Stone to Gravel");
        case 5: return TEXT("Processing Clay to Bricks");
        default: return TEXT("Unknown Process");
    }
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
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetSelectedItemId: CurrentInventory is null"));
        return 0;
    }

    // Use local selected slot for immediate response (optimistic rendering)
    // Falls back to server-confirmed slot if local slot hasn't been set
    int32 SelectedSlot = (LocalHotbarSelectedSlot >= 0) ? LocalHotbarSelectedSlot : CurrentInventory->HotbarSelectedSlot;
    UE_LOG(LogTemp, VeryVerbose, TEXT("GetSelectedItemId: LocalHotbarSelectedSlot=%d, CurrentInventory->HotbarSelectedSlot=%d, Using slot=%d"), 
           LocalHotbarSelectedSlot, CurrentInventory->HotbarSelectedSlot, SelectedSlot);
    
    if (SelectedSlot >= 36) return 0; // Max 36 slots (9 per felt252)

    // Determine which slot field to use
    int32 FeltIndex = SelectedSlot / 9;
    int32 SlotInFelt = SelectedSlot % 9;
    UE_LOG(LogTemp, VeryVerbose, TEXT("GetSelectedItemId: FeltIndex=%d, SlotInFelt=%d"), FeltIndex, SlotInFelt);

    FString SlotData;
    switch (FeltIndex)
    {
        case 0: SlotData = CurrentInventory->Slots1; break;
        case 1: SlotData = CurrentInventory->Slots2; break;
        case 2: SlotData = CurrentInventory->Slots3; break;
        case 3: SlotData = CurrentInventory->Slots4; break;
        default: return 0;
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("GetSelectedItemId: Raw slot data for felt %d: %s"), FeltIndex, *SlotData);

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
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("GetSelectedItemId: BitOffset=%d, SlotDataValue=0x%llX, Extracted ItemType=%d"), 
           BitOffset, SlotDataValue, ItemType);

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

    UE_LOG(LogTemp, Warning, TEXT("=== SPACE TRANSITION START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Space changed from %s:%d to %s:%d"),
        *CurrentSpaceOwner, CurrentSpaceId,
        *PlayerData->CurrentSpaceOwner, PlayerData->CurrentSpaceId);

    // Save current player position before changing spaces
    SaveCurrentPlayerPosition();

    // Check if we're leaving space 1 BEFORE updating current space
    // Compare addresses by removing 0x prefix and comparing the hex part
    auto GetHexPart = [](const FString& Address) -> FString {
        FString Hex = Address.StartsWith("0x") ? Address.Mid(2) : Address;
        return Hex.ToLower();
    };
    
    FString CurrentOwnerHex = GetHexPart(CurrentSpaceOwner);
    FString AccountAddressHex = GetHexPart(Account.Address);
    FString PlayerDataOwnerHex = GetHexPart(PlayerData->CurrentSpaceOwner);
    
    // Compare by checking if one address ends with the other (handles leading zeros)
    bool bAddressesMatch = (CurrentOwnerHex.EndsWith(AccountAddressHex) || AccountAddressHex.EndsWith(CurrentOwnerHex));
    bool bPlayerDataAddressesMatch = (PlayerDataOwnerHex.EndsWith(AccountAddressHex) || AccountAddressHex.EndsWith(PlayerDataOwnerHex));
    
    bool bLeavingSpace1 = (bAddressesMatch && CurrentSpaceId == 1);
    bool bReturningToSpace1 = (bPlayerDataAddressesMatch && PlayerData->CurrentSpaceId == 1);

    UE_LOG(LogTemp, Warning, TEXT("HandleSpaceTransition: CurrentSpaceOwner=%s, Account.Address=%s"), 
           *CurrentSpaceOwner, *Account.Address);
    UE_LOG(LogTemp, Warning, TEXT("HandleSpaceTransition: CurrentOwnerHex=%s, AccountAddressHex=%s"), 
           *CurrentOwnerHex, *AccountAddressHex);
    UE_LOG(LogTemp, Warning, TEXT("HandleSpaceTransition: CurrentSpaceId=%d, comparing to 1"), 
           CurrentSpaceId);
    UE_LOG(LogTemp, Warning, TEXT("HandleSpaceTransition: bAddressesMatch=%d, bLeavingSpace1=%d, bReturningToSpace1=%d"), 
           bAddressesMatch, bLeavingSpace1, bReturningToSpace1);

    // Clear all current actors when changing spaces (pass leaving space 1 info)
    if (bLeavingSpace1)
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleSpaceTransition: Hiding space 1 actors"));
        SetActorsVisibilityAndCollision(false, false);
        bSpace1ActorsHidden = true;
    }
    else if (!bSpace1ActorsHidden)
    {
        // Only clear actors if not preserving space 1 actors
        ClearAllSpawnedActors();
    }

    // Update current space tracking
    CurrentSpaceOwner = PlayerData->CurrentSpaceOwner;
    CurrentSpaceId = PlayerData->CurrentSpaceId;

    // Reset structure type if returning to main space
    if (bReturningToSpace1)
    {
        CurrentSpaceStructureType = 0;
    }

    // If returning to space 1, clear current space actors and restore space 1
    if (bReturningToSpace1)
    {
        // Clear current space actors (like buildings)
        ClearAllSpawnedActors();
        
        // If space 1 actors are hidden, show them
        if (bSpace1ActorsHidden)
        {
            SetActorsVisibilityAndCollision(true, true);
            bSpace1ActorsHidden = false;
            UE_LOG(LogTemp, Log, TEXT("Restored space 1 actors"));
        }
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
            UE_LOG(LogTemp, Log, TEXT("DefaultBuilding has %d components, CurrentSpaceStructureType=%d"),
                Components.Num(), CurrentSpaceStructureType);

            for (UActorComponent* Component : Components)
            {
                if (Component)
                {
                    FString ComponentName = Component->GetName();
                    UE_LOG(LogTemp, Log, TEXT("Checking component: %s"), *ComponentName);

                    // Check if this is a workshop-specific component
                    // Look for any component with "Workshop" in the name (case insensitive)
                    if (ComponentName.Contains(TEXT("Workshop"), ESearchCase::IgnoreCase) ||
                        ComponentName.Contains(TEXT("WoodWorkshop"), ESearchCase::IgnoreCase) ||
                        ComponentName.Contains(TEXT("B_Workshop"), ESearchCase::IgnoreCase) ||
                        ComponentName.Contains(TEXT("workshop"), ESearchCase::IgnoreCase))
                    {
                        // Show if structure type is 60 (Workshop), hide otherwise
                        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
                        if (PrimComp)
                        {
                            bool bShouldBeVisible = (CurrentSpaceStructureType == 60);
                            PrimComp->SetVisibility(bShouldBeVisible);
                            PrimComp->SetCollisionEnabled(bShouldBeVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
                            UE_LOG(LogTemp, Warning, TEXT("Workshop component %s: visibility=%s, collision=%s (structure type=%d)"),
                                *ComponentName,
                                bShouldBeVisible ? TEXT("true") : TEXT("false"),
                                bShouldBeVisible ? TEXT("enabled") : TEXT("disabled"),
                                CurrentSpaceStructureType);
                        }
                    }
                }
            }

            // Also check child actors
            TArray<AActor*> ChildActors;
            DefaultBuilding->GetAllChildActors(ChildActors, true);
            UE_LOG(LogTemp, Log, TEXT("DefaultBuilding has %d child actors"), ChildActors.Num());

            for (AActor* ChildActor : ChildActors)
            {
                if (ChildActor)
                {
                    FString ActorName = ChildActor->GetName();
                    UE_LOG(LogTemp, Log, TEXT("Checking child actor: %s"), *ActorName);

                    // Check if this is a workshop-related actor
                    if (ActorName.Contains(TEXT("Workshop"), ESearchCase::IgnoreCase) ||
                        ActorName.Contains(TEXT("WoodWorkshop"), ESearchCase::IgnoreCase) ||
                        ActorName.Contains(TEXT("B_Workshop"), ESearchCase::IgnoreCase) ||
                        ActorName.Contains(TEXT("workshop"), ESearchCase::IgnoreCase))
                    {
                        bool bShouldBeVisible = (CurrentSpaceStructureType == 60);
                        ChildActor->SetActorHiddenInGame(!bShouldBeVisible);
                        ChildActor->SetActorEnableCollision(bShouldBeVisible);

                        UE_LOG(LogTemp, Warning, TEXT("Workshop child actor %s: hidden=%s, collision=%s (structure type=%d)"),
                            *ActorName,
                            !bShouldBeVisible ? TEXT("true") : TEXT("false"),
                            bShouldBeVisible ? TEXT("enabled") : TEXT("disabled"),
                            CurrentSpaceStructureType);
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

    UE_LOG(LogTemp, Warning, TEXT("=== CLEAR ACTORS START ==="));
    UE_LOG(LogTemp, Warning, TEXT("ClearAllSpawnedActors: CurrentSpace=%s:%d, Account=%s"),
        *CurrentSpaceOwner, CurrentSpaceId, *Account.Address);
    UE_LOG(LogTemp, Warning, TEXT("ClearAllSpawnedActors: Actors.Num()=%d, bSpace1ActorsHidden=%d"), 
        Actors.Num(), bSpace1ActorsHidden);

    // Check if we're currently in space 1
    bool bLeavingSpace1 = (CurrentSpaceOwner == Account.Address && CurrentSpaceId == 1);

    UE_LOG(LogTemp, Warning, TEXT("ClearAllSpawnedActors: bLeavingSpace1=%d"), bLeavingSpace1);

    if (bLeavingSpace1)
    {
        // Hide space 1 actors instead of destroying them
        UE_LOG(LogTemp, Warning, TEXT("ClearAllSpawnedActors: Hiding %d space 1 actors"), Actors.Num());
        SetActorsVisibilityAndCollision(false, false);
        bSpace1ActorsHidden = true;
        UE_LOG(LogTemp, Warning, TEXT("ClearAllSpawnedActors: Space 1 actors hidden"));
    }
    else
    {
        // If we have hidden space 1 actors, don't destroy them!
        if (bSpace1ActorsHidden)
        {
            UE_LOG(LogTemp, Warning, TEXT("ClearAllSpawnedActors: Keeping %d hidden space 1 actors"), Actors.Num());
            // Don't clear Actors or ActorSpawnInfo - they belong to space 1
        }
        else
        {
            // Destroy actors from other spaces
            UE_LOG(LogTemp, Warning, TEXT("ClearAllSpawnedActors: Destroying %d actors from other space"), Actors.Num());
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
    UE_LOG(LogTemp, VeryVerbose, TEXT("=== RemoveActorAtPosition START ==="));
    UE_LOG(LogTemp, VeryVerbose, TEXT("Position: (%d,%d,%d), RequiredType: %d"), 
        DojoPosition.X, DojoPosition.Y, DojoPosition.Z, (int32)RequiredType);
    
    // Check if this is confirming an optimistic removal
    if (OptimisticActors.Contains(DojoPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("Found in OptimisticActors"));
        AActor* OptimisticActor = OptimisticActors[DojoPosition];
        if (OptimisticActor && OptimisticActor->Tags.Contains(FName("OptimisticRemoval")))
        {
            // Confirm the removal - actually destroy the actor now
            if (IsValid(OptimisticActor))
            {
                UE_LOG(LogTemp, Warning, TEXT("Destroying optimistic actor"));
                OptimisticActor->Destroy();
            }
            OptimisticActors.Remove(DojoPosition);
            Actors.Remove(DojoPosition);
            ActorSpawnInfo.Remove(DojoPosition);

            UE_LOG(LogTemp, Warning, TEXT("Confirmed optimistic removal at (%d, %d, %d)"),
                DojoPosition.X, DojoPosition.Y, DojoPosition.Z);
            return;
        }
    }

    // Normal removal (non-optimistic)
    if (!Actors.Contains(DojoPosition))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Position not found in Actors map (normal for empty chunks)"));
        return;
    }

    if (ActorSpawnInfo.Contains(DojoPosition))
    {
        FActorSpawnInfo SpawnInfo = ActorSpawnInfo[DojoPosition];
        UE_LOG(LogTemp, Warning, TEXT("Found SpawnInfo, Type: %d"), (int32)SpawnInfo.SpawnType);
        
        if (SpawnInfo.SpawnType == RequiredType)
        {
            AActor* ToRemove = Actors[DojoPosition];
            if (IsValid(ToRemove))
            {
                UE_LOG(LogTemp, Warning, TEXT("Destroying actor: %s"), *ToRemove->GetName());
                ToRemove->Destroy();
            }
            Actors.Remove(DojoPosition);
            ActorSpawnInfo.Remove(DojoPosition);
            UE_LOG(LogTemp, Warning, TEXT("Actor removed successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SpawnType mismatch: Expected %d, Got %d"), 
                (int32)RequiredType, (int32)SpawnInfo.SpawnType);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Position not found in ActorSpawnInfo"));
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("=== RemoveActorAtPosition END ==="));
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
    if (!Gatherable || Gatherable->IslandOwner != CurrentSpaceOwner)
    {
        return;
    }

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

// Optimistic rendering methods
void ADojoCraftIslandManager::ApplyPendingVisual(AActor* Actor)
{
    if (!Actor) return;

    // Make the actor semi-transparent to indicate it's pending
    TArray<UActorComponent*> MeshComponents = Actor->GetComponents().Array();
    for (UActorComponent* Component : MeshComponents)
    {
        if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component))
        {
            // Apply translucent material or adjust opacity
            if (PendingMaterial)
            {
                // Use a specific pending material if available
                int32 NumMaterials = MeshComp->GetNumMaterials();
                for (int32 i = 0; i < NumMaterials; i++)
                {
                    MeshComp->SetMaterial(i, PendingMaterial);
                }
            }
            else
            {
                // Fall back to adjusting render settings
                MeshComp->SetRenderCustomDepth(true);
                MeshComp->SetCustomDepthStencilValue(1);

                // Make it slightly smaller to indicate pending state
                Actor->SetActorScale3D(FVector(0.95f, 0.95f, 0.95f));

                // Reduce opacity if possible
                for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
                {
                    UMaterialInstanceDynamic* DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
                    if (DynMaterial)
                    {
                        DynMaterial->SetScalarParameterValue(FName("Opacity"), 0.7f);
                        DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor(0.0f, 0.5f, 1.0f)); // Blue tint
                    }
                }
            }

            // Disable collision until confirmed
            MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    // Add a subtle pulsing effect (can be implemented in Blueprint)
    Actor->Tags.Add(FName("OptimisticPlacement"));
}

void ADojoCraftIslandManager::RemovePendingVisual(AActor* Actor)
{
    if (!Actor) return;

    TArray<UActorComponent*> MeshComponents = Actor->GetComponents().Array();
    for (UActorComponent* Component : MeshComponents)
    {
        if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component))
        {
            // Restore normal rendering
            MeshComp->SetRenderCustomDepth(false);

            // Restore normal scale
            Actor->SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));

            // Enable collision
            MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
    }

    // Remove tag
    Actor->Tags.Remove(FName("OptimisticPlacement"));

    UE_LOG(LogTemp, Log, TEXT("Confirmed optimistic placement for actor"));
}

void ADojoCraftIslandManager::AddOptimisticPlacement(const FIntVector& Position, E_Item Item)
{
    // Implementation handled inline in RequestPlaceUse for now
}

void ADojoCraftIslandManager::AddOptimisticRemoval(const FIntVector& Position)
{
    // Store the actor that will be removed optimistically
    if (Actors.Contains(Position))
    {
        AActor* ActorToRemove = Actors[Position];
        if (ActorToRemove)
        {
            // Make it semi-transparent to show it's being removed
            ApplyPendingVisual(ActorToRemove);
            ActorToRemove->Tags.Add(FName("OptimisticRemoval"));
        }
    }
}

void ADojoCraftIslandManager::ConfirmOptimisticAction(const FIntVector& Position)
{
    if (OptimisticActors.Contains(Position))
    {
        AActor* OptimisticActor = OptimisticActors[Position];
        if (OptimisticActor)
        {
            RemovePendingVisual(OptimisticActor);
        }
        OptimisticActors.Remove(Position);
        OptimisticActorTimestamps.Remove(Position);
    }
}

void ADojoCraftIslandManager::RollbackOptimisticAction(const FIntVector& Position)
{
    if (OptimisticActors.Contains(Position))
    {
        AActor* OptimisticActor = OptimisticActors[Position];
        if (OptimisticActor)
        {
            // Play a disappear animation or effect
            OptimisticActor->Destroy();
        }
        OptimisticActors.Remove(Position);
        OptimisticActorTimestamps.Remove(Position);

        UE_LOG(LogTemp, Warning, TEXT("Rolled back optimistic action at position (%d, %d, %d)"),
            Position.X, Position.Y, Position.Z);
    }
}

void ADojoCraftIslandManager::QueueTransaction(const FTransactionQueueItem& Item)
{
    FScopeLock Lock(&TransactionQueueMutex);
    
    // Prevent unbounded queue growth
    if (TransactionQueueCount >= MAX_QUEUE_SIZE)
    {
        UE_LOG(LogTemp, Warning, TEXT("Transaction queue is full (%d items), dropping oldest transactions"), MAX_QUEUE_SIZE);
        // Remove oldest items until we have room
        while (TransactionQueueCount >= MAX_QUEUE_SIZE - 10 && !TransactionQueue.IsEmpty())
        {
            FTransactionQueueItem DroppedItem;
            TransactionQueue.Dequeue(DroppedItem);
            TransactionQueueCount--;
        }
    }
    
    TransactionQueue.Enqueue(Item);
    TransactionQueueCount++;
    
    // Notify about queue update
    OnActionQueueUpdate.Broadcast(GetPendingActionCount());
    
    // Forward to GameInstance
    if (UCraftIslandGameInst* GI = Cast<UCraftIslandGameInst>(GetGameInstance()))
    {
        GI->OnActionQueueUpdate.Broadcast(GetPendingActionCount());
    }

    // If not already processing, start processing the queue
    if (!bIsProcessingTransaction)
    {
        ProcessNextTransaction();
    }
}

void ADojoCraftIslandManager::ProcessNextTransaction()
{
    // Cancel any existing batch timer
    GetWorld()->GetTimerManager().ClearTimer(BatchTimerHandle);
    
    // Collect batchable actions
    TArray<FTransactionQueueItem> BatchedActions;
    int32 TotalBits = 0;
    
    // Lock for queue access
    {
        FScopeLock Lock(&TransactionQueueMutex);
        
        // Try to collect as many actions as can fit
        while (!TransactionQueue.IsEmpty() && BatchedActions.Num() < MAX_BATCH_SIZE)
        {
        FTransactionQueueItem* PeekedItem = TransactionQueue.Peek();
        if (!PeekedItem) break;
        
        // Check if this action can be batched
        if (CanBatchAction(PeekedItem->Type))
        {
            int32 ActionSize = GetActionSize(PeekedItem->Type);
            
            // Check if we have room (conservative estimate)
            if (TotalBits + ActionSize > 240 && BatchedActions.Num() > 0)
            {
                break; // This action would overflow, send what we have
            }
            
            FTransactionQueueItem Item;
            TransactionQueue.Dequeue(Item);
            TransactionQueueCount--;
            BatchedActions.Add(Item);
            TotalBits += ActionSize;
            
            // Force send if we have enough actions
            if (BatchedActions.Num() >= FORCE_SEND_SIZE)
            {
                break;
            }
        }
        else
        {
            // Non-batchable action
            if (BatchedActions.Num() > 0)
            {
                // Send batched actions first
                break;
            }
            else
            {
                // Process single large action
                FTransactionQueueItem Item;
                TransactionQueue.Dequeue(Item);
                TransactionQueueCount--;
                BatchedActions.Add(Item);
                break;
            }
        }
        } // End while loop
    } // End lock scope
    
    // If we have actions to process
    if (BatchedActions.Num() > 0)
    {
        // Check if we should wait for more actions (need to check queue size safely)
        bool bQueueNotEmpty = false;
        {
            FScopeLock QueueLock(&TransactionQueueMutex);
            bQueueNotEmpty = !TransactionQueue.IsEmpty();
        }
        
        bool bShouldWait = BatchedActions.Num() < 3 && 
                          bQueueNotEmpty && 
                          CanBatchAction(BatchedActions[0].Type);
        
        if (bShouldWait)
        {
            // Set timer to wait for more actions
            GetWorld()->GetTimerManager().SetTimer(BatchTimerHandle, [this]()
            {
                ProcessNextTransaction();
            }, BATCH_WAIT_TIME, false);
            
            // Re-queue the actions we dequeued
            for (int32 i = BatchedActions.Num() - 1; i >= 0; i--)
            {
                // Note: TQueue doesn't support re-queuing at front, so this is a limitation
                // In production, we'd use a different data structure
            }
            return;
        }
        
        bIsProcessingTransaction = true;
        
        // Set timeout
        GetWorld()->GetTimerManager().SetTimer(TransactionTimeoutHandle, [this]()
        {
            UE_LOG(LogTemp, Warning, TEXT("Transaction timeout reached, forcing completion"));
            OnTransactionComplete();
        }, 15.0f, false);
        
        // Use new universal encoder
        TArray<FString> PackedActions = EncodePackedActions(BatchedActions);
        
        UE_LOG(LogTemp, Warning, TEXT("=== Sending Batched Actions ==="));
        UE_LOG(LogTemp, Warning, TEXT("Number of actions: %d"), BatchedActions.Num());
        for (int32 i = 0; i < BatchedActions.Num(); i++)
        {
            const FTransactionQueueItem& Action = BatchedActions[i];
            UE_LOG(LogTemp, Warning, TEXT("Action %d: Type=%d, Position=(%d,%d,%d)"), 
                i, (int32)Action.Type, Action.Position.X, Action.Position.Y, Action.Position.Z);
        }
        
        // Call the new execute_packed_actions method
        if (DojoHelpers)
        {
            UE_LOG(LogTemp, Warning, TEXT("Calling execute_packed_actions with %d packed felts"), PackedActions.Num());
            for (int32 i = 0; i < PackedActions.Num(); i++)
            {
                UE_LOG(LogTemp, Warning, TEXT("PackedFelt[%d]: %s"), i, *PackedActions[i]);
            }
            
            // Use the universal packing system
            DojoHelpers->CallCraftIslandPocketActionsExecutePackedActions(Account, PackedActions);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Sent %d actions in %d felts"), BatchedActions.Num(), PackedActions.Num());
    }
    else
    {
        bIsProcessingTransaction = false;
    }
}

void ADojoCraftIslandManager::ProcessSingleAction(const FTransactionQueueItem& Action)
{
    UE_LOG(LogTemp, Warning, TEXT("=== ProcessSingleAction START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Action Type: %d"), (int32)Action.Type);
    
    switch (Action.Type)
    {
        case ETransactionType::SelectHotbar:
            DojoHelpers->CallCraftIslandPocketActionsSelectHotbarSlot(Account, Action.IntParam);
            break;
        case ETransactionType::MoveItem:
            DojoHelpers->CallCraftIslandPocketActionsInventoryMoveItem(Account, 
                Action.IntParam, Action.IntParam2, Action.IntParam3, Action.IntParam4);
            break;
        case ETransactionType::Craft:
            DojoHelpers->CallCraftIslandPocketActionsCraft(Account, 
                Action.IntParam, Action.Position.X, Action.Position.Y, Action.Position.Z);
            break;
        case ETransactionType::Buy:
            // TODO: Handle multiple items
            if (Action.ItemIds.Num() > 0 && Action.Quantities.Num() > 0)
            {
                DojoHelpers->CallCraftIslandPocketActionsBuy(Account, 
                    Action.ItemIds[0], Action.Quantities[0]);
            }
            break;
        case ETransactionType::Sell:
            DojoHelpers->CallCraftIslandPocketActionsSell(Account);
            break;
        case ETransactionType::StartProcess:
            DojoHelpers->CallCraftIslandPocketActionsStartProcessing(Account, 
                Action.IntParam, Action.IntParam2);
            break;
        case ETransactionType::CancelProcess:
            DojoHelpers->CallCraftIslandPocketActionsCancelProcessing(Account);
            break;
        case ETransactionType::Visit:
            DojoHelpers->CallCraftIslandPocketActionsVisit(Account, Action.IntParam);
            break;
        case ETransactionType::VisitNewIsland:
            DojoHelpers->CallCraftIslandPocketActionsVisitNewIsland(Account);
            break;
        case ETransactionType::GenerateIsland:
            DojoHelpers->CallCraftIslandPocketActionsGenerateIslandPart(Account, 
                Action.Position.X, Action.Position.Y, Action.Position.Z, Action.IntParam);
            break;
        default:
            UE_LOG(LogTemp, Warning, TEXT("Unhandled transaction type: %d"), (int32)Action.Type);
            break;
    }
}

void ADojoCraftIslandManager::OnTransactionComplete()
{
    // Clear the timeout timer since transaction completed
    GetWorld()->GetTimerManager().ClearTimer(TransactionTimeoutHandle);
    
    bIsProcessingTransaction = false;
    
    // Notify about queue update
    OnActionQueueUpdate.Broadcast(GetPendingActionCount());
    
    // Forward to GameInstance
    if (UCraftIslandGameInst* GI = Cast<UCraftIslandGameInst>(GetGameInstance()))
    {
        GI->OnActionQueueUpdate.Broadcast(GetPendingActionCount());
    }

    // Process next transaction in queue after a small delay
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
    {
        ProcessNextTransaction();
    }, 0.1f, false);
}

// Removed old EncodeCompressedActions function - now using EncodePackedActions
#if 0
FString ADojoCraftIslandManager::EncodeCompressedActions(const TArray<FTransactionQueueItem>& Actions)
{
    // Encode up to 8 actions into a felt252
    // Bit layout per action (30 bits): [1 bit: action_type][1 bit: z][14 bits: x][14 bits: y]
    
    // Use FUint64 to build the packed value, then convert to hex string
    TArray<uint8> Bytes;
    Bytes.SetNum(32); // 256 bits / 8 = 32 bytes
    for (int i = 0; i < 32; i++) Bytes[i] = 0;
    
    int BitOffset = 0;
    
    for (int i = 0; i < Actions.Num() && i < 8; i++)
    {
        const FTransactionQueueItem& Action = Actions[i];
        
        // Use coordinates as-is (they already have 8192 offset from queueing)
        int32 X = Action.Position.X;
        int32 Y = Action.Position.Y;
        int32 Z = Action.Position.Z - 8192;  // Z needs special handling for the bit
        
        // Clamp to valid range (0-16383) that fits in 14 bits
        X = FMath::Clamp(X, 0, 16383);
        Y = FMath::Clamp(Y, 0, 16383);
        
        UE_LOG(LogTemp, Log, TEXT("Encoding action %d: Type=%s, Position=(%d,%d,%d), Z-bit=%d"),
            i,
            Action.Type == ETransactionType::Hit ? TEXT("Hit") : TEXT("Place"),
            X, Y, Action.Position.Z,
            Z);
        
        // Build the 30-bit action
        uint32 ActionData = 0;
        
        // Y coordinate (14 bits)
        ActionData |= (Y & 0x3FFF);
        
        // X coordinate (14 bits)
        ActionData |= ((X & 0x3FFF) << 14);
        
        // Z bit (1 bit) - 0 for z=8192, 1 for z=8193
        ActionData |= ((Z == 1 ? 1 : 0) << 28);
        
        // Action type (1 bit) - 0 for PlaceUse, 1 for Hit
        ActionData |= ((Action.Type == ETransactionType::Hit ? 1 : 0) << 29);
        
        // Pack into byte array
        int ByteIndex = BitOffset / 8;
        int BitInByte = BitOffset % 8;
        
        // Write the 30 bits
        for (int j = 0; j < 30; j++)
        {
            if ((ActionData & (1 << j)) != 0)
            {
                int CurrentByteIndex = (BitOffset + j) / 8;
                int CurrentBitInByte = (BitOffset + j) % 8;
                if (CurrentByteIndex < 32)
                {
                    Bytes[CurrentByteIndex] |= (1 << CurrentBitInByte);
                }
            }
        }
        
        BitOffset += 30;
    }
    
    // Convert to hex string (big-endian for Starknet felt252)
    FString HexString = TEXT("0x");
    for (int i = 31; i >= 0; i--)
    {
        HexString += FString::Printf(TEXT("%02x"), Bytes[i]);
    }
    
    // Remove leading zeros (keep at least one zero after 0x)
    while (HexString.Len() > 3 && HexString[2] == '0' && HexString[3] == '0')
    {
        HexString.RemoveAt(2, 2);
    }
    
    return HexString;
}
#endif

// Queue methods for batching
void ADojoCraftIslandManager::QueueInventoryMove(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot)
{
    FTransactionQueueItem Item;
    Item.Type = ETransactionType::MoveItem;
    Item.IntParam = FromInventory;
    Item.IntParam2 = FromSlot;
    Item.IntParam3 = ToInventory;
    Item.IntParam4 = ToSlot;
    
    QueueTransaction(Item);
    
    // Fire optimistic update event
    OnOptimisticInventoryMove.Broadcast(FromInventory, FromSlot, ToInventory, ToSlot);
    
    // Forward to GameInstance
    if (UCraftIslandGameInst* GI = Cast<UCraftIslandGameInst>(GetGameInstance()))
    {
        GI->OnOptimisticInventoryMove.Broadcast(FromInventory, FromSlot, ToInventory, ToSlot);
    }
    
    // Apply optimistic update locally
    ApplyOptimisticInventoryMove(Item);
}

void ADojoCraftIslandManager::QueueCraft(int32 ItemId)
{
    FTransactionQueueItem Item;
    Item.Type = ETransactionType::Craft;
    Item.IntParam = ItemId;
    Item.Position = FIntVector(
        TargetBlock.X + 8192,
        TargetBlock.Y + 8192,
        TargetBlock.Z + 8192
    );
    
    QueueTransaction(Item);
    
    // Fire optimistic update event
    OnOptimisticCraft.Broadcast(ItemId, true);
    
    // Forward to GameInstance
    if (UCraftIslandGameInst* GI = Cast<UCraftIslandGameInst>(GetGameInstance()))
    {
        GI->OnOptimisticCraft.Broadcast(ItemId, true);
    }
}

void ADojoCraftIslandManager::QueueBuy(const TArray<int32>& ItemIds, const TArray<int32>& Quantities)
{
    FTransactionQueueItem Item;
    Item.Type = ETransactionType::Buy;
    Item.ItemIds = ItemIds;
    Item.Quantities = Quantities;
    
    QueueTransaction(Item);
}

void ADojoCraftIslandManager::QueueSell()
{
    FTransactionQueueItem Item;
    Item.Type = ETransactionType::Sell;
    
    QueueTransaction(Item);
    
    // Fire optimistic update event
    OnOptimisticSell.Broadcast(true);
    
    // Forward to GameInstance
    if (UCraftIslandGameInst* GI = Cast<UCraftIslandGameInst>(GetGameInstance()))
    {
        GI->OnOptimisticSell.Broadcast(true);
    }
}

void ADojoCraftIslandManager::QueueVisit(int32 SpaceId)
{
    FTransactionQueueItem Item;
    Item.Type = ETransactionType::Visit;
    Item.IntParam = SpaceId;
    
    QueueTransaction(Item);
}

void ADojoCraftIslandManager::FlushActionQueue()
{
    // Force process any pending actions
    bool bHasPendingActions = false;
    {
        FScopeLock Lock(&TransactionQueueMutex);
        bHasPendingActions = !TransactionQueue.IsEmpty();
    }
    
    if (!bIsProcessingTransaction && bHasPendingActions)
    {
        ProcessNextTransaction();
    }
}

int32 ADojoCraftIslandManager::GetPendingActionCount() const
{
    return TransactionQueueCount;
}

// Universal action encoder implementation
TArray<FString> ADojoCraftIslandManager::EncodePackedActions(const TArray<FTransactionQueueItem>& Actions)
{
    TArray<FString> PackedFelts;
    int32 Index = 0;
    
    while (Index < Actions.Num())
    {
        const FTransactionQueueItem& CurrentAction = Actions[Index];
        
        
        // Determine which pack type to use
        if (CurrentAction.Type == ETransactionType::MoveItem)
        {
            // Try to pack Type 1 actions
            FString Packed = PackType1Actions(Actions, Index);
            if (!Packed.IsEmpty())
            {
                PackedFelts.Add(Packed);
                continue;
            }
        }
        else if (CanBatchAction(CurrentAction.Type) || 
                 CurrentAction.Type == ETransactionType::PlaceUse || 
                 CurrentAction.Type == ETransactionType::Hit)
        {
            // Try to pack Type 2 actions (now includes PlaceUse/Hit)
            FString Packed = PackType2Actions(Actions, Index);
            if (!Packed.IsEmpty())
            {
                PackedFelts.Add(Packed);
                continue;
            }
        }
        
        // Fall back to Type 3 for large actions
        FString Packed = PackType3Action(CurrentAction);
        PackedFelts.Add(Packed);
        Index++;
    }
    
    return PackedFelts;
}

FString ADojoCraftIslandManager::PackType0Actions(const TArray<FTransactionQueueItem>& Actions, int32& Index)
{
    // Pack up to 8 PlaceUse/Hit actions
    TArray<uint8> Bytes;
    Bytes.SetNum(32);
    for (int i = 0; i < 32; i++) Bytes[i] = 0;
    
    int32 BitOffset = 0;
    
    // Pack type (8 bits)
    WriteBits(Bytes, BitOffset, 0, 8);
    
    int32 Count = 0;
    int32 StartIndex = Index;
    
    // Reserve space for count (4 bits)
    BitOffset += 4;
    
    // Pack actions
    while (Index < Actions.Num() && Count < 8)
    {
        const FTransactionQueueItem& Action = Actions[Index];
        if (Action.Type != ETransactionType::PlaceUse && Action.Type != ETransactionType::Hit)
        {
            break;
        }
        
        // Calculate coordinates - use game coordinates directly
        // The contract expects coordinates with 8192 offset (e.g., 8192,8192,8193)
        uint32 X = Action.Position.X;
        uint32 Y = Action.Position.Y;
        uint32 Z = Action.Position.Z - 8192; // Convert back to blockchain coordinate system
        uint32 ActionType = (Action.Type == ETransactionType::Hit) ? 1 : 0;
        
        // Pack 30 bits: [1 bit: action_type][14 bits: y][14 bits: x][1 bit: z]
        // Bit positions: action_type(0), y(1-14), x(15-28), z(29)
        uint32 PackedAction = (ActionType & 0x1) | ((Y & 0x3FFF) << 1) | ((X & 0x3FFF) << 15) | (Z << 29);
        
        UE_LOG(LogTemp, Warning, TEXT("Packing Type0 Action: Pos(%d,%d,%d) Z_bit=%d ActionType=%d PackedAction=0x%08X"), 
            Action.Position.X, Action.Position.Y, Action.Position.Z, Z, ActionType, PackedAction);
        UE_LOG(LogTemp, Warning, TEXT("  Raw values: X=%d(0x%X) Y=%d(0x%X) after mask: X=%d Y=%d"), 
            X, X, Y, Y, X & 0x3FFF, Y & 0x3FFF);
        
        // Debug: Check individual components
        uint32 ActionTypeCheck = PackedAction & 0x1;      // 1 bit for action type
        uint32 YCheck = (PackedAction >> 1) & 0x3FFF;     // 14 bits for Y
        uint32 XCheck = (PackedAction >> 15) & 0x3FFF;    // 14 bits for X
        uint32 ZCheck = (PackedAction >> 29) & 0x1;       // 1 bit for Z at position 29
        UE_LOG(LogTemp, Warning, TEXT("  Verify: ActionType=%d Y=%d X=%d Z=%d"), ActionTypeCheck, YCheck, XCheck, ZCheck);
        
        WriteBits(Bytes, BitOffset, PackedAction, 30);
        
        Count++;
        Index++;
    }
    
    // If no actions were packed, return empty
    if (Count == 0)
    {
        Index = StartIndex;
        return TEXT("");
    }
    
    // Write count
    int32 CountOffset = 8;
    WriteBits(Bytes, CountOffset, Count, 4);
    
    FString HexResult = BytesToHexString(Bytes);
    UE_LOG(LogTemp, Warning, TEXT("PackType0Actions result: %s"), *HexResult);
    
    return HexResult;
}

FString ADojoCraftIslandManager::PackType1Actions(const TArray<FTransactionQueueItem>& Actions, int32& Index)
{
    // Pack up to 6 MoveItem actions
    TArray<uint8> Bytes;
    Bytes.SetNum(32);
    for (int i = 0; i < 32; i++) Bytes[i] = 0;
    
    int32 BitOffset = 0;
    
    // Pack type (8 bits)
    WriteBits(Bytes, BitOffset, 1, 8);
    
    int32 Count = 0;
    int32 StartIndex = Index;
    
    // Reserve space for count (4 bits)
    BitOffset += 4;
    
    // Pack actions
    while (Index < Actions.Num() && Count < 6)
    {
        const FTransactionQueueItem& Action = Actions[Index];
        if (Action.Type != ETransactionType::MoveItem)
        {
            break;
        }
        
        // Pack 40 bits: [8 bits: from_inv][8 bits: from_slot][8 bits: to_inv][8 bits: to_slot][8 bits: reserved]
        uint64 PackedMove = (Action.IntParam & 0xFF) | 
                           ((Action.IntParam2 & 0xFF) << 8) |
                           ((Action.IntParam3 & 0xFF) << 16) |
                           ((Action.IntParam4 & 0xFF) << 24);
        
        WriteBits(Bytes, BitOffset, PackedMove, 40);
        
        Count++;
        Index++;
    }
    
    // If no actions were packed, return empty
    if (Count == 0)
    {
        Index = StartIndex;
        return TEXT("");
    }
    
    // Write count
    int32 CountOffset = 8;
    WriteBits(Bytes, CountOffset, Count, 4);
    
    return BytesToHexString(Bytes);
}

FString ADojoCraftIslandManager::PackType2Actions(const TArray<FTransactionQueueItem>& Actions, int32& Index)
{
    // Pack mixed simple actions
    TArray<uint8> Bytes;
    Bytes.SetNum(32);
    for (int i = 0; i < 32; i++) Bytes[i] = 0;
    
    int32 BitOffset = 0;
    
    // Pack type (8 bits)
    WriteBits(Bytes, BitOffset, 2, 8);
    
    int32 Count = 0;
    int32 StartIndex = Index;
    
    // Reserve space for count (4 bits)
    BitOffset += 4;
    
    // Pack actions
    while (Index < Actions.Num() && BitOffset < 240) // Leave some buffer
    {
        const FTransactionQueueItem& Action = Actions[Index];
        
        int32 ActionTypeCode = -1;
        int32 ExtraBits = 0;
        uint64 ExtraData = 0;
        
        switch (Action.Type)
        {
            case ETransactionType::PlaceUse:
                ActionTypeCode = 0;
                ExtraBits = 32; // 30 bits position + 2 bits padding
                {
                    uint32 X = Action.Position.X & 0x3FFF; // 14 bits
                    uint32 Y = Action.Position.Y & 0x3FFF; // 14 bits  
                    uint32 Z = (Action.Position.Z == 8193) ? 1 : 0; // 1 bit
                    ExtraData = Y | (X << 14) | (Z << 28); // 30 bits total
                }
                break;
            case ETransactionType::Hit:
                ActionTypeCode = 1;
                ExtraBits = 32; // 30 bits position + 2 bits padding
                {
                    uint32 X = Action.Position.X & 0x3FFF; // 14 bits
                    uint32 Y = Action.Position.Y & 0x3FFF; // 14 bits
                    uint32 Z = (Action.Position.Z == 8193) ? 1 : 0; // 1 bit
                    UE_LOG(LogTemp, Warning, TEXT("Hit encoding: Z=%d, z_bit=%d"), Action.Position.Z, Z);
                    ExtraData = Y | (X << 14) | (Z << 28); // 30 bits total
                }
                break;
            case ETransactionType::SelectHotbar:
                ActionTypeCode = 2;
                ExtraBits = 8;
                ExtraData = Action.IntParam & 0xFF;
                break;
            case ETransactionType::Sell:
                ActionTypeCode = 6;
                break;
            case ETransactionType::CancelProcess:
                ActionTypeCode = 8;
                break;
            case ETransactionType::Visit:
                ActionTypeCode = 9;
                ExtraBits = 16;
                ExtraData = Action.IntParam & 0xFFFF;
                break;
            case ETransactionType::VisitNewIsland:
                ActionTypeCode = 10;
                break;
            default:
                // Skip non-simple actions
                break;
        }
        
        if (ActionTypeCode == -1)
        {
            break;
        }
        
        // Check if we have space
        if (BitOffset + 8 + ExtraBits > 250)
        {
            break;
        }
        
        // Write action type
        WriteBits(Bytes, BitOffset, ActionTypeCode, 8);
        
        // Write extra data if any
        if (ExtraBits > 0)
        {
            WriteBits(Bytes, BitOffset, ExtraData, ExtraBits);
        }
        
        Count++;
        Index++;
    }
    
    // If no actions were packed, return empty
    if (Count == 0)
    {
        Index = StartIndex;
        return TEXT("");
    }
    
    // Write count
    int32 CountOffset = 8;
    WriteBits(Bytes, CountOffset, Count, 4);
    
    return BytesToHexString(Bytes);
}

FString ADojoCraftIslandManager::PackType3Action(const FTransactionQueueItem& Action)
{
    // Pack single large action
    TArray<uint8> Bytes;
    Bytes.SetNum(32);
    for (int i = 0; i < 32; i++) Bytes[i] = 0;
    
    int32 BitOffset = 0;
    
    // Pack type (8 bits)
    WriteBits(Bytes, BitOffset, 3, 8);
    
    // Action type (8 bits)
    int32 ActionTypeCode = -1;
    switch (Action.Type)
    {
        case ETransactionType::Craft:
            ActionTypeCode = 3;
            break;
        case ETransactionType::Buy:
            ActionTypeCode = 5;
            break;
        case ETransactionType::StartProcess:
            ActionTypeCode = 7;
            break;
        case ETransactionType::GenerateIsland:
            ActionTypeCode = 11;
            break;
        default:
            UE_LOG(LogTemp, Error, TEXT("Unsupported action type for Type 3 packing"));
            return TEXT("0x0");
    }
    
    WriteBits(Bytes, BitOffset, ActionTypeCode, 8);
    
    // Pack action-specific data
    switch (Action.Type)
    {
        case ETransactionType::Craft:
        {
            // [32 bits: item_id][30 bits: position]
            WriteBits(Bytes, BitOffset, Action.IntParam, 32);
            
            // Pack 30 bits: [14 bits: y][14 bits: x][2 bits: z and reserved]
            uint32 Y = Action.Position.Y & 0x3FFF; // 14 bits
            uint32 X = Action.Position.X & 0x3FFF; // 14 bits
            uint32 Z = (Action.Position.Z == 8193) ? 1 : 0;
            uint32 Coords = Y | (X << 14) | (Z << 28);
            WriteBits(Bytes, BitOffset, Coords, 30);
            break;
        }
        case ETransactionType::Buy:
        {
            // For simplicity, only support single item buy in Type 3
            if (Action.ItemIds.Num() > 0 && Action.Quantities.Num() > 0)
            {
                WriteBits(Bytes, BitOffset, Action.ItemIds[0], 16);
                WriteBits(Bytes, BitOffset, Action.Quantities[0], 32);
            }
            break;
        }
        case ETransactionType::StartProcess:
        {
            WriteBits(Bytes, BitOffset, Action.IntParam, 8); // process type
            WriteBits(Bytes, BitOffset, Action.IntParam2, 32); // amount
            break;
        }
        case ETransactionType::GenerateIsland:
        {
            // Pack 30 bits: [14 bits: y][14 bits: x][2 bits: z and reserved]
            uint32 Y = Action.Position.Y & 0x3FFF; // 14 bits
            uint32 X = Action.Position.X & 0x3FFF; // 14 bits
            uint32 Z = (Action.Position.Z == 8193) ? 1 : 0;
            uint32 Coords = Y | (X << 14) | (Z << 28);
            WriteBits(Bytes, BitOffset, Coords, 30);
            WriteBits(Bytes, BitOffset, Action.IntParam, 16); // island_id
            break;
        }
    }
    
    return BytesToHexString(Bytes);
}


bool ADojoCraftIslandManager::CanBatchAction(ETransactionType Type)
{
    switch(Type)
    {
        case ETransactionType::PlaceUse:
        case ETransactionType::Hit:
        case ETransactionType::MoveItem:
        case ETransactionType::SelectHotbar:
        case ETransactionType::Sell:
        case ETransactionType::CancelProcess:
        case ETransactionType::Visit:
        case ETransactionType::VisitNewIsland:
            return true;
        default:
            return false;
    }
}

int32 ADojoCraftIslandManager::GetActionSize(ETransactionType Type)
{
    switch(Type)
    {
        case ETransactionType::PlaceUse:
        case ETransactionType::Hit:
            return 30; // bits
        case ETransactionType::MoveItem:
            return 40; // bits
        case ETransactionType::SelectHotbar:
            return 16; // 8 + 8 bits
        case ETransactionType::Sell:
        case ETransactionType::CancelProcess:
        case ETransactionType::VisitNewIsland:
            return 8; // bits
        case ETransactionType::Visit:
            return 24; // 8 + 16 bits
        default:
            return 256; // Full felt
    }
}

void ADojoCraftIslandManager::WriteBits(TArray<uint8>& Bytes, int32& BitOffset, uint64 Value, int32 NumBits)
{
    for (int32 i = 0; i < NumBits; i++)
    {
        if ((Value & (1ULL << i)) != 0)
        {
            int32 ByteIndex = BitOffset / 8;
            int32 BitInByte = BitOffset % 8;
            if (ByteIndex < Bytes.Num())
            {
                Bytes[ByteIndex] |= (1 << BitInByte);
            }
        }
        BitOffset++;
    }
}

FString ADojoCraftIslandManager::BytesToHexString(const TArray<uint8>& Bytes)
{
    // Convert to hex string (big-endian for Starknet felt252)
    FString HexString = TEXT("0x");
    bool bFoundNonZero = false;
    
    for (int32 i = 31; i >= 0; i--)
    {
        if (Bytes[i] != 0 || bFoundNonZero || i == 0)
        {
            HexString += FString::Printf(TEXT("%02x"), Bytes[i]);
            bFoundNonZero = true;
        }
    }
    
    return HexString;
}

// Optimistic inventory update methods
void ADojoCraftIslandManager::ApplyOptimisticInventoryMove(const FTransactionQueueItem& Action)
{
    // Store the pending action
    OptimisticInventory.PendingActions.Add(Action);
    
    // Update action queue count
    OnActionQueueUpdate.Broadcast(GetPendingActionCount());
}

void ADojoCraftIslandManager::RollbackOptimisticInventoryMove(const FTransactionQueueItem& Action)
{
    // Remove from pending actions
    OptimisticInventory.PendingActions.RemoveAll([&Action](const FTransactionQueueItem& Item) {
        return Item.IntParam == Action.IntParam && 
               Item.IntParam2 == Action.IntParam2 &&
               Item.IntParam3 == Action.IntParam3 &&
               Item.IntParam4 == Action.IntParam4;
    });
    
    // Fire rollback event
    OnOptimisticInventoryMove.Broadcast(Action.IntParam3, Action.IntParam4, Action.IntParam, Action.IntParam2);
    
    // Update action queue count
    OnActionQueueUpdate.Broadcast(GetPendingActionCount());
}

void ADojoCraftIslandManager::ConfirmOptimisticInventoryActions()
{
    // Clear pending actions that were confirmed
    OptimisticInventory.PendingActions.Empty();
    
    // Update action queue count
    OnActionQueueUpdate.Broadcast(GetPendingActionCount());
}

void ADojoCraftIslandManager::StartOptimisticCleanupTimer()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            OptimisticCleanupTimerHandle,
            this,
            &ADojoCraftIslandManager::CleanupTimedOutOptimisticActors,
            5.0f, // Check every 5 seconds
            true  // Repeating
        );
    }
}

void ADojoCraftIslandManager::CleanupTimedOutOptimisticActors()
{
    if (!GetWorld()) return;
    
    double CurrentTime = GetWorld()->GetTimeSeconds();
    TArray<FIntVector> PositionsToRemove;
    
    // Find timed out optimistic actors
    for (auto& TimestampPair : OptimisticActorTimestamps)
    {
        const FIntVector& Position = TimestampPair.Key;
        double CreationTime = TimestampPair.Value;
        
        if (CurrentTime - CreationTime > OPTIMISTIC_TIMEOUT_SECONDS)
        {
            PositionsToRemove.Add(Position);
        }
    }
    
    // Rollback timed out actors
    for (const FIntVector& Position : PositionsToRemove)
    {
        UE_LOG(LogTemp, Warning, TEXT("Rolling back timed out optimistic actor at position (%d, %d, %d)"),
               Position.X, Position.Y, Position.Z);
        RollbackOptimisticAction(Position);
    }
    
    // Also clean up any invalid actor references
    TArray<FIntVector> InvalidActorPositions;
    for (auto& ActorPair : OptimisticActors)
    {
        if (!IsValid(ActorPair.Value))
        {
            InvalidActorPositions.Add(ActorPair.Key);
        }
    }
    
    for (const FIntVector& Position : InvalidActorPositions)
    {
        OptimisticActors.Remove(Position);
        OptimisticActorTimestamps.Remove(Position);
        UE_LOG(LogTemp, Warning, TEXT("Cleaned up invalid optimistic actor reference at position (%d, %d, %d)"),
               Position.X, Position.Y, Position.Z);
    }
}

void ADojoCraftIslandManager::ResetLocalHotbarSelection()
{
    UE_LOG(LogTemp, Warning, TEXT("Hotbar selection timeout - resetting local state from slot %d to server state"), 
           LocalHotbarSelectedSlot);
    
    // Reset to unset state - will sync with server on next inventory update
    LocalHotbarSelectedSlot = -1;
    LocalHotbarSelectionTimestamp = 0.0;
    LocalHotbarSelectionSequence = 0;
    
    // If we have current inventory, sync immediately
    if (CurrentInventory)
    {
        LocalHotbarSelectedSlot = CurrentInventory->HotbarSelectedSlot;
        LocalHotbarSelectionTimestamp = GetWorld()->GetTimeSeconds();
        bHotbarSelectionPending = false;
        UE_LOG(LogTemp, Warning, TEXT("Reset complete - now using server slot %d"), LocalHotbarSelectedSlot);
    }
}

void ADojoCraftIslandManager::QueuePendingHotbarSelection()
{
    // Only queue if we have a pending hotbar change
    if (bHotbarSelectionPending && CurrentInventory)
    {
        // Double-check that our local selection still differs from server
        if (LocalHotbarSelectedSlot != CurrentInventory->HotbarSelectedSlot)
        {
            FTransactionQueueItem HotbarItem;
            HotbarItem.Type = ETransactionType::SelectHotbar;
            HotbarItem.IntParam = LocalHotbarSelectedSlot;
            HotbarItem.SequenceNumber = NextHotbarSequence++;
            
            UE_LOG(LogTemp, Warning, TEXT("Queuing pending hotbar selection: slot %d (was %d)"), 
                   LocalHotbarSelectedSlot, CurrentInventory->HotbarSelectedSlot);
            
            QueueTransaction(HotbarItem);
            bHotbarSelectionPending = false; // Mark as sent
        }
        else
        {
            // Server caught up somehow - clear pending flag
            bHotbarSelectionPending = false;
            UE_LOG(LogTemp, Warning, TEXT("Hotbar selection no longer pending - server caught up"));
        }
    }
}
