// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkingPrototype/Public/PlayerPhone.h"

#include "../../../Plugins/AdvancedSteamSessions/Source/AdvancedSteamSessions/Classes/AdvancedSteamFriendsLibrary.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NetworkingPrototype/NetworkingPrototypeCharacter.h"
#include "OnsetVoip/Public/OnsetVoipWorldSubsystem.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogPlayerPhone);

// Hear ourselves
//#define VOICE_LOOPBACK 1

// Sets default values
APlayerPhone::APlayerPhone()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	PhoneSkele = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PhoneSkeletalMesh"));
	PhoneSkele->SetupAttachment((RootComponent));
	// Allow all players to see phone
	PhoneSkele->SetOnlyOwnerSee(false);
	// Turn off shadows for performance
	PhoneSkele->bCastDynamicShadow = false;
	PhoneSkele->CastShadow = false;
	//PhoneSkele->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	// Default create an audio component
	PhoneAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("PhoneAudioComp"));
	// Set Audio Parent to the phone skeleton
	PhoneAudioComponent->SetupAttachment(PhoneSkele);
	PhoneAudioComponent->bAutoActivate = false;

	// Phone screen widget
	PhoneScreenWidgetPopUp = CreateDefaultSubobject<UWidgetComponent>(TEXT("PhoneScreenWidget"));
	PhoneScreenWidgetPopUp->SetWidgetSpace(EWidgetSpace::World);
	PhoneScreenWidgetPopUp->SetDrawSize(FVector2D(300.f, 300.f));
	PhoneScreenWidgetPopUp->SetOnlyOwnerSee(true);
	const FName PhoneScreenSocketName = FName("ScreenSocket");
	PhoneScreenWidgetPopUp->SetupAttachment(PhoneSkele, PhoneScreenSocketName);

	// Phone screen light
	PhoneScreenLight = CreateDefaultSubobject<URectLightComponent>(TEXT("PhoneScreenLight"));
	PhoneScreenLight->SetupAttachment(PhoneSkele, PhoneScreenSocketName);
	// Phone screen light
	PhoneScreenPLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PhoneScreenPLight"));
	PhoneScreenPLight->SetupAttachment(PhoneSkele, PhoneScreenSocketName);
	
	// Setup networked replication
	bReplicates = true;
}

// Called when the game starts or when spawned
void APlayerPhone::BeginPlay()
{
	Super::BeginPlay();

	// Set our owning character ref to our owning actor casted to our class
	if (!Cast<ANetworkingPrototypeCharacter>(Owner))
	{
		UE_LOG(LogPlayerPhone, Log, TEXT("Owner actor is not the right class!"));
	}
	mOwningCharacter = Cast<ANetworkingPrototypeCharacter>(Owner);

	PhoneScreenWidgetPopUp->SetVisibility(false);
	PhoneScreenLight->SetVisibility(false);
	PhoneScreenPLight->SetVisibility(false);
}

// Called every frame
void APlayerPhone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

void APlayerPhone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APlayerPhone, mCurrentChannel);
	DOREPLIFETIME(APlayerPhone, bIsInCall);
}

void APlayerPhone::Client_NotifyCallReceived_Implementation(APlayerState* CallerPlayerState, APlayerPhone* CallerPhone, uint32 ChannelID)
{
	if (!CallerPhone)
	{
		return;
	}
	
	// Subscribe this phone to the Caller's phone to listen
	// for when the caller ends the call
	//CallerPhone->OnEndCall.BindUObject(this, &APlayerPhone::OnOtherPlayerEndCall);
	
	// Broadcast the dynamic delegate to notify the client
	OnCallReceived.Broadcast(CallerPlayerState, ChannelID);

	// Optionally show the call UI on the client
	// ShowCallUI(TargetPlayerState);

	// Clear the existing timer
	GetWorldTimerManager().ClearTimer(PhoneCallTimeoutHandle);

	// Restart the timer **without resetting progress**
	GetWorldTimerManager().SetTimer(PhoneCallTimeoutHandle, this, &APlayerPhone::PendingCallTimeout, PhoneTimeoutTime, false);

	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Call received on the client!"));
}

void APlayerPhone::Client_NotifyCallStarted_Implementation(APlayerState* TargetPlayerState, uint32 ChannelID)
{	
	// Broadcast the dynamic delegate to notify the client
	OnCallStarted.Broadcast(TargetPlayerState, ChannelID);

	// Optionally show the call UI on the client
	// ShowCallUI(TargetPlayerState);

	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Call started on the client!"));
}

void APlayerPhone::Server_PlayPhoneAudio_Implementation()
{
	Multicast_PlayPhoneAudio();
}

void APlayerPhone::Multicast_PlayPhoneAudio_Implementation()
{
	if (PhoneAudioComponent)
	{
		PhoneAudioComponent->SetSound(RingtoneSoundWave);
		PhoneAudioComponent->Play();
	}
}

void APlayerPhone::Server_CallPlayerByState_Implementation(APlayerState* TargetPlayerState, APlayerState* CallerPlayerState)
{

	if (GetPhoneFromPlayerState(TargetPlayerState)->bIsInCall || GetPhoneFromPlayerState(CallerPlayerState)->bIsInCall)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("One or both of the call players are currently in a call."));
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
TEXT("One or both of the call players are currently in a call."));
		return;
	}
	
	// Get our current world ref
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("World is invalid"));
		return;
	}

	// Get the OnsetVoipWorldSubsystem
	const UOnsetVoipWorldSubsystem* OnsetVoipWorldSubsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>();
	if (!OnsetVoipWorldSubsystem)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("OnsetVoipWorldSubsystem is NULL!"));
		return;	
	}

	// Get the caller's VOIPTalker
	UOnsetVoipTalker* CallerPlayerTalker = OnsetVoipWorldSubsystem->GetVoipTalker(CallerPlayerState);
	if (!CallerPlayerTalker)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("CallerPlayerTalker is NULL!"));
		return;	
	}

	// First check if Channel 1 is free
	const TArray<UOnsetVoipTalker*> TalkersInChannel1 = OnsetVoipWorldSubsystem->GetAllTalkersInVoiceChannel(1);
	if (TalkersInChannel1.Num() == 0)
	{
		// It's free, so join it and ask the receiver to join it too
		CallerPlayerTalker->SetVoiceChannel(1,true);
		UE_LOG(LogPlayerPhone, Log, TEXT("A player caller joined Phone Channel 1."));
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
	TEXT("A player caller joined Phone Channel 1!"));

		// Set the replicated flag bIsInCall to true for the caller's phone
		Server_SetIsInCall(true, GetPhoneFromPlayerState(CallerPlayerState));
		// TEMP UNTIL ACCEPTING CALLS ARE IN!
		// Set the target player's phone flag bIsInCall to true
		Server_SetIsInCall(true, GetPhoneFromPlayerState(TargetPlayerState));
		
		// Notify the receiving player and the calling player
		Server_NotifyClientsAboutCallByState(TargetPlayerState, CallerPlayerState, 1);
		return;
	}

	// It's not free, check Channel 2
	const TArray<UOnsetVoipTalker*> TalkersInChannel2 = OnsetVoipWorldSubsystem->GetAllTalkersInVoiceChannel(2);
	if (TalkersInChannel2.Num() == 0)
	{
		// It's free, so join it and ask the receiver to join it too
		CallerPlayerTalker->SetVoiceChannel(2, true);
		UE_LOG(LogPlayerPhone, Log, TEXT("Caller joined Phone Channel 2."));
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
TEXT("Joined Phone Channel 2!"));

		// Set the replicated flag bIsInCall to true for the caller's phone
		Server_SetIsInCall(true, GetPhoneFromPlayerState(CallerPlayerState));
		// TEMP UNTIL ACCEPTING CALLS ARE IN!
		// Set the target player's phone flag bIsInCall to true
		Server_SetIsInCall(true, GetPhoneFromPlayerState(TargetPlayerState));
		
		// Notify the receiving player and the calling player
		Server_NotifyClientsAboutCallByState(TargetPlayerState, CallerPlayerState, 2);
		return;
	}

	// Both Phone Channels are taken, wait for one to be free
	UE_LOG(LogPlayerPhone, Warning, TEXT("Both Phone Channels are taken! Wait for one to be free."));
	
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
		TEXT("Both Phone Channels are taken! Wait for one to be free."));
}

void APlayerPhone::Server_LeaveCurrentPhoneChannel_Implementation(APlayerState* PlayerToChange)
{
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
TEXT("Server_LeaveCurrentPhoneChannel_Implementation."));

	if (bIsLeavingCall)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("Already leaving call!"));
		return;
	}

	// set our flag to true to avoid recursion
	bIsLeavingCall = true;
	
	// Get the current world ref
	const UWorld* const World = GEngine->GetWorldFromContextObject(GetWorld(), EGetWorldErrorMode::LogAndReturnNull);
	if (!IsValid(World))
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("Invalid World from RecievePhoneCall!"));
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
TEXT("Invalid World from RecievePhoneCall!"));
		bIsLeavingCall = false;  // Reset flag before exiting
		return;
	}

	// Get the OnsetVoipWorldSubsystem
	const UOnsetVoipWorldSubsystem* OnsetVoipWorldSubsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>();
	if (!OnsetVoipWorldSubsystem)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("OnsetVoipWorldSubsystem is NULL!"));
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
TEXT("OnsetVoipWorldSubsystem is NULL!"));
		bIsLeavingCall = false;  // Reset flag before exiting
		return;	
	}

	// Get the player's VOIPTalker
	UOnsetVoipTalker* PlayerTalker = OnsetVoipWorldSubsystem->GetVoipTalker(PlayerToChange);
	if (!PlayerTalker)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("PlayerTalker is NULL!"));
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("PlayerTalker is NULL!"));
		bIsLeavingCall = false;  // Reset flag before exiting
		return;	
	}

	// Get Player Phone
	APlayerPhone* PlayerPhone = GetPhoneFromPlayerState(PlayerToChange);
	if (!PlayerPhone)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("PlayerPhone is NULL!"));
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("PlayerPhone is NULL!"));
		bIsLeavingCall = false;  // Reset flag before exiting
		return;	
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
		FString::Printf(TEXT("Current Channel: %d"), PlayerPhone->GetCurrentChannel()));
	
	// Check the Player's current channel
	if (PlayerPhone->GetCurrentChannel() != -1)
	{
		// Leave the current channel
		PlayerTalker->SetVoiceChannel(PlayerPhone->GetCurrentChannel(), false);
		// Notify any subscribers that this player left the call
		if (PlayerPhone->OnEndCall.IsBound())
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("Executing OnEndCall delegate"));
			//PlayerPhone->OnEndCall.ExecuteIfBound(PlayerToChange);
			PlayerPhone->OnEndCall.Broadcast(PlayerToChange);
			
			// Unbind the delegate to prevent further executions
			//PlayerPhone->OnEndCall.Unbind();
			//PlayerPhone->OnEndCall.RemoveDynamic(OtherPlayerPhone, &APlayerPhone::OnOtherPlayerEndCall);
			PlayerPhone->OnEndCall.Clear();
		}

		// Extra precaution, remove this talker from all phone channels
		if (PlayerTalker->IsInVoiceChannel(1))
		{
			PlayerTalker->SetVoiceChannel(1, false);
		}
		if (PlayerTalker->IsInVoiceChannel(2))
		{
			PlayerTalker->SetVoiceChannel(2, false);
		}

		// Reset the current channel tracker
		PlayerPhone->SetCurrentChannel(-1);

		// Set the replicated flag for bIsInCall to false for this phone
		Server_SetIsInCall(false, PlayerPhone);
		
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
	TEXT("Left current call."));
	}
	else
	{
		UE_LOG(LogPlayerPhone, Log, TEXT("Trying to leave a channel when we arent in one currently!"));
	}
	
	// Broadcast the Blueprintable OnCallEnded Event
	//PlayerPhone->OnCallEnded.Broadcast();
	Client_BroadcastOnCallEnded();
	
	
	// Reset flag before exiting
	bIsLeavingCall = false;
}

void APlayerPhone::Server_SetIsInCall_Implementation(const bool newVal, APlayerPhone* phoneToModify)
{
	phoneToModify->bIsInCall = newVal;
}

void APlayerPhone::Client_BindOnEndCall_Implementation(APlayerPhone* OtherPlayerPhone)
{
	if (OtherPlayerPhone)
	{
		OnEndCall.AddUniqueDynamic(OtherPlayerPhone, &APlayerPhone::OnOtherPlayerEndCall);
	}
}

void APlayerPhone::Client_UnbindOnEndCall_Implementation(APlayerPhone* OtherPlayerPhone)
{
	if (OtherPlayerPhone)
	{
		OnEndCall.RemoveDynamic(OtherPlayerPhone, &APlayerPhone::OnOtherPlayerEndCall);
	}
}

void APlayerPhone::Client_BroadcastOnCallEnded_Implementation()
{
	if(BPOnEndCall.IsBound())
	{
		BPOnEndCall.Broadcast();
	}
}

void APlayerPhone::AcceptCall()
{
	UE_LOG(LogPlayerPhone, Log, TEXT("Accepting received call"));

	if (PhoneCallTimeoutHandle.IsValid())
	{
		// Clear the existing timer
		GetWorldTimerManager().ClearTimer(PhoneCallTimeoutHandle);
	}
	
	// Player now in a call
	Server_SetIsInCall_Implementation(true, this);
}

void APlayerPhone::PhoneSkeleSetHidden(bool bHide) const
{
	if (PhoneSkele && PhoneScreenWidgetPopUp && PhoneScreenLight)
	{
		PhoneSkele->SetHiddenInGame(bHide);
		PhoneScreenWidgetPopUp->SetVisibility(!bHide);
		PhoneScreenLight->SetVisibility(!bHide);
		PhoneScreenPLight->SetVisibility(!bHide);
	}
}

APlayerPhone* APlayerPhone::GetPhoneFromPlayerState(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("PlayerState is NULL!"));
		return nullptr;
	}

	const APlayerController* PlayerController = PlayerState->GetPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("PlayerController is NULL!"));
		return nullptr;
	}

	const ANetworkingPrototypeCharacter* PlayerCharacter = Cast<ANetworkingPrototypeCharacter>(PlayerController->GetPawn());
	if (!PlayerCharacter)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("PlayerCharacter is NULL!"));
		return nullptr;
	}

	APlayerPhone* PlayerPhone = PlayerCharacter->GetPlayerPhone();
	if (!PlayerPhone)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("PlayerPhone is NULL!"));
		return nullptr;
	}

	return PlayerPhone;
}

void APlayerPhone::OnOtherPlayerEndCall(APlayerState* OtherPlayerState)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
	TEXT("Other player ended call, disconnecting you from channel."));
	
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("World is NULL!"));
		return;
	}
	
	// Get our local player's state
	APlayerState* LocalPlayerState = UGameplayStatics::GetPlayerState(World, 0);
	if (!LocalPlayerState || !LocalPlayerState->GetPlayerController()->IsLocalController())
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("LocalPlayerState is NULL or is not the local controller!"));
		return;
	}

	// Unbind OnEndCall to avoid recursion
	if (OnEndCall.IsBound())
	{
		APlayerPhone* OtherPlayerPhone = GetPhoneFromPlayerState(OtherPlayerState);
		Client_UnbindOnEndCall(OtherPlayerPhone);
	}
	// if (OnEndCall.IsBound())
	// {
	// 	OnEndCall.Unbind();
	// }

	// We are no longer in a call
	Server_SetIsInCall_Implementation(false, this);
	
	// Get our local player to leave the channel
	Server_LeaveCurrentPhoneChannel(LocalPlayerState);
}

void APlayerPhone::EndCall(APlayerState* PlayerEndingCall)
{
	UE_LOG(LogPlayerPhone, Log, TEXT("Ending current call"));
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
TEXT("Ending current call"));
	
	// No longer in a call
	Server_SetIsInCall(false, this);

	// Leave the current phone channel
	Server_LeaveCurrentPhoneChannel(PlayerEndingCall);
}

void APlayerPhone::PendingCallTimeout()
{
	APlayerState* OwningState = mOwningCharacter->GetPlayerState();
	if (!OwningState)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("mOwningCharacter->GetPlayerState() returned NULL!"));
		return;
	}
	
	EndCall(OwningState);
}

void APlayerPhone::PullUpPhone()
{
	UAnimInstance* AnimInstance = PhoneSkele->GetAnimInstance();
	if (AnimInstance && PullPhoneUpMontage)
	{
		// If we are already putting down the phone, don't let the player spam
		if (AnimInstance->Montage_IsPlaying(PutPhoneDownMontage) || AnimInstance->Montage_IsPlaying(PullPhoneUpMontage))
		{
			return;
		}
		
		// Show the phone skeleton in game
		PhoneSkeleSetHidden(false);
		
		// Play the phone pull up montage
		AnimInstance->Montage_Play(PullPhoneUpMontage);

		// Bind delegates properly before setting them
		FOnMontageBlendingOutStarted BlendingOutDelegate;
		BlendingOutDelegate.BindUObject(this, &APlayerPhone::KeepPhoneUp);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, PullPhoneUpMontage);

		FOnMontageEnded MontageEndedDelegate;
		MontageEndedDelegate.BindUObject(this, &APlayerPhone::KeepPhoneUp);
		AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, PullPhoneUpMontage);
	}
}

void APlayerPhone::KeepPhoneUp(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* AnimInstance = PhoneSkele->GetAnimInstance();
	if (AnimInstance && KeepPhoneUpMontage)
	{
		// If we are already putting down the phone, don't let the player spam
		if (AnimInstance->Montage_IsPlaying(PutPhoneDownMontage) || AnimInstance->Montage_IsPlaying(PullPhoneUpMontage))
		{
			return;
		}
		
		// Ensure phone stays up
		AnimInstance->Montage_Play(KeepPhoneUpMontage);
	}
}

void APlayerPhone::OnPhonePutDown(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* AnimInstance = PhoneSkele->GetAnimInstance();
	// If we are already putting down the phone, don't let the player spam
	if (AnimInstance->Montage_IsPlaying(PutPhoneDownMontage) || AnimInstance->Montage_IsPlaying(PullPhoneUpMontage))
	{
		return;
	}
	
	// Hide the phone skeleton in game
	PhoneSkeleSetHidden(true);
}

void APlayerPhone::PutDownPhone()
{
	UAnimInstance* AnimInstance = PhoneSkele->GetAnimInstance();
	if (AnimInstance && PutPhoneDownMontage)
	{
		// If we are already putting down the phone, don't let the player spam
		if (AnimInstance->Montage_IsPlaying(PutPhoneDownMontage) || AnimInstance->Montage_IsPlaying(PullPhoneUpMontage))
		{
			return;
		}

		// Hide the screen widget and lights
		PhoneScreenWidgetPopUp->SetVisibility(false);
		PhoneScreenLight->SetVisibility(false);
		PhoneScreenPLight->SetVisibility(false);
		
		// Play the phone put-down montage
		AnimInstance->Montage_Play(PutPhoneDownMontage);

		// Bind delegates properly before setting them
		FOnMontageBlendingOutStarted BlendingOutDelegate;
		BlendingOutDelegate.BindUObject(this, &APlayerPhone::OnPhonePutDown);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, PutPhoneDownMontage);

		FOnMontageEnded MontageEndedDelegate;
		MontageEndedDelegate.BindUObject(this, &APlayerPhone::OnPhonePutDown);
		AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, PutPhoneDownMontage);
	}
}

void APlayerPhone::Server_NotifyClientsAboutCallByState_Implementation(APlayerState* ReceivingPlayerState,
                                                                       APlayerState* CallerPlayerState, int32 ChannelID)
{
	// Get both player's phone by their states
	APlayerPhone* CallerPhone = GetPhoneFromPlayerState(CallerPlayerState);
	if (!CallerPhone)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("CallerPhone is invalid!"));
		return;
	}

	APlayerPhone* ReceiverPhone = GetPhoneFromPlayerState(ReceivingPlayerState);
	if (!ReceiverPhone)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("ReceiverPhone is invalid!"));
		return;
	}

	// Set mCurrentChannel on the receiver client's phone through the server
	ReceiverPhone->SetCurrentChannel(ChannelID);
	// Set mCurrentChannel on the caller client's phone through the server
	CallerPhone->SetCurrentChannel(ChannelID);

	// Subscribe the receiver's phone to the Caller's phone to listen
	// for when the caller ends the call
	CallerPhone->OnEndCall.AddUniqueDynamic(ReceiverPhone, &APlayerPhone::OnOtherPlayerEndCall);
	
	// Subscribe the Caller's phone to the Receiver's phone to listen
	// for when the receiver ends the call
	ReceiverPhone->OnEndCall.AddUniqueDynamic(CallerPhone, &APlayerPhone::OnOtherPlayerEndCall);
	
	Multicast_CallReceivingPlayerByState(ReceivingPlayerState, CallerPlayerState, CallerPhone, ReceiverPhone, ChannelID);
}

void APlayerPhone::Multicast_CallReceivingPlayerByState_Implementation(APlayerState* ReceivingPlayerState,
	APlayerState* CallerPlayerState,APlayerPhone* CallerPhone, APlayerPhone* ReceiverPhone, int32 ChannelID)
{
	GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, FString::Printf(
		TEXT("Someone is asking another player to join their call.")));
	
	if (!ReceivingPlayerState || !CallerPlayerState || (ChannelID == 0))
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("Passed in info in Multicast_CallReceivingPlayerByState_Implementation is invalid!"));
		return;	
	}
	
	const UWorld* World = GetWorld();
	if(!World)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("World is NULL!"));
		return;	
	}

	// Get the local player controller so we can get the local player state
	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(World, 0);
	if (!LocalPlayerController)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("LocalPlayerController is NULL!"));
		return;	
	}

	// Get the local player state through the controller since this is a listen server,
	// getting world player idx 0's player state ALWAYS returns the server player's PlayerState
	APlayerState* LocalPlayerState = LocalPlayerController->GetPlayerState<APlayerState>();
	if (!LocalPlayerState)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("LocalPlayerState is NULL!"));
		return;	
	}
	
	// Check to see if the local player state is the one being called
	if (LocalPlayerState == ReceivingPlayerState)
	{
		// It is
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("YOU ARE THE RECEIVER!")));
		
		// Notify the receiving player that they are being called and send in the calling player's info 
		ReceiverPhone->Client_NotifyCallReceived(CallerPlayerState, CallerPhone, ChannelID);

		// Call the local player and send in the caller ID
		ReceiverPhone->Server_ReceivePhoneCall(LocalPlayerState, CallerPlayerState, ChannelID);
	}
	// Check to see if the local player is the caller
	else if(LocalPlayerState == CallerPlayerState)
	{
		// It is
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("YOU ARE THE CALLER!")));
		
		// Notify the calling player that they started a call and send in the other player's info 
		CallerPhone->Client_NotifyCallStarted(ReceivingPlayerState, ChannelID);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("It's not you.")));
	}
}

void APlayerPhone::Server_ReceivePhoneCall_Implementation(APlayerState* ReceivingPlayerState, APlayerState* CallerPlayerState, int32 ChannelID)
{
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Receiving a phone call."));
	UE_LOG(LogPlayerPhone, Log, TEXT("Phone call received from player"));
	
	// Check if the passed params are valid
	if (!CallerPlayerState || (ChannelID == 0))
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("Invalid params passed to ReceivePhoneCall!"));
		return;
	}

	// Get the current world ref
	const UWorld* const World = GEngine->GetWorldFromContextObject(GetWorld(), EGetWorldErrorMode::LogAndReturnNull);
	if (!IsValid(World))
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("Invalid World from RecievePhoneCall!"));
		return;
	}

	// Get the OnsetVoipWorldSubsystem
	const UOnsetVoipWorldSubsystem* OnsetVoipWorldSubsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>();
	if (!OnsetVoipWorldSubsystem)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("OnsetVoipWorldSubsystem is NULL!"));
		return;	
	}

	// Get the caller's VOIPTalker
	const UOnsetVoipTalker* CallerPlayerTalker = OnsetVoipWorldSubsystem->GetVoipTalker(CallerPlayerState);
	if (!CallerPlayerTalker)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("CallerPlayerTalker is NULL!"));
		return;	
	}

	// Get the receiving player's VOIPTalker
	UOnsetVoipTalker* ReceivingPlayerTalker = OnsetVoipWorldSubsystem->GetVoipTalker(ReceivingPlayerState);
	if (!ReceivingPlayerTalker)
	{
		UE_LOG(LogPlayerPhone, Error, TEXT("ReceivingPlayerTalker is NULL!"));
		return;	
	}

	// Broadcast the delegate for receiving a call
	//OnCallReceived.ExecuteIfBound(CallerPlayerState);

	// Check to see if the caller player's talker is still in the channel, if so join it, if not cancel everything
	const TArray<UOnsetVoipTalker*> TalkersInChannel = OnsetVoipWorldSubsystem->GetAllTalkersInVoiceChannel(ChannelID);
	if (TalkersInChannel.Num() == 0)
	{
		// No one is in this channel anymore, cancel everything
		UE_LOG(LogPlayerPhone, Log, TEXT("Caller left the call early and the channel is empty!"));
		return;
	}

	// There are people in this channel, check if it's the caller
	for (const UOnsetVoipTalker* OnsetVoipTalker : TalkersInChannel)
	{
		if (CallerPlayerTalker == OnsetVoipTalker)
		{
			// The caller is still in the channel, join them
			ReceivingPlayerTalker->SetVoiceChannel(ChannelID, true);
			break;
		}
	}
	
	UE_LOG(LogPlayerPhone, Log, TEXT("Phone call successfully received and joined!"));
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("Phone call successfully received and joined!"));
}