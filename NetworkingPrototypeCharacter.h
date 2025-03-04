// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/FootprintComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/LogMacros.h"
#include "Net/VoiceConfig.h"
#include "Widgets/HoldToInteractWidget.h"
#include "NetworkingPrototypeCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
// Forward declare the Player Phone class to avoid circular dependency
class APlayerPhone;
class UWidgetComponent;
class AItem;
class ASlateItem;
class UAnimInstance;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBPItemHeldChanged, AItem*, NewItemHeld);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBPSlateHeldChanged, ASlateItem*, NewSlateHeld);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBPStartSprint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBPStopSprint);

UCLASS(config=Game)
class ANetworkingPrototypeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Player Phone */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Phone, meta = (AllowPrivateAccess = "true"), Replicated)
	APlayerPhone* PlayerPhone;
	
	// INPUT ACTIONS:
	//
	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

	/** Voice Chat Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* VoiceChatAction;

	/** Item Use Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* UseItemAction;

	/** Sprint Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

	/** Drop Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* DropAction;

	/** Item Alt Use Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* AltUse;

	/** Phone Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* PhoneAction;

	// Timer Handles
	FTimerHandle FillDashMeterTimerHandle;
	FTimerHandle FocusTimerHandle;
	
public:
	ANetworkingPrototypeCharacter();

protected:
	virtual void BeginPlay();

	// Function to request interaction from the server
	// use this when a client is trying to interact with an object that should be updated on the server
	// and updated on all clients
	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* InteractableItem);

	// Function to request stopping held interaction from the server
	// use this when a client is trying to release E while they were holding E on
	// an object that should be updated on the server and updated on all clients
	UFUNCTION(Server, Reliable)
	void Server_CancelHoldInteract(AActor* InteractableItem);

	
public:
		
	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Sound Attenuation Settings for Voice Chat **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundAttenuation* VCSoundAttenuation;

	/** Sound Attenuation Settings for Voice Chat **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundEffectSourcePresetChain* VCSoundEffectSourcePresetChain;

	/** The maximum value the sprint bar can hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "Gameplay")
	float MaxSprintValue;

	/** The maximum value the sprint bar can hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "Gameplay")
	float SprintDrainRate;

	// Interact widget
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	UWidgetComponent* Popup;

	// Hold to interact widget
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	UWidgetComponent* HoldPopup;

	UHoldToInteractWidget* HoldWidget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Phone)
	TSubclassOf<APlayerPhone> PlayerPhoneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Footprint)
	UFootprintComponent* FootprintComponent;

	void EnableSlateSpeed();
	void DisableSlateSpeed();

	UFUNCTION(BlueprintCallable)
	float GetRegularWalkSpeed() const { return RegularNormalSpeed; }
	UFUNCTION(BlueprintCallable)
	float GetRegularSprintSpeed() const { return RegularSprintingSpeed; }

	// BP event delegate to trigger when the character picks up a new item
	UPROPERTY(BlueprintAssignable, Category = "Item Events")
	FBPItemHeldChanged OnItemHeldChanged;

	// BP event delegate to trigger when the character picks up a new slate
	UPROPERTY(BlueprintAssignable, Category = "Item Events")
	FBPSlateHeldChanged OnSlateHeldChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sprint Events")
	FBPStartSprint OnStartSprint;

	UPROPERTY(BlueprintAssignable, Category = "Sprint Events")
	FBPStopSprint OnStopSprint;
	// --- Player Death and Respawn ---

	// Blueprint callable function to kill this player
	UFUNCTION(BlueprintCallable, Category= "Death Functions")
	void KillPlayer();

	// Blueprint callable function to respawn this player at a specific location
	UFUNCTION(BlueprintCallable, Category= "Death Functions")
	void RevivePlayer(const FVector& RespawnLocation);
	
	// Handle Player Respawn logic locally
	UFUNCTION(Category= "Death Functions")
	void HandleRespawn(const FVector& RespawnLocation);

	// Handle Player Respawn Server-wide
	UFUNCTION(Server, Reliable, Category = "Death Functions")
	void Server_HandleRespawn(const FVector& RespawnLocation, APlayerState* PlayerStateToRespawn);
	
	// Handle Player Death locally
	UFUNCTION(Category = "Death Functions")
	void HandleDeath();
	
	// Handle Player Death Server-wide
	UFUNCTION(Server, Reliable, Category = "Death Functions")
	void Server_HandleDeath(APlayerState* PlayerStateToKill);
	

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called when Interact Key is pressed **/
	void Interact();

	// Called when E is no longer being held
	void CancelHoldInteract();
	
	void FocusCall();
	
	void UnfocusCall();

	/** Voice Chat Implementation **/ 
	void StartTalking();
	void StopTalking();

	/** Item Usage and Drop **/
	void UseItem();
	void EndUseItem();
	void AltUseItem();

	/** Phone Helper Functions **/ 
	void SpawnPlayerPhone();
	void AttachPlayerPhone();
	void TogglePhone();
	UFUNCTION()
	void PullUpPhone();
	UFUNCTION()
	void PutDownPhone();
	UFUNCTION()
	void KeepPhoneUp(UAnimMontage* Montage, bool bInterrupted);

	/** Networked Functions **/
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartSprint();
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopSprint();
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ResetSprintBarFill();
	UFUNCTION(Server, Reliable)
	void ServerStartSprinting();
	UFUNCTION(Server, Reliable)
	void ServerStopSprinting();
	UFUNCTION(Server, Reliable)
	void ServerResetSprintBarFill();

	UPROPERTY(ReplicatedUsing = OnRep_ItemHeld)
	AItem* ItemHeld = nullptr;

	// OnRep function to handle ItemHeld changes
	UFUNCTION()
	void OnRep_ItemHeld();

	UPROPERTY(ReplicatedUsing = OnRep_SlateHeld)
	ASlateItem* SlateHeld = nullptr;

	// OnRep function to handle SlateHeld changes
	UFUNCTION()
	void OnRep_SlateHeld() const;

	UPROPERTY()
	AActor* FocusedObject = nullptr;

	UPROPERTY()
	AActor* CurrentHoldProgressionActor = nullptr;

	UPROPERTY(Replicated)
	float NormalSpeed;
	UPROPERTY(Replicated)
	float SprintingSpeed;
	float SlateNormalSpeed;
	float SlateSprintingSpeed;
	float RegularNormalSpeed;
	float RegularSprintingSpeed;

	UPROPERTY(Replicated)
	bool SprintBarCanRefill = false;
	UPROPERTY(ReplicatedUsing=OnRep_IsSprinting)
	bool CurrentlySprinting = false;
	float CurrentSprintValue;

	UPROPERTY(Replicated)
	bool UnlimitedSprint;
	
	UFUNCTION()
	void FillDashMeter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsSprinting() const;

	// Phone anim montages
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* PullPhoneUpMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* KeepPhoneUpMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* PutPhoneDownMontage;

	// Flag to keep track of whether the player has their phone held up or not 
	UPROPERTY()
	bool bHasPhoneUp = false;

	// --- Player Alive/Dead ---

	UPROPERTY(ReplicatedUsing = OnRep_IsAlive)
	bool bIsAlive = true;

	UFUNCTION()
	void OnRep_IsAlive();

	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Get the player's phone actor **/
	UFUNCTION(BlueprintCallable)
	APlayerPhone* GetPlayerPhone() const;

	/** Get the player's Currently Held Item **/
	UFUNCTION(BlueprintCallable)
	AItem* GetHeldItem() const { return ItemHeld; }

	/** Set the player's Currently Held Item **/
	UFUNCTION(BlueprintCallable)
	void SetHeldItem(AItem* NewItem, ANetworkingPrototypeCharacter* Character);
	UFUNCTION(Server,Reliable)
	void Server_SetHeldItem(AItem* NewItem, ANetworkingPrototypeCharacter* Character);

	/** Get the player's Currently Held Item **/
	UFUNCTION(BlueprintCallable)
	ASlateItem* GetHeldSlate() const { return SlateHeld; }

	/** Set the player's Currently Held Item **/
	UFUNCTION(BlueprintCallable)
	void SetHeldSlate(ASlateItem* NewSlate, ANetworkingPrototypeCharacter* Character);
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_SetHeldSlate(ASlateItem* NewSlate, ANetworkingPrototypeCharacter* Character);

	UFUNCTION(Server, Reliable)
	void SetUnlimitedSprint(bool IsSprintUnlimited);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetSprintPercentage();

	//Drop Item needs to be public for the Slate Holder
	void DropItem(AActor* PlayerUser, bool Respawn);

	void DropItemWrapper();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_DropItem(AActor* PlayerUser, bool Respawn);

	// Function to set IsAlive
	UFUNCTION(Server, Reliable)
	void Server_SetIsAlive(bool newAlive);

private:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	class UAIPerceptionStimuliSourceComponent* StimulusSource;

	void SetupStimulusSource() const;	
};
