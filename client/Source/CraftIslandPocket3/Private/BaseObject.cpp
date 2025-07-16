// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseObject.h"

// Sets default values
ABaseObject::ABaseObject()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

USceneComponent* ABaseObject::FindRootBase()
{
    TArray<USceneComponent*> Components;
    this->GetComponents<USceneComponent>(Components);

    for (USceneComponent* Comp : Components)
    {
        if (Comp && Comp->GetName().Contains(TEXT("RootBase")))
        {
            return Comp;
        }
    }

    return nullptr;
}

void ABaseObject::HarvestableBeginPlay() {
    this->BeginPlay();
}

void ABaseObject::BeginPlay()
{
    Super::BeginPlay();

    USceneComponent* RootBase = this->FindRootBase();
    if (!RootBase) return;
    Grew = false;
    const bool bCanGrow = RootBase->GetNumChildrenComponents() > 1;
    SetActorTickEnabled(bCanGrow);
    if (bCanGrow)
    {
        SetupHarvestableResource(RootBase);
    }
}

void ABaseObject::SetupHarvestableResource(USceneComponent* RootBase)
{
    TArray<USceneComponent*> Children;
    RootBase->GetChildrenComponents(true, Children);

    NextGrowthStep = 0;
    
    // Clear the map first
    MultiStepParents.Empty();
    
    // Look for numbered components (folders like "0", "1", "2", etc.)
    for (USceneComponent* Child : Children)
    {
        if (!Child) continue;
        
        const FString Name = Child->GetName();
        
        // Try to parse the name as a number
        const int32 StepNumber = FCString::Atoi(*Name);
        
        // Check if the name is a valid number (0, 1, 2, etc.)
        if (Name.IsNumeric() || (StepNumber == 0 && Name == TEXT("0")))
        {
            MultiStepParents.Add(StepNumber, Child);
            
            // Initially hide all except step 0
            Child->SetVisibility(StepNumber == 0, true);
        }
    }
    
    NbGrowthStep = MultiStepParents.Num();
}

float ABaseObject::GetCurrentStepPercentage() const
{
    if (NbGrowthStep == 0) return 0.f;

    return static_cast<float>(NextGrowthStep) / static_cast<float>(NbGrowthStep);
}

float ABaseObject::GetRealTimePercentage() const
{
    if (!GatherableResourceInfo) return 0.0f;
    
    const double Now = FDateTime::UtcNow().ToUnixTimestamp();

    const double Start = GatherableResourceInfo->PlantedAt > 0
        ? static_cast<double>(GatherableResourceInfo->PlantedAt)
        : static_cast<double>(GatherableResourceInfo->HarvestedAt);

    const double End = static_cast<double>(GatherableResourceInfo->NextHarvestAt);
    const double Duration = End - Start;
    if (Duration <= 0.0) return 1.0f;

    return static_cast<float>((Now - Start) / Duration);
}

// Called every frame
void ABaseObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (Grew) return;

    float RealTime = GetRealTimePercentage();
    float CurrentStep = GetCurrentStepPercentage();

    // Progress to next step when real time percentage exceeds current step percentage
    if (RealTime < CurrentStep) return;

    // Cache component lookups to avoid multiple TMap searches
    USceneComponent** CurrentStepPtr = MultiStepParents.Find(NextGrowthStep);
    if (!CurrentStepPtr || !*CurrentStepPtr)
    {
        Grew = true;
        return;
    }

    // Hide previous step (only if we're not on the first step)
    if (NextGrowthStep > 0)
    {
        USceneComponent** PrevStepPtr = MultiStepParents.Find(NextGrowthStep - 1);
        if (PrevStepPtr && *PrevStepPtr)
        {
            (*PrevStepPtr)->SetVisibility(false, true);
        }
    }

    // Show current step
    (*CurrentStepPtr)->SetVisibility(true, true);

    NextGrowthStep++;

    if (NextGrowthStep >= NbGrowthStep)
    {
        Grew = true;
    }
}
