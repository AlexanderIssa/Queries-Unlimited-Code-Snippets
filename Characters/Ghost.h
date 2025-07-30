// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GhostAIController.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Ghost.generated.h"

UENUM(BlueprintType)
enum class E_GhostState : uint8
{
	Patrolling UMETA(DisplayName = "Patrolling"),
	Chasing UMETA(DisplayName = "Chasing"),
	Distracted UMETA(DisplayName = "Distracted")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBPGhostStateChanged, E_GhostState, NewGhostState);

UCLASS()
class NETWORKINGPROTOTYPE_API AGhost : public ACharacter
{
	GENERATED_BODY()

public:
	
	// Sets default values for this character's properties
	AGhost();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(Replicated)
	AGhostAIController* GhostAIController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* KillPlayerMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (AllowPrivateAccess = "true"))
	USoundBase* KillPlayerSound;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// For replicated properties
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Item Interations

	/** Boombox intereaction API */
	void Distract(ACharacter* Distractor);
	void EndDistract();

	/** Stunning */
	void Stun(const float Duration = 0);

	// Set Ghost Invisible and Visible
	UFUNCTION(Server, Reliable)
	void TurnInvisible() const;

	UFUNCTION(Server, Reliable)
	void Manifest() const;

	// Overlap Used for door opening
	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// Ghost State Getter
	E_GhostState GetGhostState() const;
	// Ghost State Setter
	void SetGhostState(E_GhostState NewGhostState);

	UFUNCTION(Server, Reliable)
	void Server_SetGhostState(E_GhostState NewGhostState);

	// BP event delegate to trigger when the Ghost's State changes
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Ghost Delegates")
	FBPGhostStateChanged OnGhostStateChanged;

	// Ghost Audio
	UFUNCTION(Server, Reliable)
	void Server_PlayAudio(USoundBase* SoundCue);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAudio(USoundBase* SoundCue);

	/** Ghost Teleportation */
	UFUNCTION(Server, Reliable)
	void Server_Teleport(const FVector& TeleportLocation);
	
	
	// Getters
	UFUNCTION(Server, Reliable)
	void Server_GetGhostAIController(AGhostAIController* output);

	AGhostAIController* GetGhostAIController();

	void SetGhostAIController(AGhostAIController* NewGhostAIController);

	// Multicast function to play the kill anim montage on all clients
	// and tell the server to call PlayKillAnimWithDelegate()
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayKillAnim(AGhostAIController* OwningController);

	// Function to call when Ghost kills a player and this ghost
	// has authority. Binds Blend Out of montage to PostKillAnim()
	// on AIController
	void PlayKillAnimWithDelegate(AGhostAIController* OwningController);

	// Helper function to get the blackboard bool isHaunting
	UFUNCTION(BlueprintCallable)
	bool IsHaunting();

	// Helper function to force a haunt and the option to target a specific player
	UFUNCTION(BlueprintCallable)
	void ForceHaunt(AActor* NewTarget = nullptr);
	
	// Helper function to set Passive Multiplier
	UFUNCTION(BlueprintCallable)
	void SetPassiveMultiplier(float NewMultiplier);

	// Helper function to override Passive Multiplier for set duration
	UFUNCTION(BlueprintCallable)
	void OverridePassiveMultiplierForDuration(float NewMultiplier, float Duration);

	// Helper function to reset Passive Multiplier back to default value
	UFUNCTION(BlueprintCallable)
	void ResetPassiveMultiplier();

private:

	// Adding New Spherical Component to be used to detect Doors //
	USphereComponent* SphereComponent = nullptr;

	// Current Ghost State holder. Replicated using OnRep_CurrentGhostState
	UPROPERTY(ReplicatedUsing=OnRep_CurrentGhostState)
	E_GhostState CurrentGhostState;
	
	UFUNCTION()
	void OnRep_CurrentGhostState();

	// Audio component to replicate sounds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Audio, Replicated, meta = (AllowPrivateAccess = "true"))
	UAudioComponent* GhostAudioComponent;

	// Holder for default value of PassiveMultiplier
	float DefaultPassiveHauntingMultiplier = 1.0f;

	//
	FTimerHandle OverrideHauntingMultiplierTimerHandle;
};
