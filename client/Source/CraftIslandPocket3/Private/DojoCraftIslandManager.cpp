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
    if (OnboardingWidgetClass)
    {
        OnboardingUI = CreateWidget<UUserWidget>(GetWorld(), OnboardingWidgetClass);
        if (OnboardingUI)
        {
            OnboardingUI->AddToViewport();
        }
    }

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
    if (FloatingShipClass)
    {
        FVector Location(500.f, 3000.f, 2.f);
        FRotator Rotation(0.f, 0.f, 90.f);
        FVector Scale(1.f, 2.f, 1.f);

        FTransform SpawnTransform(Rotation, Location, Scale);

        FloatingShip = GetWorld()->SpawnActor<AActor>(FloatingShipClass, SpawnTransform);
    }
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
    int32 X = FMath::TruncToInt(TargetLocation.X);
    int32 Y = FMath::TruncToInt(TargetLocation.Y);
    int32 Z = FMath::TruncToInt(TargetLocation.Z);

    FIntVector TestVector(X, Y, Z + 8192);

    // Step 4: Check if in Actors array
    bool bFound = Actors.Contains(TestVector);

    // Step 5: Select Z value based on presence
    int32 ZValue = bFound ? 1 : 0;

    // Step 6: Rebuild target vector
    FVector FinalTargetVector(X, Y, ZValue);

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

    if (Name == "craft_island_pocket-IslandChunk") {
        UDojoModelCraftIslandPocketIslandChunk* Chunk = reinterpret_cast<UDojoModelCraftIslandPocketIslandChunk*>(Model);
        
        if (Chunk->IslandOwner == Account.Address) {
            // Check IslandId

            FString Blocks = Chunk->Blocks1.Mid(2) + Chunk->Blocks2.Mid(2);
            FString SubStr = Blocks.Reverse();

            FIntVector ChunkOffset = this->HexStringToVector(Chunk->ChunkId);

            int32 Index = 0;
            for (TCHAR Char : SubStr)
            {
                uint8 Byte = FParse::HexDigit(Char);
                int32 IntValue = static_cast<int32>(Byte);
                E_Item Item = static_cast<E_Item>(IntValue);
                FIntVector dojoPos = this->GetWorldPositionFromLocal(Index, ChunkOffset);
                this->PlaceAssetInWorld(Item, dojoPos, false);
                Index++;
            }
        }
    }
    else if (Name == "craft_island_pocket-GatherableResource") {
        UDojoModelCraftIslandPocketGatherableResource* Gatherable = reinterpret_cast<UDojoModelCraftIslandPocketGatherableResource*>(Model);
        
        if (Gatherable->IslandOwner == Account.Address) {
            // Check IslandId

            FIntVector ChunkOffset = this->HexStringToVector(Gatherable->ChunkId);

            E_Item Item = static_cast<E_Item>(Gatherable->ResourceId);
            FIntVector dojoPos = this->GetWorldPositionFromLocal(Gatherable->Position, ChunkOffset);
            AActor *SpawnedActor = this->PlaceAssetInWorld(Item, dojoPos, false);
            
        }
    }
    else if (Name == "craft_island_pocket-WorldStructure") {
        UDojoModelCraftIslandPocketWorldStructure* Structure = reinterpret_cast<UDojoModelCraftIslandPocketWorldStructure*>(Model);
        if (Structure->IslandOwner == Account.Address) {
            // Check IslandId

            FIntVector ChunkOffset = this->HexStringToVector(Structure->ChunkId);

            E_Item Item = static_cast<E_Item>(Structure->StructureType);
            FIntVector dojoPos = this->GetWorldPositionFromLocal(Structure->Position, ChunkOffset);
            this->PlaceAssetInWorld(Item, dojoPos, false);
        }
    }
    else if (Name == "craft_island_pocket-Inventory") {
        this->HandleInventory(Model);
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

void ADojoCraftIslandManager::OnUIDelayedLoad()
{
    if (UI)
    {
        UI->CallFunctionByNameWithArguments(TEXT("Loaded"), *GLog, nullptr, true);
    }
}

void ADojoCraftIslandManager::CraftIslandSpawn()
{
    DojoHelpers->CallCraftIslandPocketActionsSpawn(Account);
}

AActor* ADojoCraftIslandManager::PlaceAssetInWorld(E_Item Item, const FIntVector& DojoPosition, bool Validated)
{

//    FIntVector LocalPosition = this->DojoPositionToLocalPosition(DojoPosition);
//    int32 Index = Chunk->ChunkBlock.IndexOfByKey(LocalPosition);
//
//    AActor* ExistingActor = nullptr;
//    if (Chunk->Access.Contains(LocalPosition))
//    {
//        ExistingActor = Chunk->Access[LocalPosition];
//    }

    // Destroy old actor if valid
//    if (ExistingActor && !IsValid(ExistingActor))
//    {
//        Chunk->Access.Remove(LocalPosition);
//        ExistingActor->Destroy();
//    }

    // Resource class to spawn
//    int32 ClassIndex = Chunk->ClassIndexBlock.IsValidIndex(Index) ? Chunk->ClassIndexBlock[Index] : 0;
//    if (!ResourceIndexClass.IsValidIndex(ClassIndex)) return nullptr;

//    TSubclassOf<AActor> SpawnClass = ResourceIndexClass[ClassIndex];
//    if (!SpawnClass) return nullptr;
//
    
    TSubclassOf<AActor> SpawnClass = nullptr;

    const void* RowPtr = DataTableHelpers::FindRowByColumnValue<int32>(ItemDataTable, FName("Index"), static_cast<int>(Item));
    if (RowPtr)
    {
        const FItemDataRow* Row = static_cast<const FItemDataRow*>(RowPtr);
        UE_LOG(LogTemp, Log, TEXT("Found item: %d"), Row->Enum);
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

//    // Transform to spawn at
    FTransform SpawnTransform = this->DojoPositionToTransform(DojoPosition);
//
//    // Spawn the actor
    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(SpawnClass, SpawnTransform);
    if (!SpawnedActor) return nullptr;
//
//    // Optional: Cast to ADefaultBlock and set data
//    if (ABaseBlock* Block = (Cast<ABaseBlock>(SpawnedActor))) {
//    {
//        Block->DojoPosition = DojoPosition;
//        Block->Item = Item;
//        Block->IsOnChain = Validated;
//    }
//
//    // Register in access lists
//    if (!Chunk->ChunkBlock.Contains(LocalPosition))
//    {
//        Chunk->ChunkBlock.Add(LocalPosition);
//        Chunk->ClassIndexBlock.Add(ClassIndex);
//    }
//
//    Chunk->Access.Add(LocalPosition, SpawnedActor);

//    return SpawnedActor;
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
