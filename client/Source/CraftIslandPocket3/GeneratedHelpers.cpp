//
//  GeneratedHelpers.cpp
//  dojo_starter_ue5 (Mac)
//
//  Created by Corentin Cailleaud on 21/10/2024.
//  Copyright © 2024 Epic Games, Inc. All rights reserved.
//

#include "GeneratedHelpers.h"
#include <string>
#include <iomanip>
#include <sstream>
#include <memory>
#include "Async/Async.h"

AGeneratedHelpers::AGeneratedHelpers()
{
    
}

void AGeneratedHelpers::Connect(const FString& torii_url, const FString& rpc_url, const FString& world)
{
    std::string torii_url_string = std::string(TCHAR_TO_UTF8(*torii_url));
    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));
    std::string world_string = std::string(TCHAR_TO_UTF8(*world));
    Async(EAsyncExecution::Thread, [this, torii_url_string, rpc_url_string, world_string]()
          {
//        ToriiClient *toriiClientTmp = FDojoModule::CreateToriiClient(torii_url_string.c_str(), rpc_url_string.c_str(), world_string.c_str());
//        Async(EAsyncExecution::TaskGraphMainThread, [this, toriiClientTmp]()
//              {
//            toriiClient = toriiClientTmp;
//        });
    });
}

static std::string bytes_to_string(const uint8_t* data, size_t length) {
    std::stringstream ss;
    ss << "0x";
    for (size_t i = 0; i < length; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return ss.str();
}

static FString bytes_to_fstring(const uint8_t* data, size_t length, bool addPrefix = true) {
    if (data == nullptr || length == 0)
        return TEXT("0x");

    FString result = addPrefix ? TEXT("0x") : TEXT("");
    for (size_t i = 0; i < length; ++i) {
        result += FString::Printf(TEXT("%02x"), data[i]);
    }
    return result;
}

FAccount AGeneratedHelpers::CreateAccount(const FString& rpc_url, const FString& address, const FString& private_key)
{
    FAccount account;

    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));
    std::string address_string = std::string(TCHAR_TO_UTF8(*address));
    std::string private_key_string = std::string(TCHAR_TO_UTF8(*private_key));
    account.account = FDojoModule::CreateAccount(rpc_url_string.c_str(), address_string.c_str(), private_key_string.c_str());
    account.Address = address;
    return account;
}

FAccount AGeneratedHelpers::CreateBurner(const FString& rpc_url, const FString& address, const FString& private_key)
{
    FAccount account;

    std::string rpc_url_string = std::string(TCHAR_TO_UTF8(*rpc_url));
    std::string address_string = std::string(TCHAR_TO_UTF8(*address));
    std::string private_key_string = std::string(TCHAR_TO_UTF8(*private_key));
    Account *master_account = FDojoModule::CreateAccount(rpc_url_string.c_str(), address_string.c_str(), private_key_string.c_str());
    account.account = FDojoModule::CreateBurner(rpc_url_string.c_str(), master_account);
    if (account.account == nullptr) {
        account.Address = UTF8_TO_TCHAR("0x0");
        return account;
    }
//    std::string burner_address = FDojoModule::AccountAddress(account.account);
//    account.Address = UTF8_TO_TCHAR(burner_address.c_str());
//    activeBurner = account.account;
    return account;
}

void AGeneratedHelpers::ExecuteRaw(const FAccount& account, const FString& to, const FString& selector, const FString& calldataParameter)
{
    Async(EAsyncExecution::Thread, [this, to, selector, calldataParameter]()
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
        FDojoModule::ExecuteRaw(activeBurner, TCHAR_TO_UTF8(*to), TCHAR_TO_UTF8(*selector), felts);
    });
}

void AGeneratedHelpers::GetAllEntities()
{
    // Run the heavy computation on a separate thread
    Async(EAsyncExecution::Thread, [this]()
          {
        TArray<FModelIslandChunk> IslandChunks;
        TArray<FModelGatherableResource> GatherableResources;
        
        ResultCArrayEntity resEntities = FDojoModule::GetEntities(toriiClient, "{ not used }");
        if (resEntities.tag == ErrCArrayEntity) {
            UE_LOG(LogTemp, Log, TEXT("Failed to fetch entities: %hs"), resEntities.err.message);
            return;
        }
        CArrayEntity *entities = &resEntities.ok;
        
        UE_LOG(LogTemp, Log, TEXT("NB ENTITIES FETCH: %d"), entities->data_len);
        for (int i = 0; i < entities->data_len; i++) {
            CArrayStruct* models = &entities->data[i].models;
            
            for (int j = 0; j < models->data_len; j++) {
                const char* name = models->data[j].name;
                
                if (strcmp(name, "craft_island_pocket-IslandChunk") == 0) {
                    FModelIslandChunk chunk;
                    CArrayMember* members = &models->data[j].children;
                    FString blocks1;
                    FString blocks2;
                    
                    for (int k = 0; k < members->data_len; k++) {
                        Member* member = &members->data[k];
                        if (strcmp("island_id", member->name) == 0) {
                            chunk.island_id = bytes_to_fstring(member->ty->primitive.felt252.data, 32);
                        } else if (strcmp("chunk_id", member->name) == 0) {
                            chunk.chunk_id = bytes_to_fstring(member->ty->primitive.u128, 16);
                        } else if (strcmp("blocks1", member->name) == 0) {
                            blocks1 = bytes_to_fstring(member->ty->primitive.u128, 16);
                        } else if (strcmp("blocks2", member->name) == 0) {
                            blocks2 = bytes_to_fstring(member->ty->primitive.u128, 16, false);
                        }
                    }
                    chunk.blocks = blocks1 + blocks2;
                    IslandChunks.Add(chunk);
                }
                
                if (strcmp(name, "craft_island_pocket-GatherableResource") == 0) {
                    FModelGatherableResource resource;
                    CArrayMember* members = &models->data[j].children;
                    
                    for (int k = 0; k < members->data_len; k++) {
                        Member* member = &members->data[k];
                        if (strcmp("island_id", member->name) == 0) {
                            resource.island_id = bytes_to_fstring(member->ty->primitive.felt252.data, 32);
                        } else if (strcmp("chunk_id", member->name) == 0) {
                            resource.chunk_id = bytes_to_fstring(member->ty->primitive.u128, 16);
                        } else if (strcmp("position", member->name) == 0) {
                            resource.position = member->ty->primitive.u8;
                        } else if (strcmp("resource_id", member->name) == 0) {
                            resource.resource_id = member->ty->primitive.u32;
                        } else if (strcmp("planted_at", member->name) == 0) {
                            resource.planted_at = member->ty->primitive.u64;
                        } else if (strcmp("next_harvest_at", member->name) == 0) {
                            resource.next_harvest_at = member->ty->primitive.u64;
                        } else if (strcmp("harvested_at", member->name) == 0) {
                            resource.harvested_at = member->ty->primitive.u64;
                        } else if (strcmp("max_harvest", member->name) == 0) {
                            resource.max_harvest = member->ty->primitive.u8;
                        } else if (strcmp("remained_harvest", member->name) == 0) {
                            resource.remained_harvest = member->ty->primitive.u8;
                        } else if (strcmp("destroyed", member->name) == 0) {
                            resource.destroyed = member->ty->primitive.u8 == 1;
                        }
                    }
                    GatherableResources.Add(resource);
                }
            }
        }
        
        // Send results back to the game thread
        Async(EAsyncExecution::TaskGraphMainThread, [this, IslandChunks, GatherableResources]()
              {
            for (const FModelIslandChunk& Chunk : IslandChunks) {
                OnIslandChunkUpdated.Broadcast(Chunk);
            }
            for (const FModelGatherableResource& Resource : GatherableResources) {
                OnGatherableResourceUpdated.Broadcast(Resource);
            }
        });
    });
    
    
    
    
    
//    // Run the GetAllEntities function on a separate thread
//    Async(EAsyncExecution::Thread, [this]()
//    {
//
//    //UE_LOG(LogTemp, Log, TEXT("GetAllEntities!"));
//
//    ResultCArrayEntity resEntities = FDojoModule::GetEntities(toriiClient, "{ not used }");
//    if (resEntities.tag == ErrCArrayEntity) {
//        UE_LOG(LogTemp, Log, TEXT("Failed to fetch entities: %hs"), resEntities.err.message);
//        return;
//    }
//    CArrayEntity entities = resEntities.ok;
//
//    //UE_LOG(LogTemp, Log, TEXT("Number of entities: %d"), entities.data_len);
//
//    for (int i = 0; i < entities.data_len; i++) {
//        CArrayStruct *models = &entities.data[i].models;
//
//        for (int j = 0; j < models->data_len; j++) {
//            const char *name = models->data[j].name;
//            //UE_LOG(LogTemp, Log, TEXT("Name model: %hs"), name);
//
//            if (strcmp(name, "craft_island_pocket-IslandChunk") == 0) {
//                FModelIslandChunk chunk;
//                CArrayMember *members = &models->data[j].children;
//                std::string blocks1;
//                std::string blocks2;
//                for (int k = 0; k < members->data_len; k++) {
//                    Member *member = &members->data[k];
//                    if (strcmp("island_id", member->name) == 0) {
//                        // get felt252/ContractAddress
//                        std::string islandId = bytes_to_string(member->ty->primitive.felt252.data, 32);
//                        FString strIslandId(islandId.c_str());
//                        chunk.island_id = strIslandId;
//                    } else if (strcmp("chunk_id", member->name) == 0) {
//                        // get u128
//                        std::string chunkId = bytes_to_string(member->ty->primitive.u128, 16);
//                        FString strChunkId(chunkId.c_str());
//                        chunk.chunk_id = strChunkId;
//                    } else if (strcmp("blocks1", member->name) == 0) {
//                        // get u128
//                        blocks1 = bytes_to_string(member->ty->primitive.u128, 16);
//                    } else if (strcmp("blocks2", member->name) == 0) {
//                        // get u128
//                        blocks2 = bytes_to_string(member->ty->primitive.u128, 16);
//                    }
//                }
//                FString strBlocks((blocks1 + blocks2.substr(2)).c_str());
//                chunk.blocks = strBlocks;
//                OnIslandChunkUpdated.Broadcast(chunk);
//                //Arr.Add(chunk);
//            }
//            if (strcmp(name, "craft_island_pocket-GatherableResource") == 0) {
//                FModelGatherableResource resource;
//                CArrayMember *members = &models->data[j].children;
//                for (int k = 0; k < members->data_len; k++) {
//                    Member *member = &members->data[k];
//                    if (strcmp("island_id", member->name) == 0) {
//                        // get felt252/ContractAddress
//                        std::string islandId = bytes_to_string(member->ty->primitive.felt252.data, 32);
//                        FString strIslandId(islandId.c_str());
//                        resource.island_id = strIslandId;
//                    } else if (strcmp("chunk_id", member->name) == 0) {
//                        // get u128
//                        std::string chunkId = bytes_to_string(member->ty->primitive.u128, 16);
//                        FString strChunkId(chunkId.c_str());
//                        resource.chunk_id = strChunkId;
//                    } else if (strcmp("position", member->name) == 0) {
//                        resource.position = member->ty->primitive.u8;
//                    } else if (strcmp("resource_id", member->name) == 0) {
//                        resource.resource_id = member->ty->primitive.u32;
//                    } else if (strcmp("planted_at", member->name) == 0) {
//                        resource.planted_at = member->ty->primitive.u64;
//                    } else if (strcmp("next_harvest_at", member->name) == 0) {
//                        resource.next_harvest_at = member->ty->primitive.u64;
//                    } else if (strcmp("harvested_at", member->name) == 0) {
//                        resource.harvested_at = member->ty->primitive.u64;
//                    } else if (strcmp("max_harvest", member->name) == 0) {
//                        resource.max_harvest = member->ty->primitive.u8;
//                    } else if (strcmp("remained_harvest", member->name) == 0) {
//                        resource.remained_harvest = member->ty->primitive.u8;
//                    } else if (strcmp("destroyed", member->name) == 0) {
//                        resource.destroyed = member->ty->primitive.u8 == 1;
//                    }
//                }
//                OnGatherableResourceUpdated.Broadcast(resource);
//            }
//        }
//    }
//        
//    });
//    return Arr;
}
//
//void AGeneratedHelpers::SubscribeOnEntityPositionUpdate()
//{
//    UE_LOG(LogTemp, Log, TEXT("Start listening to entity update"));
//    FDojoModule::OnEntityUpdate(toriiClient, "{}", nullptr, [](struct FieldElement key, struct CArrayStruct models, void *user_data) {
//        UE_LOG(LogTemp, Log, TEXT("ON ENTITY UPDATE POSITION"));
//        FModelPosition pos;
//        for (int j = 0; j < models.data_len; j++) {
//            const char *name = models.data[j].name;
//            if (strcmp(name, "dojo_starter-Position") != 0) {
//                continue;
//            }
//            struct FModelPosition pos;
//            CArrayMember *members = &models.data[j].children;
//            for (int k = 0; k < members->data_len; k++) {
//                Member *member = &members->data[k];
//                if (strcmp("player", member->name) == 0) {
//                    std::string contractAddress = bytes_to_string(member->ty->primitive.contract_address.data, 32);
//                    FString strContractAddress(contractAddress.c_str());
//                    pos.player = strContractAddress;
//                } else if (strcmp("vec", member->name) == 0) {
//                    int x = member->ty->struct_.children.data[0].ty->primitive.u32;
//                    int y = member->ty->struct_.children.data[1].ty->primitive.u32;
//                    pos.vec = {
//                        .x = x, .y = y
//                    };
//                }
//            }
//            ((AGeneratedHelpers*)user_data)->OnPositionUpdated.Broadcast(pos);
//            UE_LOG(LogTemp, Log, TEXT("ON ENTITY UPDATE POSITION"));
//        }
//    });
//}
