// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseBlock.h"
#include "../DojoHelpers.h"
#include "BaseObject.generated.h"

UCLASS()
class CRAFTISLANDPOCKET3_API ABaseObject : public ABaseBlock
{
	GENERATED_BODY()
    
    float GetCurrentStepPercentage() const;
    float GetRealTimePercentage() const;
    void SetupHarvestableResource(USceneComponent* RootBase);
    USceneComponent* FindRootBase();

public:	
	// Sets default values for this actor's properties
	ABaseObject();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gatherable")
    UDojoModelCraftIslandPocketGatherableResource* GatherableResourceInfo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gatherable")
    bool Grew;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gatherable")
    TMap<int32, USceneComponent *> MultiStepParents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gatherable")
    int32 NextGrowthStep;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gatherable")
    int32 NbGrowthStep;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
    
    UFUNCTION(BlueprintCallable)
    void HarvestableBeginPlay();
};
