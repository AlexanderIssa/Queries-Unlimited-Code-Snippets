// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractableInterface.h"
#include "GameFramework/Actor.h"
#include "Item.h"
#include "SlateItem.h"
#include "Pickup.generated.h"

// Declare the log category
DECLARE_LOG_CATEGORY_EXTERN(LogPickup, Log, All);

UCLASS()
class NETWORKINGPROTOTYPE_API APickup : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APickup();
	APickup(TSubclassOf<AItem> DefaultItem);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	bool ItemIsSlate = false;

public:	

	//======== Properties =============
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Properties")
	TSubclassOf<AItem> Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Properties")
	UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(ReplicatedUsing=OnRep_SpawnedItem)
	AItem* mSpawnedItem;
	UFUNCTION()
	void OnRep_SpawnedItem();

	//========== Functions =============

	UFUNCTION(BlueprintImplementableEvent)
	void BPPickUpItem(AActor* Interactor);

	//========= Interactable Interface Methods =============

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual E_InteractType GetInteractType() override { return E_InteractType::Tap; }

	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Interactor);

	// Replication functions
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
};