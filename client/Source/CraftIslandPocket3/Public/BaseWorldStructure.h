// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseBlock.h"
#include "../DojoHelpers.h"
#include "BaseWorldStructure.generated.h"

UCLASS()
class CRAFTISLANDPOCKET3_API ABaseWorldStructure : public ABaseBlock
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABaseWorldStructure();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    UFUNCTION(BlueprintImplementableEvent)
    void OnConstructed();

public:
    // Call this when the structure is completed
    void NotifyConstructionCompleted();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Structure")
    bool Constructed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Structure")
    UDojoModelCraftIslandPocketWorldStructure* WorldStructure;
};
