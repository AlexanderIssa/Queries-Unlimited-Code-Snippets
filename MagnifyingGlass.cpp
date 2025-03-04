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

	// Create the Skeletal Mesh
	GlassSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	// Parent the Root with the Skeleton
	GlassSkeletalMesh->SetupAttachment(Root);

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
		// Now check for actors that are no longer being looked at
		for (auto& Elem : mRevealedActorsMap)
		{
			AHiddenActor* RevealedActor = Elem.Value;
            
			// If the revealed actor is not in the currently looked-at set, hide it again
			if (!CurrentLookedAtActors.Contains(RevealedActor))
			{
				RevealedActor->SetActorHiddenInGame(true);
				RevealedActor->SetIsBeingLookedAt(false);

				// Optionally remove it from the map if you don't need to track it anymore
				mRevealedActorsMap.Remove(Elem.Key);

				if (bShowDebug)
				{
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("Hiding Actor")));
				}
			}
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

		// Optionally remove it from the map if you don't need to track it anymore
		mRevealedActorsMap.Remove(Elem.Key);

		if (bShowDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("Hiding all Hidden Actors")));
		}
	}

	// Play the lowering len's animation to all clients including the owning client's
	if (mUserCharacter->HasAuthority())
	{
		Multicast_PlayLowerLensAnim();
	}
	else
	{
		Server_PlayLowerLensAnim();
	}

}

void AMagnifyingGlass::OnPickupItem(ANetworkingPrototypeCharacter* Interactor)
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
	Multicast_PlayLowerLensAnim();
}

void AMagnifyingGlass::Multicast_PlayLowerLensAnim_Implementation()
{

	// Post the new state to any BP listeners
	OnStateChanged.Broadcast(false);
	
	// if (!mUserCharacter)
	// {
	// 	UE_LOG(LogMagnifyingGlass, Error, TEXT("Multicast: mUserCharacter is not valid!"));
	// 	return;
	// }
	
	// // Play Magnifying Glass' use animation here
	// // Only move the glass on the owning client, not locally on other clients
	// if (mUserCharacter->IsLocallyControlled())
	// {		
	// 	// Post the new state to any BP listeners
	// 	OnStateChanged.Broadcast(false);
	// }
	// else
	// {
	// 	// this is where to play the 3rd person animation for all other clients to see that aren't the
	// 	// owning client
	// 	OnStateChanged.Broadcast(false);
	// }

}

/*
 * @brief Use the Magnifying Glass server side.
 * Calls Multicast_UseItem() so that every client
 * can see that the owner of this item is playing
 * a specific animation.
 */
void AMagnifyingGlass::Server_UseItem_Implementation(AActor* user)
{
	// Tell all clients and the server about the glass' new state
	Multicast_UseItem(user);
}

/*
 * @brief Use the Magnifying Glass multicasted server side.
 * Run any functionality that should run for all clients
 * when the owner of this item uses it. Such as playing
 * an animation on the owner's character server wide.
 */
void AMagnifyingGlass::Multicast_UseItem_Implementation(AActor* user)
{

	// Post the new state to any BP listeners
	OnStateChanged.Broadcast(true);
	
	// if (!mUserCharacter)
	// {
	// 	UE_LOG(LogMagnifyingGlass, Error, TEXT("Multicast: mUserCharacter is not valid!"));
	// 	return;
	// }
	
	// // Play Magnifying Glass' use animation here
	// // Only move the glass on the owning client, not locally on other clients
	// if (mUserCharacter->IsLocallyControlled())
	// {		
	// 	// Post the new state to any BP listeners
	// 	OnStateChanged.Broadcast(true);
	// }
	// else
	// {
	// 	// this is where to play the 3rd person animation for all other clients to see that aren't the
	// 	// owning client
	// 	OnStateChanged.Broadcast(true);
	// }
}
