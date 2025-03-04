// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractableInterface.h"
#include "GameFramework/Actor.h"
#include "PlayerCoffin.generated.h"

UCLASS()
class NETWORKINGPROTOTYPE_API APlayerCoffin : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

	// Coffin's Static Mesh, assuming no moving parts
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* CoffinSM;

	// Scene comp to keep track of where to spawn the dead players after being revived
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Respawn, meta = (AllowPrivateAccess = "true"))
	USceneComponent* RespawnLocation;
	
public:	
	// Sets default values for this actor's properties
	APlayerCoffin();
	
    // Called when a player starts holding the coffin
    UFUNCTION(Server, Reliable)
    void Server_StartHold(AActor* Interactor);

    // Called when a player stops holding the coffin
    UFUNCTION(Server, Reliable)
    void Server_StopHold(AActor* Interactor);
	
	// Called when AdjustedTotalTime changes on clients
	UFUNCTION()
	void OnRep_AdjustedTotalTime();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Used to replicate properties
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Function called when hold interaction is completed
	UFUNCTION()
	void OnInteractionComplete();

	// Function called when hold interaction is canceled
	UFUNCTION()
	void CancelInteraction();

	// Function to update the timer multiplier by the num of
	// player's interacting with the coffin at a time
	UFUNCTION()
	void UpdateTimerMultiplier();

	// Function to reset the interaction timer or adjust the timer
	// based off of the num of player's interacting with the coffin at a time
	UFUNCTION()
	void RestartTimer();

	// Looping function that checks every interacting player
	// and if they are looking at the coffin currently
	UFUNCTION()
	void CheckPlayerFocus();

	// How long players should interact with the coffin, in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coffin")
	float InteractionTime = 5.0f;

	// Interaction Timer
	FTimerHandle InteractionTimerHandle;
	// Focus Check Timer
	FTimerHandle FocusCheckTimerHandle;

	// Replicated Array of all currently Interacting Players
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Coffin")
	TArray<AActor*> InteractingPlayers;

	// Replicated Timer progress tracker
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Coffin")
	float TimerProgress = 0.0f;

	// Replicated float that keeps track of the adjusted total hold time
	UPROPERTY(ReplicatedUsing = OnRep_AdjustedTotalTime)
	float AdjustedTotalHoldTime = 5.0f;  // Default to normal InteractionTime

	// Timer speed multiplier (based on the number of players)
	float TimerSpeedMultiplier = 1.0f;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// --- Interact Interface overrides ---
	virtual E_InteractType GetInteractType() override { return E_InteractType::Hold; }
	virtual void StartHold_Implementation(AActor* Interactor) override;
	virtual void StopHold_Implementation(AActor* Interactor) override;
	virtual float GetTotalHoldTime_Implementation() override;
	virtual float GetCurrentHeldTime_Implementation() override;
	virtual float GetCurrentNumInteractors_Implementation() override;
	// ---Interact Interface overrides END ---
};