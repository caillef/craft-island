// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseBlock.h"

// Sets default values
ABaseBlock::ABaseBlock()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComponent);

    RootBase = CreateDefaultSubobject<USceneComponent>(TEXT("RootBase"));
    RootBase->SetupAttachment(RootComponent);

    Cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
    Cube->SetupAttachment(RootComponent);

    // Mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        Cube->SetStaticMesh(MeshAsset.Object);
    }

    // Transform
    Cube->SetRelativeLocation(FVector::ZeroVector);
    Cube->SetRelativeRotation(FRotator::ZeroRotator);
    Cube->SetRelativeScale3D(FVector(0.5f));

    // Collision
    Cube->SetCollisionProfileName(TEXT("Block"));

    // Mobility
    Cube->SetVisibility(false, true);
    Cube->SetHiddenInGame(true);
}

// Called when the game starts or when spawned
void ABaseBlock::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABaseBlock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

