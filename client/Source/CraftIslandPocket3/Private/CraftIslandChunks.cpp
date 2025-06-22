// Fill out your copyright notice in the Description page of Project Settings.


#include "CraftIslandChunks.h"
#include <memory>

void UCraftIslandChunks::HandleCraftIslandModel(UDojoModel* model, UPARAM(ref) TMap<FString, FSpaceChunks>& RawSpaces)
{
    FString name = model->DojoModelType;
    FSpaceChunks* data;

    if (name == "craft_island_pocket-IslandChunk") {
        UDojoModelCraftIslandPocketIslandChunk* chunk = reinterpret_cast<UDojoModelCraftIslandPocketIslandChunk*>(model);
        FString Combined = chunk->IslandOwner + FString::FromInt(chunk->IslandId);

        data = &RawSpaces.FindOrAdd(Combined);
        data->Chunks.Add(chunk->ChunkId, chunk);
    }
    else if (name == "craft_island_pocket-GatherableResource") {
        UDojoModelCraftIslandPocketGatherableResource* gatherable = reinterpret_cast<UDojoModelCraftIslandPocketGatherableResource*>(model);
        FString Combined = gatherable->IslandOwner + FString::FromInt(gatherable->IslandId);

        data = &RawSpaces.FindOrAdd(Combined);
        FString GatherableKey = gatherable->ChunkId + FString::FromInt(gatherable->Position);
        data->Gatherables.Add(GatherableKey, gatherable);
    }
    else if (name == "craft_island_pocket-WorldStructure") {
        UDojoModelCraftIslandPocketWorldStructure* structure = reinterpret_cast<UDojoModelCraftIslandPocketWorldStructure*>(model);
        FString Combined = structure->IslandOwner + FString::FromInt(structure->IslandId);

        data = &RawSpaces.FindOrAdd(Combined);
        FString StructureKey = structure->ChunkId + FString::FromInt(structure->Position);
        data->Structures.Add(StructureKey, structure);
    }
}
