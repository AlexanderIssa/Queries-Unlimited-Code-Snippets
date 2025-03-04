// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HiddenActor.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"

#include "HiddenActor_Footprint.generated.h"

/**
 * 
 */
UCLASS()
class NETWORKINGPROTOTYPE_API AHiddenActor_Footprint : public AHiddenActor
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Collider box for responding to sphere trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UBoxComponent* BoxColliderComp;

	// Decal for footprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDecalComponent* FootprintDecalComp;

	// Decal materials for feet
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterial* LeftFootDecalMat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterial* RightFootDecalMat;

public:

	// Sets default values for this actor's properties
	AHiddenActor_Footprint();

	UFUNCTION()
	void ChangeFootprintColor(int PlayerIdx, E_FootEmum FootSelection) const;

private:
	// Root component
	UPROPERTY(EditAnywhere)
	USceneComponent* Root;
};
