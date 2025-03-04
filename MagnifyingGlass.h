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

	//
	void RevealHiddenObjects();

	//
	void LowerMagnifyingGlass(ANetworkingPrototypeCharacter* UserCharacter);

	//
	UFUNCTION()
	void OnPickupItem(ANetworkingPrototypeCharacter* Interactor);
	
	// Networked Functions:
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_UseItem(AActor* user);

	UFUNCTION(Server, Reliable)
	void Server_UseItem(AActor* user);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayLowerLensAnim();

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

	// Mesh for MagnifyingGlass
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* GlassSkeletalMesh;

	// Reference to our owning Character
	// Replicated so the server always knows who is using this item
	UPROPERTY(Replicated)
	ANetworkingPrototypeCharacter* mUserCharacter;

	// Map to hold our currently revealed actors
	TMap<FString, AHiddenActor*> mRevealedActorsMap;

	// OnRep function for when mUserCharacter is replicated to clients
	// UFUNCTION()
	// void OnRep_UserCharacter();

	// Simple bool to show/hide debug lines
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	bool bShowDebug = false;
};
