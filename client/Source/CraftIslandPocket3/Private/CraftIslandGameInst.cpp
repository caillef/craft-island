// Fill out your copyright notice in the Description page of Project Settings.


#include "CraftIslandGameInst.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UCraftIslandGameInst::Init()
{
    Super::Init();
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
