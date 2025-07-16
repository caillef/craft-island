// Fill out your copyright notice in the Description page of Project Settings.


#include "CraftIslandGameInst.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UCraftIslandGameInst::Init()
{
    Super::Init();
    
    // Start the memory logging timer - logs every 10 seconds
    GetWorld()->GetTimerManager().SetTimer(
        MemoryLogTimerHandle,
        this,
        &UCraftIslandGameInst::LogDojoMemoryUsage,
        10.0f,  // Log every 10 seconds
        true    // Loop
    );
    
    UE_LOG(LogTemp, Warning, TEXT("Started Dojo memory logging timer (every 10 seconds)"));
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
