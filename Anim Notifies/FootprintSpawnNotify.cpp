// Fill out your copyright notice in the Description page of Project Settings.


#include "FootprintSpawnNotify.h"
#include "NetworkingPrototype/Components/FootprintComponent.h"


void UFootprintSpawnNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (!MeshComp)
	{
		return;
	}

	// // Keep a ref to the passed in Skeletal Mesh Component
	// MeshCompRef = MeshComp;

	// Get the owner of the skeletal mesh (usually the player or AI character)
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Cast the owner to a pawn
	APawn* OwningPawn = Cast<APawn>(Owner);
	// This notify is designed to only work with player controlled pawns
	if (!OwningPawn || !OwningPawn->IsPlayerControlled())
	{
		return;
	}

	// Get the footprint component from the owning pawn
	UFootprintComponent* FootprintComp = OwningPawn->FindComponentByClass<UFootprintComponent>();
	if (FootprintComp)
	{
		// GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, TEXT("FootprintComp found"));
		// Tell the footprint component to spawn the footprint
		FootprintComp->Server_SpawnFootprint(MeshComp, FootSelection);
	}
}