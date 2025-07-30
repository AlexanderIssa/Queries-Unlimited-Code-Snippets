// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkingPrototype/Components/FootprintComponent.h"

#include "HiddenActor_Footprint.h"
#include "Kismet/GameplayStatics.h"
#include "NetworkingPrototype/GameStates/QueriesUnlimitedGameState.h"

// Sets default values for this component's properties
UFootprintComponent::UFootprintComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UFootprintComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UFootprintComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UFootprintComponent::Server_SpawnFootprint_Implementation(USkeletalMeshComponent* MeshComp, E_FootEmum FootSelection)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	
	// Cast the owner to a pawn
	APawn* OwningPawn = Cast<APawn>(Owner);
	if (!OwningPawn)
	{
		return;
	}
	
	APlayerState* PlayerState = OwningPawn->GetPlayerState();
	if (!PlayerState)
	{
		return;
	}

	// Retrieve the game state to find our player index
	const AQueriesUnlimitedGameState* QUGameState = Cast<AQueriesUnlimitedGameState>(UGameplayStatics::GetGameState(OwningPawn));
	if (!QUGameState)
	{
		return;
	}

	// Get our player index
	int PlayerIdx = QUGameState->PlayerArray.Find(PlayerState);

	// Tell each client to spawn their own copy of a footprint actor with passed in info
	Multicast_SpawnFootprint(MeshComp, FootSelection, PlayerIdx);
}

bool UFootprintComponent::Server_SpawnFootprint_Validate(USkeletalMeshComponent* MeshComp, E_FootEmum FootSelection)
{
	return true;
}

void UFootprintComponent::Multicast_SpawnFootprint_Implementation(USkeletalMeshComponent* MeshComp, E_FootEmum FootSelection, int PlayerIdx)
{
	// Setup variables for our foot selection
	AHiddenActor_Footprint* RightFootprintActor = nullptr;
	AHiddenActor_Footprint* LeftFootprintActor = nullptr;
	const FName RightFootSocketName = FName("foot_r_Socket");
	const FName LeftFootSocketName = FName("foot_l_Socket");
	const FTransform LeftFootSocketTransform = MeshComp->GetSocketTransform(LeftFootSocketName);
	const FTransform RightFootSocketTransform = MeshComp->GetSocketTransform(RightFootSocketName);
	
	switch (FootSelection)
	{
	case E_FootEmum::LeftFoot:
		// Spawn the footprint actor
			LeftFootprintActor = MeshComp->GetWorld()->SpawnActor<AHiddenActor_Footprint>(FootprintBP,
				LeftFootSocketTransform.GetLocation(), LeftFootSocketTransform.GetRotation().Rotator());

		// Change the footprint decal's color and material depending on the foot placed and player idx
		LeftFootprintActor->ChangeFootprintColor(PlayerIdx, E_FootEmum::LeftFoot);
		break;
		
	case E_FootEmum::RightFoot:
		// Spawn the footprint actor
			RightFootprintActor = MeshComp->GetWorld()->SpawnActor<AHiddenActor_Footprint>(FootprintBP,
				RightFootSocketTransform.GetLocation(), RightFootSocketTransform.GetRotation().Rotator());

		// Change the footprint decal's color and material depending on the foot placed and player idx
		RightFootprintActor->ChangeFootprintColor(PlayerIdx, E_FootEmum::RightFoot);
		break;
	}
}