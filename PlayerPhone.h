// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "GameFramework/Actor.h"
#include "NetworkingPrototype/NetworkingPrototypeCharacter.h"

#include "PlayerPhone.generated.h"

// Declare the log category
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerPhone, Log, All);

// Delegate declarations
//DECLARE_DELEGATE_OneParam(EndCallDelegate, APlayerState*);
//DECLARE_DELEGATE_OneParam(CallReceivedDelegate, APlayerState*);
//DECLARE_DELEGATE_OneParam(CallStartedDelegate, APlayerState*);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCallStartedDelegate,APlayerState*, ReceivingPlayerState, int, ChannelID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCallReceivedDelegate,APlayerState*, CallerPlayerState, int, ChannelID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBPEndCallDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEndCallDelegate, APlayerState*, PlayerState);

UCLASS()
class NETWORKINGPROTOTYPE_API APlayerPhone final : public AActor
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* PhoneSkele;

	/** Audio Component for phone sounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Audio, meta = (AllowPrivateAccess = "true"))
	UAudioComponent* PhoneAudioComponent;
	
public:	
	// Sets default values for this actor's properties
	APlayerPhone();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Getter for bIsInCall
	bool GetIsInCall() const { return bIsInCall; }

	// Getter for mCurrentChannel
	UFUNCTION(BlueprintCallable)
	int GetCurrentChannel() const { return mCurrentChannel; }
	// Setter for mCurrentChannel
	void SetCurrentChannel(const int NewChannel) { mCurrentChannel = NewChannel; }

	/* Phone anim functions */
	UFUNCTION()
	void PullUpPhone();
	UFUNCTION()
	void PutDownPhone();
	UFUNCTION()
	void KeepPhoneUp(UAnimMontage* Montage, bool bInterrupted);
	UFUNCTION()
	void OnPhonePutDown(UAnimMontage* Montage, bool bInterrupted);

	// Ringtone sound wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	USoundWave* RingtoneSoundWave;

	// Server function to call another player.
	// Caller checks to see if Channel 1 or 2 are open and joins one of them.
	// After joining, calls Server_NotifyClientsAboutCallByState which calls
	// Multicast_CallReceivingPlayerByState.
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Server_CallPlayerByState(APlayerState* TargetPlayerState, APlayerState* CallerPlayerState);

	// Server RPC to leave the current phone channel
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_LeaveCurrentPhoneChannel(APlayerState* PlayerToChange);

	// Delegates

	// Called when either player ends the current call
	FEndCallDelegate OnEndCall;
	// OnEndCall but for Blueprint listeners
	UPROPERTY(BlueprintAssignable, Category = "Phone Events")
	FBPEndCallDelegate BPOnEndCall;
	// Called when this player receives a call from another player
	UPROPERTY(BlueprintAssignable, Category = "Phone Events")
	FCallReceivedDelegate OnCallReceived;
	// Called when a call is started
	UPROPERTY(BlueprintAssignable, Category = "Phone Events")
	FCallStartedDelegate OnCallStarted;

	// Timer handle for managing phone call timeouts
	FTimerHandle PhoneCallTimeoutHandle;

	// Phone Screen Widget
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	UWidgetComponent* PhoneScreenWidgetPopUp;

	// Phone Screen Rect Light
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	URectLightComponent* PhoneScreenLight;

	// Phone Screen Point Light
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	UPointLightComponent* PhoneScreenPLight;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Client RPC that broadcasts the OnCallStarted Delegate to the
	// client's phone
	UFUNCTION(Client, Reliable)
	void Client_NotifyCallStarted(APlayerState* TargetPlayerState, uint32 ChannelID);

	//
	UFUNCTION(Client, Reliable)
	void Client_NotifyCallReceived(APlayerState* CallerPlayerState, APlayerPhone* CallerPhone, uint32 ChannelID);

	//
	UFUNCTION(BlueprintCallable)
	void EndCall(APlayerState* PlayerEndingCall);

	// Function that gets called after a receiving player doesn't answer/deny the phone call
	UFUNCTION()
	void PendingCallTimeout();

	// Float for how long it takes for an ignored call to be
	// auto denied
	float PhoneTimeoutTime = 5.0f;

	/* Phone anim montages */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* PullPhoneUpMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* KeepPhoneUpMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* PutPhoneDownMontage;

private:

	// NETWORKED FUNCTIONS
	
	// Play phone ringtone from this phone to all clients 
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPhoneAudio();
	
	// Tell the server to run Multicast_PlayPhoneAudio()
	UFUNCTION(Server, Reliable)
	void Server_PlayPhoneAudio();

	// Server RPC to call Multicast_CallReceivingPlayerByState
	UFUNCTION(Server, Reliable)
	void Server_NotifyClientsAboutCallByState(APlayerState* ReceivingPlayerState, APlayerState* CallerPlayerState, int32 ChannelID);

	// Multicast RPC that goes through each client and compares their
	// local player state to the passed in ReceivingPlayerState.
	// If they are equal, then we know this player is the Receiving Player.
	// So we let them know they've been invited to a call.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CallReceivingPlayerByState(APlayerState* ReceivingPlayerState, APlayerState* CallerPlayerState,
		APlayerPhone* CallerPhone, APlayerPhone* ReceiverPhone,	int32 ChannelID);

	// Server RPC that gets called when this player receives a call.
	// Allows the player to accept or deny the call.
	UFUNCTION(Server, Reliable)
	void Server_ReceivePhoneCall(APlayerState* ReceivingPlayerState, APlayerState* CallerPlayerState, int32 ChannelID);

	// Server RPC to modify the bIsInCall replicated bool on a specific phone
	UFUNCTION(Server, Reliable)
	void Server_SetIsInCall(const bool newVal, APlayerPhone* phoneToModify);

	// Client RPC to bind two phone's OnEndCall Delegates locally
	UFUNCTION(Client, Reliable)
	void Client_BindOnEndCall(APlayerPhone* OtherPlayerPhone);
	// Client RPC to unbind two phone's OnEndCall Delegates locally
	UFUNCTION(Client, Reliable)
	void Client_UnbindOnEndCall(APlayerPhone* OtherPlayerPhone);

	UFUNCTION(Client, Reliable)
	void Client_BroadcastOnCallEnded();
	

	// HELPER FUNCTIONS

	// Function to accept another player's invitation to a lobby
	void AcceptCall();

	// Debug Function to allow player to hear themselves while in a call
	void PlayVoiceLocally();

	// Helper to show or hide the PhoneSkele component in game
	void PhoneSkeleSetHidden(bool bHide) const;

	// Helper to get a Player Phone from the passed in state
	// MUST BE CALLED THROUGH THE SERVER! Trying to access variables
	// that are not replicated, such as player controllers
	static APlayerPhone* GetPhoneFromPlayerState(const APlayerState* PlayerState);

	//
	UFUNCTION()
	void OnOtherPlayerEndCall(APlayerState* OtherPlayerState);
	
	
	// MEMBER VARIABLES
	
	// Character who is holding this actor
	ANetworkingPrototypeCharacter* mOwningCharacter = nullptr;

	// bool for debugging, allows player to hear themselves in a call
	bool bHearSelf = true;

	// bool
	bool bIsLeavingCall = false;

	// FUniqueNetIdPtr to keep track of the other person in the call with us
	FUniqueNetIdPtr mOtherPlayerId;
	// FUniqueNetIdPtr to keep track of our owning player's id
	FUniqueNetIdPtr mOwningPlayerId;

	// Replicated property to keep track of the current channel
	// Channel -1 means no phone channel connected currently
	UPROPERTY(Replicated, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int mCurrentChannel = -1;

	// Replicated bool to keep track of if the player is currently in a call
	UPROPERTY(Replicated)
	bool bIsInCall = false;
	
};
