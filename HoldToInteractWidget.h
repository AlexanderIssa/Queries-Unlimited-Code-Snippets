// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractableInterface.h"
#include "Blueprint/UserWidget.h"
#include "HoldToInteractWidget.generated.h"

/**
 * 
 */
UCLASS()
class NETWORKINGPROTOTYPE_API UHoldToInteractWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Called every frame (Tick equivalent for widgets)
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Helper function to update the InteractableActor reference
	UFUNCTION()
	void UpdateInteractableActor(AActor* NewActor);

	// Helper function to get InteractableActor
	UFUNCTION()
	AActor* GetInteractableActor();

	// Progress bar for the Interaction progress on the passed in Actor
	UPROPERTY(EditAnywhere, meta=(BindWidget))
	class UProgressBar* ProgressBar;

	// Text for the number of current interactors on the passed in Actor
	UPROPERTY(EditAnywhere, meta=(BindWidget))
	class UTextBlock* CurrentNumInteractorsText;
	
private:

	// Reference of the Interactable Actor passed in
	UPROPERTY()
	AActor* InteractableActor = nullptr;

	// If the Actor passed in implements IInteractableInterface
	bool bIsActorInteractable = false;
};
