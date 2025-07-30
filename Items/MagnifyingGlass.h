// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Item.h"
#include "HiddenActor.h"

#include "MagnifyingGlass.generated.h"

// Declare the log category
DECLARE_LOG_CATEGORY_EXTERN(LogMagnifyingGlass, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBPStateChanged, bool, BeingHeldUp);

/**
 * 
 */
UCLASS()
class NETWORKINGPROTOTYPE_API AMagnifyingGlass final : public AItem
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMagnifyingGlass();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Implementing the Item Interface
	virtual void UseItem(AActor* user) override;
	virtual E_ClickType GetClickType() override { return E_ClickType::Hold; }

	// Replication functions
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// BP event delegate to trigger when the character raises or lowers the glass
	UPROPERTY(BlueprintAssignable, Category = "Blueprint Events")
	FBPStateChanged OnStateChanged;

private:
	// Helper Functions:

	// The actual functionality of holding up the MG.
	// Reveals Actors of class AHiddenActor, client side. 
	void RevealHiddenObjects();

	// Empties mRevealedActorsMap and hides all AHiddenActors,
	// then calls Server_PlayLowerLensAnim
	void LowerMagnifyingGlass(ANetworkingPrototypeCharacter* UserCharacter);

	// Called when a player interacts with the MG pickup and spawns this item.
	// Sets that player character as the owner of this MG.
	UFUNCTION()
	void OnPickupItem(ANetworkingPrototypeCharacter* Interactor);
	
	// Networked Functions:

	// Server side logic for using the MG.
	UFUNCTION(Server, Reliable)
	void Server_UseItem(AActor* user);

	// Server side logic for picking up the MG.
	UFUNCTION(Server, Reliable)
	void Server_OnPickupItem(ANetworkingPrototypeCharacter* Interactor);

	// Server side logic for lowering the MG.
	UFUNCTION(Server, Reliable)
	void Server_PlayLowerLensAnim();

	
	// Member Properties

	// Root component
	UPROPERTY(EditAnywhere)
	USceneComponent* Root;

	// Mesh for MagnifyingGlass
	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* StaticMesh;

	// Scene comp that is a wrapper for the position to begin the capsule trace
	UPROPERTY(EditAnywhere)
	USceneComponent* MiddleOfLens;

	// Reference to our owning Character
	// Replicated so the server always knows who is using this item
	UPROPERTY(Replicated)
	ANetworkingPrototypeCharacter* mUserCharacter;

	// Map to hold our currently revealed actors
	UPROPERTY()
	TMap<FString, AHiddenActor*> mRevealedActorsMap;

	// Simple bool to show/hide debug lines
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	bool bShowDebug = false;

	// Replicated flag to keep track of whether the MG is being held up or not
	UPROPERTY(ReplicatedUsing = OnRep_IsHeldUp)
	bool bIsHeldUp;
	UFUNCTION()
	void OnRep_IsHeldUp();
};
