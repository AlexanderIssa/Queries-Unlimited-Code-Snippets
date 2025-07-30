// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkingPrototype/Characters/MagnifyingGlass.h"

#include "Net/UnrealNetwork.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogMagnifyingGlass);

AMagnifyingGlass::AMagnifyingGlass()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Set Id
	ID = 1;
	
	// Make sure this item replicates
	bReplicates = true;

	// Create the root Component
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Create the Static Mesh
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	// Parent the Root with the Mesh
	StaticMesh->SetupAttachment(Root);
	
	// Create a default Scene Comp
	MiddleOfLens = CreateDefaultSubobject<USceneComponent>(TEXT("MiddleOfLens"));
	// Parent the SM with the Scene Comp
	MiddleOfLens->SetupAttachment(StaticMesh);
}

void AMagnifyingGlass::BeginPlay()
{
	Super::BeginPlay();

	// Bind the OnEndUseItem delegate to our Event
	OnEndUseItem.BindUObject(this, &AMagnifyingGlass::LowerMagnifyingGlass);
	// Bind the OnPickup delegate to our Event
	OnPickup.BindUObject(this, &AMagnifyingGlass::OnPickupItem);
}

/*
 * @brief Use the Magnifying Glass client side.
 * Reveal hidden things only to the owner of this item when
 * they are holding down the UseItem button.
 * @param user The actor that is using the Magnifying Glass
 */
void AMagnifyingGlass::UseItem(AActor* user)
{
	Super::UseItem(user);
	
	if (!user)
	{
		UE_LOG(LogMagnifyingGlass, Error, TEXT("Passed in user is null!"));
		return;
	}
	
	// Apply a Post Process effect through the lens of the glass
	

	// Continue some functionality server side such as animations to play
	// to all clients
	Server_UseItem(user);
	
	// Reveal Hidden Objects/Textures/Decals to the owning client
	RevealHiddenObjects();
}

void AMagnifyingGlass::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMagnifyingGlass, mUserCharacter);
	DOREPLIFETIME(AMagnifyingGlass, bIsHeldUp);
}

void AMagnifyingGlass::RevealHiddenObjects()
{
	TArray<FHitResult> HitResults;
	const float CapsuleRadius = 50.0f;  // Radius of the sphere trace
	const float CapsuleHalfHeight = 200.0f;
	
	// // Get the start point from the lens of the magnifying glass
	// FVector Start = StaticMesh->GetSocketLocation(FName("LensSocket"));  // Assuming there's a socket named "LensSocket" on the magnifying glass
	FVector Start = MiddleOfLens->GetComponentLocation();
	
	// Perform a line trace or sphere trace to detect hidden objects in front of the magnifying glass
	const FVector ForwardVec = MiddleOfLens->GetForwardVector();
	const FVector End = Start + ForwardVec * 1000.0f;
	
	// Calculate rotation based on the camera's forward vector (to align the capsule horizontally)
	const FQuat CapsuleRotation = FRotationMatrix::MakeFromZ(ForwardVec).ToQuat();

	// Trace parameters, we can ignore this actor itself during the trace
	FCollisionQueryParams TraceParams(FName(TEXT("CapsuleTrace")), true, this);

	//Ignore the character holding the magnifying glass (the user character)
	if (mUserCharacter)
	{
		// Ignore the character (AddIgnoredActor doesnt work with GetOwner for some reason..)
		TraceParams.AddIgnoredActor(GetOwner());
	}

	// Debug line trace
	if (GetWorld() && bShowDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Purple, false, 1.0f, 0, 1.0f);
		
		// Draw the debug capsule at the Start and End points
		//DrawDebugCapsule(GetWorld(), Start, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Green, false, 5.0f);
		//DrawDebugCapsule(GetWorld(), End, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Red, false, 5.0f);
		
		// Draw the capsule from Start to End with horizontal orientation
		//DrawDebugCapsule(GetWorld(), Start, CapsuleHalfHeight, CapsuleRadius, CapsuleRotation, FColor::Purple, false, 5.0f);
	}

	// Create a temporary set to track the actors you're currently looking at
	TSet<AHiddenActor*> CurrentLookedAtActors;

	// MULTI SWEEP
	if (GetWorld()->SweepMultiByChannel(HitResults,      // Output hit result
			Start,          // Start of the trace
			End,            // End of the trace
			CapsuleRotation, // Rotate 90 on the y axis
			ECC_Camera, // Collision channel
			FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), // The collision shape (sphere)
			TraceParams     // Additional trace parameters);
		))
	{
		for (auto HitResult : HitResults)
		{
			if (bShowDebug)
			{
				DrawDebugSphere(GetWorld(), HitResult.Location, CapsuleRadius, 2, FColor::Green, false, 5.0f);
			}
			
			AActor* HitActor = HitResult.GetActor();
			if (HitActor)
			{
				if (bShowDebug)
				{
					// Print the name of the hit actor
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("Hit actor: %s"), *HitActor->GetName()));
				}
				
				// Check to see if the actor is of type AHiddenActor
				AHiddenActor* HiddenActor = Cast<AHiddenActor>(HitActor);
				if (HiddenActor)
				{
					// Set the actor to visible and mark it as being looked at
					HiddenActor->SetActorHiddenInGame(false);
					HiddenActor->SetIsBeingLookedAt(true);
	                
					// Add the actor to the revealed map if it's not already in it
					if (!mRevealedActorsMap.Contains(HiddenActor->GetName()))
					{
						mRevealedActorsMap.Add(HiddenActor->GetName(), HiddenActor);
						if (bShowDebug)
						{
							GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("Revealing Actor")));
						}
					}
					
					// Add to the current looked-at set
					CurrentLookedAtActors.Add(HiddenActor);
				}
			}
		}

		TArray<FString> KeysToRemove;
		// Now check for actors that are no longer being looked at
		for (const auto& Elem : mRevealedActorsMap)
		{
			if (!CurrentLookedAtActors.Contains(Elem.Value))
			{
				Elem.Value->SetActorHiddenInGame(true);
				Elem.Value->SetIsBeingLookedAt(false);
				KeysToRemove.Add(Elem.Key);

				if (bShowDebug)
				{
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("Hiding Actor"));
				}
			}
		}

		// Now remove safely outside the loop
		for (const FString& Key : KeysToRemove)
		{
			mRevealedActorsMap.Remove(Key);
		}
	}
}


/*
 * @brief When the player lowers the glass, hide all
 * the revealed hidden actors that the player saw since they
 * are no longer peering through the glass
 */
void AMagnifyingGlass::LowerMagnifyingGlass(ANetworkingPrototypeCharacter* UserCharacter)
{
	for (auto Elem : mRevealedActorsMap)
	{
		AHiddenActor* RevealedActor = Elem.Value;
	
		RevealedActor->SetActorHiddenInGame(true);
		RevealedActor->SetIsBeingLookedAt(false);

		if (bShowDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("Hiding all Hidden Actors")));
		}
	}

	// Clear our revealed actors map
	mRevealedActorsMap.Empty();

	// Play the lowering len's animation to all clients including the owning client's
	Server_PlayLowerLensAnim();

}

void AMagnifyingGlass::OnPickupItem(ANetworkingPrototypeCharacter* Interactor)
{
	Server_OnPickupItem(Interactor);
}

void AMagnifyingGlass::Server_OnPickupItem_Implementation(ANetworkingPrototypeCharacter* Interactor)
{
	if (!Interactor)
	{
		UE_LOG(LogMagnifyingGlass, Error, TEXT("OnPickupItem Interactor is not valid!"));
		return;
	}
	mUserCharacter = Interactor;
	SetOwner(Interactor);
}

void AMagnifyingGlass::Server_PlayLowerLensAnim_Implementation()
{
	//Multicast_PlayLowerLensAnim();
	bIsHeldUp = false;
	OnRep_IsHeldUp();
}

/*
 * @brief Use the Magnifying Glass server side.
 * Calls OnRep_IsHeldUp() so that every client
 * can see that the owner of this item is playing
 * a specific animation.
 */
void AMagnifyingGlass::Server_UseItem_Implementation(AActor* user)
{
	// Tell all clients and the server about the glass' new state
	bIsHeldUp = true;
	OnRep_IsHeldUp();
}

void AMagnifyingGlass::OnRep_IsHeldUp()
{
	OnStateChanged.Broadcast(bIsHeldUp);
}