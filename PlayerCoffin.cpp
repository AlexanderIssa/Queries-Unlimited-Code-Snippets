// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCoffin.h"

#include "Net/UnrealNetwork.h"
#include "NetworkingPrototype/NetworkingPrototypeGameMode.h"

// Sets default values
APlayerCoffin::APlayerCoffin()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CoffinSM = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoffinSM"));
	CoffinSM->SetupAttachment(RootComponent);

	RespawnLocation = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RespawnLocation"));
	RespawnLocation->SetupAttachment(CoffinSM);

	bReplicates = true;
}

void APlayerCoffin::OnRep_AdjustedTotalTime()
{
	// Optionally, print debug info or update the UI here
	// FString ProgressMessage = FString::Printf(TEXT("Replicated HoldProgress: %.2f"), HoldProgress);
	// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, ProgressMessage);
}

// Called when the game starts or when spawned
void APlayerCoffin::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(FocusCheckTimerHandle, this, &APlayerCoffin::CheckPlayerFocus, 0.1f, true);
	}
}

void APlayerCoffin::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APlayerCoffin, InteractingPlayers);
	DOREPLIFETIME(APlayerCoffin, TimerProgress);
	DOREPLIFETIME(APlayerCoffin, AdjustedTotalHoldTime);
}

void APlayerCoffin::OnInteractionComplete()
{	
	// Reset interaction
	if (HasAuthority())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, "Coffin Finished!");
		
		TimerSpeedMultiplier = 1.0f;
		InteractingPlayers.Empty();
		TimerProgress = 0.0f;
		AdjustedTotalHoldTime = InteractionTime;
	

		// Get the game mode to revive players
		ANetworkingPrototypeGameMode* QUGameMode = GetWorld()->GetAuthGameMode<ANetworkingPrototypeGameMode>();
		if (QUGameMode)
		{
			// Use the coffin's location to revive players
			QUGameMode->RespawnDeadPlayers(RespawnLocation->GetComponentLocation());
		}

		// Clear the timer
		GetWorldTimerManager().ClearTimer(InteractionTimerHandle);
	}
}

void APlayerCoffin::CancelInteraction()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, "Canceled Revival!");

	if (HasAuthority())
	{
		// Reset the interaction state
		TimerSpeedMultiplier = 1.0f;
		InteractingPlayers.Empty();
		TimerProgress = 0.0f;
		AdjustedTotalHoldTime = InteractionTime;
		
		// Clear the interaction timer
		GetWorldTimerManager().ClearTimer(InteractionTimerHandle);
	}
}

void APlayerCoffin::UpdateTimerMultiplier()
{
	int32 PlayerCount = InteractingPlayers.Num();
	TimerSpeedMultiplier = FMath::Max(1.0f, static_cast<float>(PlayerCount)); // Multiplier is at least 1.0

	// Update the replicated total hold time
	AdjustedTotalHoldTime = InteractionTime / TimerSpeedMultiplier;
}

void APlayerCoffin::RestartTimer()
{
	// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, "RestartTimer!");
	
	if (InteractionTimerHandle.IsValid())
	{
		// Get remaining time
		float RemainingTime = GetWorldTimerManager().GetTimerRemaining(InteractionTimerHandle);

		// If the timer is already about to expire, DO NOT restart it!
		if (RemainingTime <= 0.01f)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, "Timer too close to finish - Not restarting.");
			return;
		}

		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, "Restarting timer!");

		// Adjust remaining time by the new speed multiplier
		float AdjustedRemainingTime = RemainingTime / TimerSpeedMultiplier;

		// Clear the existing timer
		GetWorldTimerManager().ClearTimer(InteractionTimerHandle);

		// Restart the timer **without resetting progress**
		GetWorldTimerManager().SetTimer(InteractionTimerHandle, this, &APlayerCoffin::OnInteractionComplete, AdjustedRemainingTime, false);
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, "Starting new timer!");

		// Start a new timer if none exists
		GetWorldTimerManager().SetTimer(InteractionTimerHandle, this, &APlayerCoffin::OnInteractionComplete, InteractionTime / TimerSpeedMultiplier, false);
	}
}

void APlayerCoffin::CheckPlayerFocus()
{
	if (!HasAuthority())
	{
		return;
	}

	if (InteractingPlayers.Num() == 0)
	{
		// Stop checking if no players are left
		GetWorldTimerManager().ClearTimer(FocusCheckTimerHandle);
		return;
	}

	// Temp array to keep track of the players to remove from
	// InteractingPlayers
	TArray<AActor*> PlayersToRemove;
    
	for (const auto Player : InteractingPlayers)
	{
		APlayerController* PC = Cast<APlayerController>(Cast<APawn>(Player)->GetController());
		if (PC)
		{
			FVector PlayerViewLocation;
			FRotator PlayerViewRotation;
			PC->GetPlayerViewPoint(PlayerViewLocation, PlayerViewRotation);

			// Calculate if the coffin is within the player's view
			FVector DirectionToCoffin = (GetActorLocation() - PlayerViewLocation).GetSafeNormal();
			float DotProduct = FVector::DotProduct(PlayerViewRotation.Vector(), DirectionToCoffin);

			// If the player is not looking directly at the coffin, mark them for removal
			if (DotProduct < 0.8f) 
			{
				PlayersToRemove.Add(Player);
			}
		}
	}

	// Remove all players that looked away
	for (AActor* Player : PlayersToRemove)
	{
		InteractingPlayers.Remove(Player);
	}

	// If no players are left interacting, cancel interaction
	if (InteractingPlayers.Num() == 0)
	{
		CancelInteraction();
	}
	else
	{
		// Adjust the timer speed multiplier
		UpdateTimerMultiplier();

		// Restart the timer with the updated speed
		RestartTimer();
	}
}

// Called every frame
void APlayerCoffin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		if (InteractionTimerHandle.IsValid())
		{
			float TotalTime = InteractionTime / TimerSpeedMultiplier;
			float RemainingTime = GetWorldTimerManager().GetTimerRemaining(InteractionTimerHandle);
			TimerProgress = TotalTime - RemainingTime;
		}
	}
}

void APlayerCoffin::StartHold_Implementation(AActor* Interactor)
{
	IInteractableInterface::StartHold_Implementation(Interactor);

	if (HasAuthority())
	{
		if (!Interactor || InteractingPlayers.Contains(Interactor))
		{
			// Ignore if already interacting
			return;
		}

		// Add this player to the array of interacting players
		InteractingPlayers.Add(Interactor);

		// Adjust timer speed multi based on number of players interacting
		UpdateTimerMultiplier();

		// Restart the timer with the new time remaining
		RestartTimer();

		// **Restart focus check timer if it's not already running**
		if (!GetWorldTimerManager().IsTimerActive(FocusCheckTimerHandle))
		{
			GetWorldTimerManager().SetTimer(FocusCheckTimerHandle, this, &APlayerCoffin::CheckPlayerFocus, 0.1f, true);
		}

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, "Starting Revival!");
	}
	else
	{
		Server_StartHold(Interactor);
	}
}

void APlayerCoffin::Server_StartHold_Implementation(AActor* Interactor)
{
	if (HasAuthority())
	{
		if (!Interactor || InteractingPlayers.Contains(Interactor))
		{
			// Ignore if already interacting
			return;
		}
		
		// Add this player to the array of interacting players
		InteractingPlayers.Add(Interactor);

		// Adjust timer speed multi based on number of players interacting
		UpdateTimerMultiplier();

		// Restart the timer with the new time remaining
		RestartTimer();

		// **Restart focus check timer if it's not already running**
		if (!GetWorldTimerManager().IsTimerActive(FocusCheckTimerHandle))
		{
			GetWorldTimerManager().SetTimer(FocusCheckTimerHandle, this, &APlayerCoffin::CheckPlayerFocus, 0.1f, true);
		}

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, "Starting Revival!");
	}
}

void APlayerCoffin::StopHold_Implementation(AActor* Interactor)
{
	IInteractableInterface::StopHold_Implementation(Interactor);

	if (HasAuthority())
	{
		if (InteractingPlayers.Contains(Interactor))
		{
			InteractingPlayers.Remove(Interactor);

			// Adjust the timer speed multiplier
			UpdateTimerMultiplier();

			if (InteractingPlayers.Num() == 0) // No more players interacting
			{
				CancelInteraction();
			}
			else
			{
				// Restart the timer with the updated speed
				RestartTimer();
			}
		}
	}
	else
	{
		Server_StopHold(Interactor); // Call server-side logic
	}
}

void APlayerCoffin::Server_StopHold_Implementation(AActor* Interactor)
{
	if (HasAuthority())
	{
		if (InteractingPlayers.Contains(Interactor))
		{
			InteractingPlayers.Remove(Interactor);

			// Adjust the timer speed multiplier
			UpdateTimerMultiplier();

			if (InteractingPlayers.Num() == 0) // No more players interacting
			{
				CancelInteraction();
			}
			else
			{
				// Restart the timer with the updated speed
				RestartTimer();
			}
		}
	}
}

float APlayerCoffin::GetTotalHoldTime_Implementation()
{	
	return AdjustedTotalHoldTime;
}

float APlayerCoffin::GetCurrentHeldTime_Implementation()
{
	if (InteractionTimerHandle.IsValid())
	{
		float TotalTime = InteractionTime / TimerSpeedMultiplier;
		float RemainingTime = GetWorldTimerManager().GetTimerRemaining(InteractionTimerHandle);
		TimerProgress = TotalTime - RemainingTime;
	}

	// FString ProgressMessage = FString::Printf(TEXT("TimerProgress: %.2f"), TimerProgress);
	// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, ProgressMessage);
    
	return TimerProgress;
}

float APlayerCoffin::GetCurrentNumInteractors_Implementation()
{
	return InteractingPlayers.Num();
}
