// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DojoCraftIslandManager.h"
#include "BaseBlock.generated.h"

UCLASS()
class CRAFTISLANDPOCKET3_API ABaseBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABaseBlock();
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* Cube;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
    FIntVector DojoPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
    bool IsOnChain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
    E_Item Item;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
