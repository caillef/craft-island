// Fill out your copyright notice in the Description page of Project Settings.


#include "CraftIslandGameInst.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DojoCraftIslandManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void UCraftIslandGameInst::Init()
{
    Super::Init();
    
    // Connect queue delegates - these will forward to DojoCraftIslandManager when found
    QueueInventoryMoveItem.AddDynamic(this, &UCraftIslandGameInst::OnQueueInventoryMove);
    QueueCraft.AddDynamic(this, &UCraftIslandGameInst::OnQueueCraft);
    QueueSell.AddDynamic(this, &UCraftIslandGameInst::OnQueueSell);
    QueueBuy.AddDynamic(this, &UCraftIslandGameInst::OnQueueBuy);
    QueueVisit.AddDynamic(this, &UCraftIslandGameInst::OnQueueVisit);
    FlushActionQueue.AddDynamic(this, &UCraftIslandGameInst::OnFlushActionQueue);
}

void UCraftIslandGameInst::LogDojoMemoryUsage()
{
    if (ADojoHelpers* DojoHelpers = ADojoHelpers::GetGlobalInstance())
    {
        DojoHelpers->LogResourceUsage();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DojoHelpers instance not found!"));
    }
}

ADojoCraftIslandManager* UCraftIslandGameInst::GetDojoCraftIslandManager() const
{
    // Use cached reference if still valid
    if (CachedDojoCraftIslandManagerRef.IsValid())
    {
        return CachedDojoCraftIslandManagerRef.Get();
    }
    
    // Find the manager in the world and cache it
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ADojoCraftIslandManager> ActorItr(World); ActorItr; ++ActorItr)
        {
            ADojoCraftIslandManager* Manager = *ActorItr;
            if (IsValid(Manager))
            {
                CachedDojoCraftIslandManagerRef = Manager;
                return Manager;
            }
        }
    }
    
    return nullptr;
}

int32 UCraftIslandGameInst::GetPendingActionCount()
{
    if (ADojoCraftIslandManager* Manager = GetDojoCraftIslandManager())
    {
        return Manager->GetPendingActionCount();
    }
    return 0;
}

void UCraftIslandGameInst::OnQueueInventoryMove(int32 FromInventory, int32 FromSlot, int32 ToInventory, int32 ToSlot)
{
    if (ADojoCraftIslandManager* Manager = GetDojoCraftIslandManager())
    {
        Manager->QueueInventoryMove(FromInventory, FromSlot, ToInventory, ToSlot);
    }
}

void UCraftIslandGameInst::OnQueueCraft(int32 Item)
{
    if (ADojoCraftIslandManager* Manager = GetDojoCraftIslandManager())
    {
        Manager->QueueCraft(Item);
    }
}

void UCraftIslandGameInst::OnQueueSell()
{
    if (ADojoCraftIslandManager* Manager = GetDojoCraftIslandManager())
    {
        Manager->QueueSell();
    }
}

void UCraftIslandGameInst::OnQueueBuy(const TArray<int32>& ItemIds, const TArray<int32>& Quantities)
{
    if (ADojoCraftIslandManager* Manager = GetDojoCraftIslandManager())
    {
        Manager->QueueBuy(ItemIds, Quantities);
    }
}

void UCraftIslandGameInst::OnQueueVisit(int32 SpaceId)
{
    if (ADojoCraftIslandManager* Manager = GetDojoCraftIslandManager())
    {
        Manager->QueueVisit(SpaceId);
    }
}

void UCraftIslandGameInst::OnFlushActionQueue()
{
    if (ADojoCraftIslandManager* Manager = GetDojoCraftIslandManager())
    {
        Manager->FlushActionQueue();
    }
}
