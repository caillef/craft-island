// Generated by dojo-bindgen on Wed, 26 Mar 2025 16:00:15 +0000. Do not modify this file manually.

#include "DojoHelpers.h"
#include <string>
#include <iomanip>
#include <sstream>
#include <memory>
#include "Async/Async.h"


ADojoHelpers* ADojoHelpers::Instance = nullptr;

ADojoHelpers::ADojoHelpers()
{
    Instance = this;
    subscribed = false;
}

ADojoHelpers::~ADojoHelpers()
{
    if (subscribed) {
        FDojoModule::SubscriptionCancel(subscription);
    }
}

void ADojoHelpers::Connect(const FString& torii_url, const FString& world)
{
    std::string torii_url_string = std::string(TCHAR_TO_UTF8(*torii_url));
    std::string world_string = std::string(TCHAR_TO_UTF8(*world));
    toriiClient = FDojoModule::CreateToriiClient(torii_url_string.c_str(), world_string.c_str());
    UE_LOG(LogTemp, Log, TEXT("Torii Client initialized."));
}

void ADojoHelpers::SetContractsAddresses(const TMap<FString,FString>& addresses)
{
    ContractsAddresses = addresses;
}

FAccount ADojoHelpers::CreateAccountDeprecated(const FString& rpc_url, const FString& \
             address, const FString& private_key)
{
    FAccount account;

    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));
    std::string address_string = std::string(TCHAR_TO_UTF8(*address));
    std::string private_key_string = std::string(TCHAR_TO_UTF8(*private_key));
    account.account = FDojoModule::CreateAccount(rpc_url_string.c_str(), address_string.c_str(), \
             private_key_string.c_str());
    account.Address = address;
    return account;
}

FAccount ADojoHelpers::CreateBurnerDeprecated(const FString& rpc_url, const FString& address, \
             const FString& private_key)
{
    FAccount account;

    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));
    std::string address_string = std::string(TCHAR_TO_UTF8(*address));
    std::string private_key_string = std::string(TCHAR_TO_UTF8(*private_key));
    Account *master_account = FDojoModule::CreateAccount(rpc_url_string.c_str(), \
             address_string.c_str(), private_key_string.c_str());
    if (master_account == nullptr) {
        account.Address = UTF8_TO_TCHAR("0x0");
        return account;
    }
    account.account = FDojoModule::CreateBurner(rpc_url_string.c_str(), master_account);
    if (account.account == nullptr) {
        account.Address = UTF8_TO_TCHAR("0x0");
        return account;
    }
    account.Address = FDojoModule::AccountAddress(account.account);
    return account;
}

void ADojoHelpers::ControllerGetAccountOrConnect(const FString& rpc_url, const FString& \
             chain_id)
{
    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));
    std::string chain_id_string = std::string(TCHAR_TO_UTF8(*chain_id));

    FieldElement CraftIslandPocketActionsContract;
    FDojoModule::string_to_bytes(std::string(TCHAR_TO_UTF8(*\
    this->ContractsAddresses["craft_island_pocket-actions"])), CraftIslandPocketActionsContract.data, 32);
    struct Policy policies[] = {
        { CraftIslandPocketActionsContract, "spawn", "spawn" },
        { CraftIslandPocketActionsContract, "place", "place" },
        { CraftIslandPocketActionsContract, "hit_block", "hit_block" }
    };
    int nbPolicies = 3;


    FDojoModule::ControllerGetAccountOrConnect(rpc_url_string.c_str(), chain_id_string.c_str(), \
             policies, nbPolicies, ControllerCallbackProxy);
}

void ADojoHelpers::ControllerConnect(const FString& rpc_url)
{
    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));

    FieldElement CraftIslandPocketActionsContract;
    FDojoModule::string_to_bytes(std::string(TCHAR_TO_UTF8(*\
    this->ContractsAddresses["craft_island_pocket-actions"])), CraftIslandPocketActionsContract.data, 32);
    struct Policy policies[] = {
        { CraftIslandPocketActionsContract, "spawn", "spawn" },
        { CraftIslandPocketActionsContract, "place", "place" },
        { CraftIslandPocketActionsContract, "hit_block", "hit_block" }
    };
    int nbPolicies = 3;

    FDojoModule::ControllerConnect(rpc_url_string.c_str(), policies, nbPolicies, ControllerCallbackProxy);
}
//
//void ADojoHelpers::ControllerGetAccountOrConnectMobile(const FString& rpc_url, const FString& \
//             chain_id)
//{
//    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));
//    std::string chain_id_string = std::string(TCHAR_TO_UTF8(*chain_id));
//
//    FieldElement CraftIslandPocketActionsContract;
//    FDojoModule::string_to_bytes(std::string(TCHAR_TO_UTF8(*\
//    this->ContractsAddresses["craft_island_pocket-actions"])), CraftIslandPocketActionsContract.data, 32);
//    struct Policy policies[] = {
//        { CraftIslandPocketActionsContract, "spawn", "spawn" },
//        { CraftIslandPocketActionsContract, "place", "place" },
//        { CraftIslandPocketActionsContract, "hit_block", "hit_block" }
//    };
//    int nbPolicies = 3;
//
//
//    FDojoModule::ControllerGetAccountOrConnectMobile(rpc_url_string.c_str(), chain_id_string.c_str(), \
//             policies, nbPolicies, ControllerCallbackProxy, ControllerCallbackUrlProxy);
//}

void ADojoHelpers::ControllerCallbackProxy(ControllerAccount *account)
{
    if (!Instance) return;
    Instance->ControllerAccountCallback(account);
}

//void ADojoHelpers::ControllerCallbackUrlProxy(const char *url)
//{
//    if (!Instance) return;
//    Instance->ControllerAccountCallbackUrl(url);
//}

void ADojoHelpers::ControllerAccountCallback(ControllerAccount *account)
{
    // Going back to Blueprint thread to broadcast the account
    Async(EAsyncExecution::TaskGraphMainThread, [this, account]() {
        FControllerAccount controllerAccount;
        controllerAccount.account = account;
        controllerAccount.Address = FDojoModule::ControllerAccountAddress(account);
        FOnDojoControllerAccount.Broadcast(controllerAccount);
    });
}

//void ADojoHelpers::ControllerAccountCallbackUrl(const char *url)
//{
//    // Going back to Blueprint thread to broadcast the account
//    Async(EAsyncExecution::TaskGraphMainThread, [this, url]() {
//        FString final_url(UTF8_TO_TCHAR(url));
//        FOnDojoControllerURL.Broadcast(final_url);
//    });
//}

void ADojoHelpers::ExecuteRawDeprecated(const FAccount& account, const FString& to, const \
             FString& selector, const FString& calldataParameter)
{
    Async(EAsyncExecution::Thread, [this, account, to, selector, calldataParameter]()
    {
        std::vector<std::string> felts;
        if (strcmp(TCHAR_TO_UTF8(*calldataParameter), "") != 0) {
            TArray<FString> Out;
            calldataParameter.ParseIntoArray(Out,TEXT(","),true);
            for (int i = 0; i < Out.Num(); i++) {
                std::string felt = TCHAR_TO_UTF8(*Out[i]);
                felts.push_back(felt);
            }
        }
        FDojoModule::ExecuteRaw(account.account, TCHAR_TO_UTF8(*to), TCHAR_TO_UTF8(*selector), \
             felts);
    });
}

void ADojoHelpers::ExecuteFromOutside(const FControllerAccount& account, const FString& to, \
             const FString& selector, const FString& calldataParameter)
{
    Async(EAsyncExecution::Thread, [this, account, to, selector, calldataParameter]()
    {
        std::vector<std::string> felts;
        if (strcmp(TCHAR_TO_UTF8(*calldataParameter), "") != 0) {
            TArray<FString> Out;
            calldataParameter.ParseIntoArray(Out,TEXT(","),true);
            for (int i = 0; i < Out.Num(); i++) {
                std::string felt = TCHAR_TO_UTF8(*Out[i]);
                felts.push_back(felt);
            }
        }
        FDojoModule::ExecuteFromOutside(account.account, TCHAR_TO_UTF8(*to), \
             TCHAR_TO_UTF8(*selector), felts);
    });
}

void ADojoHelpers::FetchExistingModels()
{
    Async(EAsyncExecution::Thread, [this]()
            {
        ResultCArrayEntity resEntities =
            FDojoModule::GetEntities(toriiClient, "{ not used }");
        if (resEntities.tag == ErrCArrayEntity) {
            UE_LOG(LogTemp, Log, TEXT("Failed to fetch entities: %hs"), \
             resEntities.err.message);
            return;
        }
        CArrayEntity *entities = &resEntities.ok;

        for (int i = 0; i < entities->data_len; i++) {
            CArrayStruct* models = &entities->data[i].models;
            this->ParseModelsAndSend(models);
        }
        FDojoModule::CArrayFree(entities->data, entities->data_len);
    });
    }

void ADojoHelpers::SubscribeOnDojoModelUpdate()
{
    UE_LOG(LogTemp, Log, TEXT("Subscribing to entity update."));
    if (subscribed) {
        UE_LOG(LogTemp, Log, TEXT("Warning: cancelled, already subscribed."));
        return;
    }
    if (toriiClient == nullptr) {
        UE_LOG(LogTemp, Log, TEXT("Error: Torii Client is not initialized."));
        return;
    }
    subscribed = true;
    struct ResultSubscription res =
        FDojoModule::OnEntityUpdate(toriiClient, "{}", nullptr, CallbackProxy);
    subscription = res.ok;
}

void ADojoHelpers::CallbackProxy(struct FieldElement key, struct CArrayStruct models)
{
    if (!Instance) return;
    Instance->ParseModelsAndSend(&models);
}

template<typename T>
static void ConvertTyToUnrealEngineType(const Member* member, const char* expectedName, const \
             char* expectedType, T& output);

class TypeConverter {
public:
    static FString ConvertToFString(const Member* member) {
        switch (member->ty->primitive.tag) {
            case Primitive_Tag::ContractAddress:
                return FDojoModule::bytes_to_fstring(member->ty->primitive.contract_address.data, \
             32);
            case Primitive_Tag::I128:
                return FDojoModule::bytes_to_fstring(member->ty->primitive.i128, 16);
            case Primitive_Tag::U128:
                return FDojoModule::bytes_to_fstring(member->ty->primitive.u128, 16);
            case Primitive_Tag::Felt252:
                return FDojoModule::bytes_to_fstring(member->ty->primitive.felt252.data, 32);
            case Primitive_Tag::ClassHash:
                return FDojoModule::bytes_to_fstring(member->ty->primitive.class_hash.data, 32);
            case Primitive_Tag::EthAddress:
                return FDojoModule::bytes_to_fstring(member->ty->primitive.eth_address.data, 32);
            default:
                return FString();
        }
    }

    static int ConvertToInt(const Member* member) {
        switch(member->ty->primitive.tag) {
            case Primitive_Tag::I8:  return member->ty->primitive.i8;
            case Primitive_Tag::I16: return member->ty->primitive.i16;
            case Primitive_Tag::I32: return member->ty->primitive.i32;
            case Primitive_Tag::U8:  return member->ty->primitive.u8;
            case Primitive_Tag::U16: return member->ty->primitive.u16;
            case Primitive_Tag::U32: return member->ty->primitive.u32;
            default: return 0;
        }
    }

    static int64 ConvertToLong(const Member* member) {
        switch(member->ty->primitive.tag) {
            case Primitive_Tag::I64: return member->ty->primitive.i64;
            case Primitive_Tag::U64: return member->ty->primitive.u64;
            default: return 0;
        }
    }

    static bool ConvertToBool(const Member* member) {
        if (member->ty->primitive.tag == Primitive_Tag::Bool) {
            return member->ty->primitive.bool_;
        }
        return false;
    }

    template<typename T>
    static TArray<T> ConvertToArray(const Member* member) {
        TArray<T> result;
        if (member->ty->tag == Ty_Tag::Array_) {
            // Implémenter la logique de conversion d'array
            // Accéder à member->ty->array pour les données
        }
        return result;
    }

    
};

template<typename T>
static TArray<FString> ConvertToFeltHexa(const T& value, const char* valueType) {
    if constexpr (std::is_same_v<T, FString>) {
        if (strcmp(valueType, "i128") == 0 ||
            strcmp(valueType, "u128") == 0 ||
            strcmp(valueType, "u256") == 0 ||
            strcmp(valueType, "felt252") == 0 ||
            strcmp(valueType, "bytes31") == 0 ||
            strcmp(valueType, "ClassHash") == 0 ||
            strcmp(valueType, "ContractAddress") == 0 ||
            strcmp(valueType, "ByteArray") == 0) {

            // Remove "0x" if present
            FString hexValue = value;
            if (hexValue.StartsWith(TEXT("0x"))) {
                hexValue.RightChopInline(2);
            }

            // Pad with leading zeros to make it 64 characters
            while (hexValue.Len() < 64) {
                hexValue = TEXT("0") + hexValue;
            }

            // Truncate if longer than 64 characters
            if (hexValue.Len() > 64) {
                hexValue = hexValue.Right(64);
            }

            return TArray<FString>{TEXT("0x") + hexValue};
        }
    }
    else if constexpr (std::is_same_v<T, int>) {
        if (strcmp(valueType, "i8") == 0 ||
            strcmp(valueType, "i16") == 0 ||
            strcmp(valueType, "i32") == 0 ||
            strcmp(valueType, "u8") == 0 ||
            strcmp(valueType, "u16") == 0 ||
            strcmp(valueType, "u32") == 0) {
            FString hexValue = FString::Printf(TEXT("%X"), static_cast<int>(value));
            while (hexValue.Len() < 64) {
                hexValue = TEXT("0") + hexValue;
            }
            return TArray<FString>{TEXT("0x") + hexValue};
        }
    }
    else if constexpr (std::is_same_v<T, int64>) {
        if (strcmp(valueType, "u64") == 0 ||
            strcmp(valueType, "i64") == 0) {
            FString hexValue = FString::Printf(TEXT("%X"), static_cast<int64>(value));
            while (hexValue.Len() < 64) {
                hexValue = TEXT("0") + hexValue;
            }
            return TArray<FString>{TEXT("0x") + hexValue};
        }
    }
    else if constexpr (std::is_same_v<T, bool>) {
        if (strcmp(valueType, "bool") == 0) {
            FString hexValue = FString::Printf(TEXT("%X"), value ? 1 : 0);
            while (hexValue.Len() < 64) {
                hexValue = TEXT("0") + hexValue;
            }
            return TArray<FString>{TEXT("0x") + hexValue};
        }
    }
    else if constexpr (std::is_same_v<T, TArray<int>>) {
        if (strcmp(valueType, "array") == 0) {
            TArray<FString> strings;
            FString hexValue = FString::Printf(TEXT("%X"), static_cast<int>(value.Num()));
            while (hexValue.Len() < 64) {
                hexValue = TEXT("0") + hexValue;
            }
            strings.Add(TEXT("0x") + hexValue);

            // Add array elements
            for (const int& element : value) {
                FString elementHexValue = FString::Printf(TEXT("%X"), element);
                while (elementHexValue.Len() < 64) {
                    elementHexValue = TEXT("0") + elementHexValue;
                }
                strings.Add(TEXT("0x") + elementHexValue);
            }

            return strings;
        }
    }
    
    // Default return value padded to 64 characters
    return TArray<FString>{TEXT("0x") + FString::ChrN(64, TEXT('0'))};
}

template<typename T>
static void ConvertTyToUnrealEngineType(const Member* member, const char* expectedName, const \
             char* expectedType, T& output) {
    if (strcmp(member->name, expectedName) != 0) {
        return;
    }

    if constexpr (std::is_same_v<T, FString>) {
        if (strcmp(expectedType, "i128") == 0 ||
            strcmp(expectedType, "u128") == 0 ||
            strcmp(expectedType, "u256") == 0 ||
            strcmp(expectedType, "felt252") == 0 ||
            strcmp(expectedType, "bytes31") == 0 ||
            strcmp(expectedType, "ClassHash") == 0 ||
            strcmp(expectedType, "ContractAddress") == 0 ||
            strcmp(expectedType, "ByteArray") == 0) {
            output = TypeConverter::ConvertToFString(member);
        }
    }
    else if constexpr (std::is_same_v<T, int>) {
        if (strcmp(expectedType, "i8") == 0 ||
        strcmp(expectedType, "i16") == 0 ||
        strcmp(expectedType, "i32") == 0 ||
        strcmp(expectedType, "u8") == 0 ||
        strcmp(expectedType, "u16") == 0 ||
        strcmp(expectedType, "u32") == 0) {
            output = TypeConverter::ConvertToInt(member);
        }
    }
    else if constexpr (std::is_same_v<T, int64>) {
        if (strcmp(expectedType, "i64") == 0 ||
        strcmp(expectedType, "u64") == 0 ||
        strcmp(expectedType, "usize") == 0) {
            output = TypeConverter::ConvertToLong(member);
        }
    }
    else if constexpr (std::is_same_v<T, bool>) {
        if (strcmp(expectedType, "bool") == 0) {
            output = TypeConverter::ConvertToBool(member);
        }
    }
    
}



UDojoModel* ADojoHelpers::parseCraftIslandPocketGatherableResourceModel(struct Struct* model)
{
    UDojoModelCraftIslandPocketGatherableResource* Model = NewObject<UDojoModelCraftIslandPocketGatherableResource>();
    CArrayMember* members = &model->children;

    for (int k = 0; k < members->data_len; k++) {
        Member* member = &members->data[k];
        ConvertTyToUnrealEngineType(member, "island_id", "felt252", Model->IslandId);
        ConvertTyToUnrealEngineType(member, "chunk_id", "u128", Model->ChunkId);
        ConvertTyToUnrealEngineType(member, "position", "u8", Model->Position);
        ConvertTyToUnrealEngineType(member, "resource_id", "u32", Model->ResourceId);
        ConvertTyToUnrealEngineType(member, "planted_at", "u64", Model->PlantedAt);
        ConvertTyToUnrealEngineType(member, "next_harvest_at", "u64", Model->NextHarvestAt);
        ConvertTyToUnrealEngineType(member, "harvested_at", "u64", Model->HarvestedAt);
        ConvertTyToUnrealEngineType(member, "max_harvest", "u8", Model->MaxHarvest);
        ConvertTyToUnrealEngineType(member, "remained_harvest", "u8", Model->RemainedHarvest);
        ConvertTyToUnrealEngineType(member, "destroyed", "bool", Model->Destroyed);
    }

    FDojoModule::CArrayFree(members->data, members->data_len);
    return Model;
}

UDojoModel* ADojoHelpers::parseCraftIslandPocketInventoryModel(struct Struct* model)
{
    UDojoModelCraftIslandPocketInventory* Model = NewObject<UDojoModelCraftIslandPocketInventory>();
    CArrayMember* members = &model->children;

    for (int k = 0; k < members->data_len; k++) {
        Member* member = &members->data[k];
        ConvertTyToUnrealEngineType(member, "owner", "ContractAddress", Model->Owner);
        ConvertTyToUnrealEngineType(member, "id", "u16", Model->Id);
        ConvertTyToUnrealEngineType(member, "inventory_type", "u8", Model->InventoryType);
        ConvertTyToUnrealEngineType(member, "slots1", "felt252", Model->Slots1);
        ConvertTyToUnrealEngineType(member, "slots2", "felt252", Model->Slots2);
        ConvertTyToUnrealEngineType(member, "slots3", "felt252", Model->Slots3);
        ConvertTyToUnrealEngineType(member, "slots4", "felt252", Model->Slots4);
    }

    FDojoModule::CArrayFree(members->data, members->data_len);
    return Model;
}

UDojoModel* ADojoHelpers::parseCraftIslandPocketIslandChunkModel(struct Struct* model)
{
    UDojoModelCraftIslandPocketIslandChunk* Model = NewObject<UDojoModelCraftIslandPocketIslandChunk>();
    CArrayMember* members = &model->children;

    for (int k = 0; k < members->data_len; k++) {
        Member* member = &members->data[k];
        ConvertTyToUnrealEngineType(member, "island_id", "felt252", Model->IslandId);
        ConvertTyToUnrealEngineType(member, "chunk_id", "u128", Model->ChunkId);
        ConvertTyToUnrealEngineType(member, "blocks1", "u128", Model->Blocks1);
        ConvertTyToUnrealEngineType(member, "blocks2", "u128", Model->Blocks2);
    }

    FDojoModule::CArrayFree(members->data, members->data_len);
    return Model;
}

UDojoModel* ADojoHelpers::parseCraftIslandPocketPlayerDataModel(struct Struct* model)
{
    UDojoModelCraftIslandPocketPlayerData* Model = NewObject<UDojoModelCraftIslandPocketPlayerData>();
    CArrayMember* members = &model->children;

    for (int k = 0; k < members->data_len; k++) {
        Member* member = &members->data[k];
        ConvertTyToUnrealEngineType(member, "player", "ContractAddress", Model->Player);
        ConvertTyToUnrealEngineType(member, "onboarding_step", "u8", Model->OnboardingStep);
        ConvertTyToUnrealEngineType(member, "coins", "u32", Model->Coins);
        ConvertTyToUnrealEngineType(member, "current_island", "felt252", Model->CurrentIsland);
    }

    FDojoModule::CArrayFree(members->data, members->data_len);
    return Model;
}

UDojoModel* ADojoHelpers::parseCraftIslandPocketPlayerStatsModel(struct Struct* model)
{
    UDojoModelCraftIslandPocketPlayerStats* Model = NewObject<UDojoModelCraftIslandPocketPlayerStats>();
    CArrayMember* members = &model->children;

    for (int k = 0; k < members->data_len; k++) {
        Member* member = &members->data[k];
        ConvertTyToUnrealEngineType(member, "player", "ContractAddress", Model->Player);
        ConvertTyToUnrealEngineType(member, "miner_level", "u8", Model->MinerLevel);
        ConvertTyToUnrealEngineType(member, "lumberjack_level", "u8", Model->LumberjackLevel);
        ConvertTyToUnrealEngineType(member, "farmer_level", "u8", Model->FarmerLevel);
        ConvertTyToUnrealEngineType(member, "miner_xp", "u32", Model->MinerXp);
        ConvertTyToUnrealEngineType(member, "lumberjack_xp", "u32", Model->LumberjackXp);
        ConvertTyToUnrealEngineType(member, "farmer_xp", "u32", Model->FarmerXp);
    }

    FDojoModule::CArrayFree(members->data, members->data_len);
    return Model;
}

void ADojoHelpers::ParseModelsAndSend(struct CArrayStruct* models)
{
    if (!models || !models->data)
    {
        UE_LOG(LogTemp, Warning, TEXT("ParseModelsAndSend: Invalid input models"));
        return;
    }

    TArray<UDojoModel*> ParsedModels;
    ParsedModels.Reserve(models->data_len);

    for (int32 Index = 0; Index < models->data_len; ++Index)
    {
        const char* ModelName = models->data[Index].name;
        if (!ModelName)
        {
            UE_LOG(LogTemp, Warning, TEXT("ParseModelsAndSend: null model name (%d)"), Index);
            continue;
        }

        UDojoModel* ParsedModel = nullptr;
        
        if (strcmp(ModelName, "craft_island_pocket-GatherableResource") == 0)
        {
            ParsedModel = \
                     ADojoHelpers::parseCraftIslandPocketGatherableResourceModel(&\
                     models->data[Index]);
        }
        else if (strcmp(ModelName, "craft_island_pocket-Inventory") == 0)
        {
            ParsedModel = \
                     ADojoHelpers::parseCraftIslandPocketInventoryModel(&\
                     models->data[Index]);
        }
        else if (strcmp(ModelName, "craft_island_pocket-IslandChunk") == 0)
        {
            ParsedModel = \
                     ADojoHelpers::parseCraftIslandPocketIslandChunkModel(&\
                     models->data[Index]);
        }
        else if (strcmp(ModelName, "craft_island_pocket-PlayerData") == 0)
        {
            ParsedModel = \
                     ADojoHelpers::parseCraftIslandPocketPlayerDataModel(&\
                     models->data[Index]);
        }
        else if (strcmp(ModelName, "craft_island_pocket-PlayerStats") == 0)
        {
            ParsedModel = \
                     ADojoHelpers::parseCraftIslandPocketPlayerStatsModel(&\
                     models->data[Index]);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ParseModelsAndSend: Unknown model type %s"), \
             UTF8_TO_TCHAR(ModelName));
            continue;
        }

        if (ParsedModel)
        {
            ParsedModel->DojoModelType = ModelName;
            ParsedModels.Add(ParsedModel);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ParseModelsAndSend: Failed to parse model %s"), \
             UTF8_TO_TCHAR(ModelName));
        }
    }

    if (ParsedModels.Num() > 0)
    {
        AsyncTask(ENamedThreads::GameThread, [this, ParsedModels = MoveTemp(ParsedModels)]()
        {
            for (UDojoModel* Model : ParsedModels)
            {
                if (IsValid(Model))
                {
                    OnDojoModelUpdated.Broadcast(Model);
                }
            }
        });
    }

    // Cleanup
    if (models->data)
    {
        FDojoModule::CArrayFree(models->data, models->data_len);
    }
}


void ADojoHelpers::CallCraftIslandPocketActionsSpawn(const FAccount& account) {
    TArray<FString> args;
    
    this->ExecuteRawDeprecated(account, this->ContractsAddresses["craft_island_pocket-actions"], \
                     TEXT("spawn"), FString::Join(args, TEXT(",")));
}

void ADojoHelpers::CallControllerCraftIslandPocketActionsSpawn(const FControllerAccount& \
                     account) {
    TArray<FString> args;
    
    this->ExecuteFromOutside(account, this->ContractsAddresses["craft_island_pocket-actions"], \
                     TEXT("spawn"), FString::Join(args, TEXT(",")));
}

void ADojoHelpers::CallCraftIslandPocketActionsPlace(const FAccount& account, int64 x, int64 y, int64 z, int resource_id) {
    TArray<FString> args;
    args.Append(ConvertToFeltHexa<int64>(x, "u64"));
    args.Append(ConvertToFeltHexa<int64>(y, "u64"));
    args.Append(ConvertToFeltHexa<int64>(z, "u64"));
    args.Append(ConvertToFeltHexa<int>(resource_id, "u32"));
    this->ExecuteRawDeprecated(account, this->ContractsAddresses["craft_island_pocket-actions"], \
                     TEXT("place"), FString::Join(args, TEXT(",")));
}

void ADojoHelpers::CallControllerCraftIslandPocketActionsPlace(const FControllerAccount& \
                     account, int64 x, int64 y, int64 z, int resource_id) {
    TArray<FString> args;
    args.Append(ConvertToFeltHexa<int64>(x, "u64"));
    args.Append(ConvertToFeltHexa<int64>(y, "u64"));
    args.Append(ConvertToFeltHexa<int64>(z, "u64"));
    args.Append(ConvertToFeltHexa<int>(resource_id, "u32"));
    this->ExecuteFromOutside(account, this->ContractsAddresses["craft_island_pocket-actions"], \
                     TEXT("place"), FString::Join(args, TEXT(",")));
}

void ADojoHelpers::CallCraftIslandPocketActionsHitBlock(const FAccount& account, int64 x, int64 y, int64 z, int hp) {
    TArray<FString> args;
    args.Append(ConvertToFeltHexa<int64>(x, "u64"));
    args.Append(ConvertToFeltHexa<int64>(y, "u64"));
    args.Append(ConvertToFeltHexa<int64>(z, "u64"));
    args.Append(ConvertToFeltHexa<int>(hp, "u32"));
    this->ExecuteRawDeprecated(account, this->ContractsAddresses["craft_island_pocket-actions"], \
                     TEXT("hit_block"), FString::Join(args, TEXT(",")));
}

void ADojoHelpers::CallControllerCraftIslandPocketActionsHitBlock(const FControllerAccount& \
                     account, int64 x, int64 y, int64 z, int hp) {
    TArray<FString> args;
    args.Append(ConvertToFeltHexa<int64>(x, "u64"));
    args.Append(ConvertToFeltHexa<int64>(y, "u64"));
    args.Append(ConvertToFeltHexa<int64>(z, "u64"));
    args.Append(ConvertToFeltHexa<int>(hp, "u32"));
    this->ExecuteFromOutside(account, this->ContractsAddresses["craft_island_pocket-actions"], \
                     TEXT("hit_block"), FString::Join(args, TEXT(",")));
}

