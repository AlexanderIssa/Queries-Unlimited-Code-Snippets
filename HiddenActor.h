// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HiddenActor.generated.h"

UCLASS()
class NETWORKINGPROTOTYPE_API AHiddenActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AHiddenActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Getter and setter for bIsBeingLookedAt
	void SetIsBeingLookedAt(const bool val) { bIsBeingLookedAt = val; }
	bool GetIsBeingLookedAt() const { return bIsBeingLookedAt; }

private:

	// Bool to keep track of whether or not this actor is being looked at
	// by the magnifying glass
	bool bIsBeingLookedAt = false;

};
