// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickup.h"

#include "Net/UnrealNetwork.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogPickup);

// Sets default values
APickup::APickup()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Created Default Static Mesh Component
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(FName(TEXT("Static Mesh")));

	// Replicate the Pickups
	bReplicates = true;

}

APickup::APickup(TSubclassOf<AItem> DefaultItem)
{
	// Sets Default Item and Creates Static Mesh Component
	Item = DefaultItem;
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(FName(TEXT("Static Mesh")));

	// Replicate the Pickups
	bReplicates = true;
}

// Called when the game starts or when spawned
void APickup::BeginPlay()
{
	Super::BeginPlay();
	
}

void APickup::OnRep_SpawnedItem()
{
	// Hide our pickup visually
	SetActorHiddenInGame(true);
	// Disable interactions
	SetCanInteract(false);
	// Disable Collisions
	SetActorEnableCollision(false);
	
	// Call Attach logic for clients
	if (mSpawnedItem && mSpawnedItem->GetOwner())
	{
		// The owner of the Item MUST be the character pawn
		ANetworkingPrototypeCharacter* Character = Cast<ANetworkingPrototypeCharacter>(mSpawnedItem->GetOwner());
		if (!Character)
		{
			return;
		}
	
		if (Character->IsLocallyControlled())
		{
			//AttachToPlayer(mSpawnedItem->GetOwner());  // If it's the locally controlled player

			const EAttachmentRule LocationRule = EAttachmentRule::KeepRelative;
			const EAttachmentRule RotationRule = EAttachmentRule::KeepRelative;
			const EAttachmentRule ScaleRule = EAttachmentRule::KeepRelative;

			const FAttachmentTransformRules AttachmentTransformRules(LocationRule, RotationRule, ScaleRule, true);

			// Call the helper function to attach SpawnedItem to the character directly
			Character->AttachToMesh1P(mSpawnedItem, AttachmentTransformRules);
		}
		else 
		{
			//AttachToOtherPlayer(mSpawnedItem->GetOwner());  // If it's another player's character

			const EAttachmentRule LocationRule = EAttachmentRule::KeepRelative;
			const EAttachmentRule RotationRule = EAttachmentRule::KeepRelative;
			const EAttachmentRule ScaleRule = EAttachmentRule::KeepRelative;

			const FAttachmentTransformRules AttachmentTransformRules(LocationRule, RotationRule, ScaleRule, true);

			// Call the helper function to attach SpawnedItem to the character directly
			Character->AttachToMesh3P(mSpawnedItem, AttachmentTransformRules);
		}
	}
}

void APickup::Interact_Implementation(AActor* Interactor)
{
	if (HasAuthority())
	{
		if (ANetworkingPrototypeCharacter* ThisCharacter = Cast<ANetworkingPrototypeCharacter>(Interactor))
		{
			// If this character is holding an object, the Item we spawn is NOT a slate,
			// AND this character is not holding a slate
			if (ThisCharacter->GetHeldItem() && !ItemIsSlate && !ThisCharacter->GetHeldSlate())
			{
				// Force the player to drop the currently held Item
				ThisCharacter->DropItem(ThisCharacter, true);
			}
			else if (!ItemIsSlate && ThisCharacter->GetHeldSlate())
			{
				// If the item we spawn is NOT a slate AND this character is holding a slate,
				// Don't let them pick this item up
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Emerald,TEXT("Player is holding a slate and is trying to pickup an item that is not a slate!"));
				return;
			}
			else if (ItemIsSlate && ThisCharacter->GetHeldSlate())
			{
				// If the item we spawn IS a slate AND this character is holding a slate,
				// Let them swap slates
				// Force the player to drop the currently held Slate
				ThisCharacter->DropItem(ThisCharacter, true);
			}
		}
		
		UWorld* World = GetWorld();
		if (World)
		{

			// Specify the location and rotation for the new actor
			const FVector SpawnLocation(0.0f, 0.0f, 100.0f);   // X, Y, Z coordinates
			const FRotator SpawnRotation(0.0f, 0.0f, 0.0f);   // Pitch, Yaw, Roll

			// Spawn parameters
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Interactor; // Hopefully this doesn't break anything
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			// Spawn the actor
			mSpawnedItem = World->SpawnActor<AItem>(Item, SpawnLocation, SpawnRotation, SpawnParams);

			if (!mSpawnedItem)
			{
				UE_LOG(LogPickup, Error, TEXT("Failed to spawn actor!"));
				return;
			}

			// Set the Item's owner to be the one that interacted with this pickup
			mSpawnedItem->SetOwner(Interactor);

			UE_LOG(LogPickup, Log, TEXT("Actor spawned successfully: %s"), *mSpawnedItem->GetName());

			// Call the OnRep that is responsible for attachment on the server since
			// the OnRep func doesn't get called on the server unless manually called
			OnRep_SpawnedItem();
			
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Emerald, FString(TEXT("PICK UP IMPLEMENTATION!!!!")));
			
			// Ensure the actor is destroyed after a delay
			// We don't immediately destroy because we need the Multicast to finish processing
			SetLifeSpan(0.5f);
		}
	}
	else
	{
		Server_Interact(Interactor);
	}
}

void APickup::Server_Interact_Implementation(AActor* Interactor)
{
	if (HasAuthority())
		Interact_Implementation(Interactor);
}

void APickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickup, mSpawnedItem);
}

