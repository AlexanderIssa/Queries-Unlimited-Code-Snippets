// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "NetworkingPrototype/Components/FootprintComponent.h"

#include "FootprintSpawnNotify.generated.h"


/**
 * A wrapper that is used in animation montages to tell the Footprint Component
 * to spawn the footprint actor with specified settings
 */
UCLASS()
class NETWORKINGPROTOTYPE_API UFootprintSpawnNotify : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot")
	E_FootEmum FootSelection;

public:
	// Simply gets the MeshComp's owning actor then finds that actor's Footprint Component
	// and tells it to spawn the footprint from there
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};