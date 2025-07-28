// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseWorldStructure.h"

// Sets default values
ABaseWorldStructure::ABaseWorldStructure()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ABaseWorldStructure::BeginPlay()
{
	Super::BeginPlay();
    Constructed = false;
}

void ABaseWorldStructure::NotifyConstructionCompleted()
{
    if (!Constructed)
    {
        Constructed = true;
        OnConstructed();
    }
}

