// Copyright Epic Games, Inc. All Rights Reserved.

#include "CraftIslandPocket3PickUpComponent.h"

UCraftIslandPocket3PickUpComponent::UCraftIslandPocket3PickUpComponent()
{
	// Setup the Sphere Collision
	SphereRadius = 32.f;
}

void UCraftIslandPocket3PickUpComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register our Overlap Event
	OnComponentBeginOverlap.AddDynamic(this, &UCraftIslandPocket3PickUpComponent::OnSphereBeginOverlap);
}

void UCraftIslandPocket3PickUpComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Checking if it is a First Person Character overlapping
	ACraftIslandPocket3Character* Character = Cast<ACraftIslandPocket3Character>(OtherActor);
	if(Character != nullptr)
	{
		// Notify that the actor is being picked up
		OnPickUp.Broadcast(Character);

		// Unregister from the Overlap Event so it is no longer triggered
		OnComponentBeginOverlap.RemoveAll(this);
	}
}
