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

    UE_LOG(LogTemp, Warning, TEXT("BeginPlay called"));
    USceneComponent* RootBase = this->FindRootBase();
    if (!RootBase) return;
    UE_LOG(LogTemp, Warning, TEXT("RootBase found: %s"), *RootBase->GetName());
    Grew = false;
    UE_LOG(LogTemp, Warning, TEXT("NumChildren: %d"), RootBase->GetNumChildrenComponents());
    const bool bCanGrow = RootBase->GetNumChildrenComponents() > 1;
    SetActorTickEnabled(bCanGrow);
    UE_LOG(LogTemp, Warning, TEXT("bCanGrow: %s"), bCanGrow ? TEXT("true") : TEXT("false"));
    if (bCanGrow)
    {
        UE_LOG(LogTemp, Warning, TEXT("Setting up harvestable resource"));
        SetupHarvestableResource(RootBase);
    }
}

void ABaseObject::SetupHarvestableResource(USceneComponent* RootBase)
{
    TArray<USceneComponent*> Children;
    RootBase->GetChildrenComponents(true, Children);

    NextGrowthStep = 0;
    NbGrowthStep = Children.Num();

    for (int32 i = 0; i < Children.Num(); ++i)
    {
        USceneComponent* Comp = Children[i];
        if (!Comp) continue;

        MultiStepParents.Add(i, Comp);

        const FString Name = Comp->GetName();
        const int32 ParsedName = FCString::Atoi(*Name); // or parse properly

        const bool bIsZero = ParsedName == 0;
        Comp->SetVisibility(bIsZero, true);
    }
}

float ABaseObject::GetCurrentStepPercentage() const
{
    if (NbGrowthStep == 0) return 0.f;

    return static_cast<float>(NextGrowthStep) / static_cast<float>(NbGrowthStep);
}

float ABaseObject::GetRealTimePercentage() const
{
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

    if (CurrentStep < RealTime) return;

    if (!MultiStepParents.Contains(NextGrowthStep)) {
        Grew = true;
        return;
    }

    USceneComponent* TargetComponent = nullptr;

    // Hide prev step
    TargetComponent = MultiStepParents[NextGrowthStep - 1];
    TargetComponent->SetVisibility(false, true);

    TargetComponent = MultiStepParents[NextGrowthStep];

    if (!TargetComponent) return;

    TargetComponent->SetVisibility(true, true);

    NextGrowthStep++;

    if (NextGrowthStep >= NbGrowthStep)
    {
        Grew = true;
    }
}
