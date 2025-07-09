// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseBlock.h"
#include "BaseObject.generated.h"

USTRUCT(BlueprintType)
struct FGatherableResource
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ResourceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 NextHarvestAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RawPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Destroyed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntVector DojoPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* Actor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 PlantedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 HarvestedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxHarvest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RemainingHarvest;
};

UCLASS()
class CRAFTISLANDPOCKET3_API ABaseObject : public ABaseBlock
{
	GENERATED_BODY()
    
    float GetCurrentStepPercentage() const;
    float GetRealTimePercentage() const;
    void SetupHarvestableResource(USceneComponent* RootBase);
	
public:	
	// Sets default values for this actor's properties
	ABaseObject();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gatherable")
    FGatherableResource GatherableResourceInfo;

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
};
