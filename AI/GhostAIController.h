// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "NetworkingPrototype/NetworkingPrototypeCharacter.h"
#include "NetworkingPrototype/DungeonGeneration/DungeonGenerator.h"
#include "Perception/AIPerceptionTypes.h"
#include "GhostAIController.generated.h"

class UBehaviorTreeComponent;
class AGhost;


/**
 * AI Controller for Ghost NPC
 */
UCLASS()
class NETWORKINGPROTOTYPE_API AGhostAIController : public AAIController
{
	GENERATED_BODY()
public:
	AGhostAIController();

	// Perception Component Property //
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAIPerceptionComponent* GhostPerceptionComponent;

	void Distract(ACharacter* Distractor) const;
	void Undistract() const;

	void Stun(const float CustomDuration = 0);
	void StunReset() const;

	// SOUND MANAGER FUNCTIONS
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ProcessSoundEvent(const FSoundEvent& SoundEvent);

	bool IsActorSeen(const AActor* Actor) const;

	/**
	 * Starts the Timer before resetting the Ghost to normal/docile state
	 * @param Duration: Amount of Time (in seconds) the Haunt should last
	 */
	void StartHauntTimer(const float Duration);

	/**
	 * Ends Haunt state and cancels Haunt Timer
	 * Use when Ghost kills a player during haunt
	 */
	void EndHauntTimer();

	// Indicator To game start to know when to start the Aggro Stage //
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ActivateGhost();

	FVector GetRoomLocationFromDG() const;

	void SetTargetPlayer(ANetworkingPrototypeCharacter* Player);

	// Small function that runs when killing a player,
	// used for kill animations and starting the EndHauntTimer
	UFUNCTION()
	void KillingAPlayer();
	UFUNCTION(BlueprintCallable)
	void PostKillAnim(UAnimMontage* Montage, bool bInterrupted);
	
protected:
	virtual void BeginPlay() override;
	void Tick(float deltaTime);
	virtual void OnPossess(APawn* InPawn) override;
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = true))
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = true))
	TObjectPtr<UBlackboardComponent> BlackboardComponent;

	// Caching Onwer Pawn
	AGhost* OwnerGhost = nullptr;

	/** Set up Perception */
	void SetupPerceptionSystem() const;
	
	/** A Sight Sense Config for Ghost AI */
	UPROPERTY(VisibleAnywhere)
	class UAISenseConfig_Sight* GhostSightConfig;

	UFUNCTION()
	void SenseSight(AActor* Actor, const FAIStimulus Stimulus);

	UFUNCTION()
	void OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	UFUNCTION()
	void RegisterSelfToSoundManager();

	FName Distracted = TEXT("Distracted");
	FName Stunned = TEXT("Stunned");
	FName TargetPlayer = TEXT("TargettedPlayer");
	FName KillingPlayer = TEXT("KillingPlayer");
	FName CanSeePlayer = TEXT("CanSeePlayer");
	const FName& StunDuration = TEXT("StunDuration");
	FName isHaunting = TEXT("isHaunting");
	FName CanTeleport = TEXT("CanTeleport");
	FName GhostActive = TEXT("GhostActive");

	// Internal Check if the Ghost can see someone
	bool SeesPlayer = false;

	/** Haunting Timer */
	FTimerHandle HauntingTimerHandle;
	/* Timer to register self with Sound Manager */
	FTimerHandle SoundManagerTimerHandle;

	void PlayCalmingSound() const;

	ADungeonGenerator* DungeonGenerator = nullptr;
	
	/* Public Variables to adjust Ghost's Aggro defaults */

public:
	// How long the ghost remains docile before haunting a player (without seeing any players)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	float AggroTimer = 30.0f;
	FName AggroTimerName = TEXT("AggroMeter");

	// Rate at which the ghost's haunting increases normaly
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	float AggroIncreaseRate = 0.5f;
	FName AggroRate = TEXT("HauntingRate");

	// Rate Multiplier when the ghost can see a player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	float AggroMultiplier = 4.0f;
	FName AggroRateMultiplier = TEXT("HauntingMultiplier");

	// Duration of Haunting without killing a player before calming down again
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	float HauntDuration = 30.0f;
	FName HauntDurationName = TEXT("HauntingDuration");

	float DebugTimerVerify = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	TObjectPtr<USoundBase> CalmingSoundCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	TObjectPtr<USoundBase> StunSoundCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = true))
	TObjectPtr<USoundBase> DistractSoundCue; 

	
};
