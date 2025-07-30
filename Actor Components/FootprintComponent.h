// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FootprintComponent.generated.h"

// Forward declare our footprint actor
class AHiddenActor_Footprint;

UENUM(BlueprintType)
enum class E_FootEmum : uint8
{
	LeftFoot UMETA(DisplayName = "Left Foot"),
	RightFoot  UMETA(DisplayName = "Right Foot")
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NETWORKINGPROTOTYPE_API UFootprintComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UFootprintComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Footprint Actor to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn");
	TSubclassOf<AHiddenActor_Footprint> FootprintBP;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Server function to spawn a footprint
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SpawnFootprint(USkeletalMeshComponent* MeshComp, E_FootEmum FootSelection);

	// Server function to spawn a footprint
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SpawnFootprint(USkeletalMeshComponent* MeshComp, E_FootEmum FootSelection, int PlayerIdx);
};
