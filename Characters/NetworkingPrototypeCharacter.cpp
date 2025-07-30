// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkingPrototypeCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "OnsetVoipLocalPlayerSubsystem.h"
#include "PlayerPhone.h"
#include "Characters/Item.h"
#include "Characters/SlateItem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/ItemInterface.h"
#include "Net/UnrealNetwork.h"
#include "Public/InteractableInterface.h"
#include "Components/WidgetComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "NetworkingPrototype/NetworkingPrototypeGameMode.h"
#include "Perception/AIPerceptionComponent.h"

class UAISense_Sight;
DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ANetworkingPrototypeCharacter

ANetworkingPrototypeCharacter::ANetworkingPrototypeCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	// Allow only owning player to see hands
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
	MaxSprintValue = 100.f;
	SprintDrainRate = 5.f;
	CurrentSprintValue = MaxSprintValue;
	
	NormalSpeed = GetCharacterMovement()->MaxWalkSpeed;
	SprintingSpeed = NormalSpeed * 2;
	RegularNormalSpeed = NormalSpeed;
	RegularSprintingSpeed = SprintingSpeed;
	SlateNormalSpeed = NormalSpeed / 2;
	SlateSprintingSpeed = SprintingSpeed / 2;
	UnlimitedSprint = false;

	Popup = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPopup"));
	Popup->SetWidgetSpace(EWidgetSpace::Screen);
	Popup->SetDrawSize(FVector2D(300.f, 300.f));
	Popup->SetOnlyOwnerSee(true);

	HoldPopup = CreateDefaultSubobject<UWidgetComponent>(TEXT("HoldInteractionPopup"));
	HoldPopup->SetWidgetSpace(EWidgetSpace::Screen);
	HoldPopup->SetDrawSize(FVector2D(300.f, 300.f));
	HoldPopup->SetOnlyOwnerSee(true);

	FootprintComponent = CreateDefaultSubobject<UFootprintComponent>(TEXT("FootprintComponent"));
	
	bReplicates = true;
	AActor::SetReplicateMovement(true);

	// Create Stimulus for AI Perception to detect player
	StimulusSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("Stimulus Source"));
}

void ANetworkingPrototypeCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
	GetWorld()->GetTimerManager().SetTimer(FocusTimerHandle, this, &ANetworkingPrototypeCharacter::FocusCall, 0.1f, true);
	GetWorld()->GetTimerManager().SetTimer(FillDashMeterTimerHandle, this, &ANetworkingPrototypeCharacter::FillDashMeter, 0.1f, true);
	Popup->SetVisibility(false);
	HoldPopup->SetVisibility(false);

	if (HoldPopup->GetWidget())
	{
		HoldWidget = Cast<UHoldToInteractWidget>(HoldPopup->GetWidget());
	}
	// Ensure the server spawns the phone for all players
	// The server spawns individual phone instances for each player and ensures they are replicated to the clients.
	if (HasAuthority()) // Only the server should execute this
	{
		SpawnPlayerPhone(); // Spawn phone for this player
	}

	SetupStimulusSource();

	// Quick Check to make sure Stimulus Source is still there
	if (StimulusSource)
	{
		UE_LOG(LogTemp, Display, TEXT("Stimulus Source EXISTS on Player"));
	}
}

//////////////////////////////////////////////////////////////////////////// Input

// Primary use of currently held item
void ANetworkingPrototypeCharacter::UseItem()
{
	// Only use the item if we have an item held and are not holding a slate
	if(ItemHeld && !SlateHeld)
	{
		ItemHeld->UseItem(this);
	}
}

//
void ANetworkingPrototypeCharacter::EndUseItem()
{
	if(ItemHeld && (ItemHeld->GetClickType() == E_ClickType::Hold))
	{
		ItemHeld->OnEndUseItem.ExecuteIfBound(this);
	}
}

// Alternate Use if Item Held is bound it
void ANetworkingPrototypeCharacter::AltUseItem()
{
	if(ItemHeld && (ItemHeld->OnAltUse.ExecuteIfBound()))
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("ALT fire used successfully"));
	}
}

void ANetworkingPrototypeCharacter::SpawnPlayerPhone()
{
	if (!PlayerPhoneClass)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("PlayerPhoneClass is not set!"));
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		const FVector SpawnLocation = GetActorLocation();
		const FRotator SpawnRotation = GetActorRotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Server spawns the phone for this player
		PlayerPhone = World->SpawnActor<APlayerPhone>(PlayerPhoneClass, SpawnLocation, SpawnRotation, SpawnParams);

		if (PlayerPhone)
		{
			PlayerPhone->SetOwner(this);  // Assign ownership
			PlayerPhone->SetReplicates(true);  // Ensure it replicates to clients

			// Inform the client to attach the phone to the character
			AttachPlayerPhone();
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Error, TEXT("Failed to spawn PlayerPhone!"));
		}
	}
}

void ANetworkingPrototypeCharacter::AttachPlayerPhone()
{
		if (!PlayerPhone)
		{
			UE_LOG(LogTemplateCharacter, Error, TEXT("Tried to attach phone but phone does not exist!"));
		}
		// Attach the phone to our left hand
		const EAttachmentRule LocationRule = EAttachmentRule::SnapToTarget;
		const EAttachmentRule RotationRule = EAttachmentRule::KeepRelative;
		const EAttachmentRule ScaleRule = EAttachmentRule::KeepRelative;

		const FAttachmentTransformRules AttachmentTransformRules(LocationRule, RotationRule, ScaleRule, true);
		const FName ArmSocketName = FName("GripPoint_l");
			
		PlayerPhone->AttachToComponent(Mesh1P, AttachmentTransformRules, ArmSocketName);
			
		UE_LOG(LogTemplateCharacter, Log, TEXT("Player phone spawned and assigned to character."));
}

void ANetworkingPrototypeCharacter::TogglePhone()
{	
	if (!bHasPhoneUp)
	{
		PullUpPhone();
	}
	else
	{
		PutDownPhone();
	}
}

void ANetworkingPrototypeCharacter::PullUpPhone()
{
	UAnimInstance* AnimInstance = GetMesh1P()->GetAnimInstance();
	if (AnimInstance && PullPhoneUpMontage)
	{
		// If we are already pulling up the phone, don't let the player spam
		if (AnimInstance->Montage_IsPlaying(PullPhoneUpMontage) || AnimInstance->Montage_IsPlaying(PutPhoneDownMontage))
		{
			return;
		}

		// Tell the phone to play its respective anim montages
		if (PlayerPhone)
		{
			PlayerPhone->PullUpPhone();
		}
		
		// Play the phone pull up montage
		AnimInstance->Montage_Play(PullPhoneUpMontage);

		// Bind delegates properly before setting them
		FOnMontageBlendingOutStarted BlendingOutDelegate;
		BlendingOutDelegate.BindUObject(this, &ANetworkingPrototypeCharacter::KeepPhoneUp);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, PullPhoneUpMontage);

		FOnMontageEnded MontageEndedDelegate;
		MontageEndedDelegate.BindUObject(this, &ANetworkingPrototypeCharacter::KeepPhoneUp);
		AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, PullPhoneUpMontage);

		// Set flag
		bHasPhoneUp = true;
	}
}

void ANetworkingPrototypeCharacter::KeepPhoneUp(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* AnimInstance = GetMesh1P()->GetAnimInstance();
	if (AnimInstance && KeepPhoneUpMontage)
	{
		// If we are already pulling up the phone, don't let the player spam
		if (AnimInstance->Montage_IsPlaying(PullPhoneUpMontage) || AnimInstance->Montage_IsPlaying(PutPhoneDownMontage))
		{
			return;
		}
		
		// Ensure phone stays up
		AnimInstance->Montage_Play(KeepPhoneUpMontage);
	}
}

void ANetworkingPrototypeCharacter::PutDownPhone()
{
	UAnimInstance* AnimInstance = GetMesh1P()->GetAnimInstance();
	if (AnimInstance && PutPhoneDownMontage)
	{
		// If we are already putting down the phone, don't let the player spam
		if (AnimInstance->Montage_IsPlaying(PutPhoneDownMontage) || AnimInstance->Montage_IsPlaying(PullPhoneUpMontage))
		{
			return;
		}
		
		if (PlayerPhone)
		{
			PlayerPhone->PutDownPhone();
		}
		
		// Play the phone put-down montage
		AnimInstance->Montage_Play(PutPhoneDownMontage);

		// Set flag
		bHasPhoneUp = false;
	}
}

// Function called when ItemHeld var is changed, ONLY RUNS ON CLIENT PLAYERS
void ANetworkingPrototypeCharacter::OnRep_ItemHeld()
{
	if (ItemHeld)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("ItemHeld changed to: %s"), *ItemHeld->GetName());
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("ItemHeld is now null."));
	}

	// Broadcast the change event so clients can react
	OnItemHeldChanged.Broadcast(ItemHeld);
}

// Function called when SlateHeld var is changed, ONLY RUNS ON CLIENT PLAYERS
void ANetworkingPrototypeCharacter::OnRep_SlateHeld() const
{
	if (SlateHeld)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("SlateHeld changed to: %s"), *SlateHeld->GetName());
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("SlateHeld is now null."));
	}

	// Broadcast the change event so clients can react
	OnSlateHeldChanged.Broadcast(SlateHeld);
}

void ANetworkingPrototypeCharacter::FillDashMeter()
{
	if (!UnlimitedSprint)
	{
		//Refilling
		if (SprintBarCanRefill && !CurrentlySprinting && CurrentSprintValue < MaxSprintValue)
		{
			CurrentSprintValue += SprintDrainRate;
			if (CurrentSprintValue > MaxSprintValue)
			{
				CurrentSprintValue = MaxSprintValue;
				SprintBarCanRefill = false;
			}
		}
		//Depleting
		else if (CurrentlySprinting && CurrentSprintValue > 0)
		{
			CurrentSprintValue -= SprintDrainRate;
			if (CurrentSprintValue <= 0)
			{
				CurrentSprintValue = 0;
				ServerStopSprinting();
			}
		}
	}
}

void ANetworkingPrototypeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANetworkingPrototypeCharacter, CurrentlySprinting);
	DOREPLIFETIME(ANetworkingPrototypeCharacter, SprintingSpeed);
	DOREPLIFETIME(ANetworkingPrototypeCharacter, NormalSpeed);
	DOREPLIFETIME(ANetworkingPrototypeCharacter, SprintBarCanRefill);
	DOREPLIFETIME(ANetworkingPrototypeCharacter, UnlimitedSprint);

	DOREPLIFETIME(ANetworkingPrototypeCharacter, PlayerPhone);
	DOREPLIFETIME(ANetworkingPrototypeCharacter, ItemHeld);
	DOREPLIFETIME(ANetworkingPrototypeCharacter, SlateHeld);

	DOREPLIFETIME(ANetworkingPrototypeCharacter, bIsAlive);
}

void ANetworkingPrototypeCharacter::OnRep_IsSprinting() const
{
	GetCharacterMovement()->MaxWalkSpeed = CurrentlySprinting ? SprintingSpeed : NormalSpeed;
}

void ANetworkingPrototypeCharacter::SetHeldSlate(ASlateItem* NewSlate, ANetworkingPrototypeCharacter* Character)
{
	if (!NewSlate)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("NewSlate Invalid."));
		return;
	}

	if (HasAuthority())
	{
		Character->SlateHeld = NewSlate;
		OnSlateHeldChanged.Broadcast(SlateHeld);
	}
	else
	{
		Server_SetHeldSlate(NewSlate, Character);
	}
}

void ANetworkingPrototypeCharacter::Server_SetHeldSlate_Implementation(ASlateItem* NewSlate,
	ANetworkingPrototypeCharacter* Character)
{
	if (HasAuthority())
	{
		SetHeldSlate(NewSlate, Character);
	}
}

float ANetworkingPrototypeCharacter::GetSprintPercentage()
{
	return CurrentSprintValue / MaxSprintValue;
}

void ANetworkingPrototypeCharacter::Server_DropItem_Implementation(AActor* PlayerUser, bool Respawn)
{
	DropItem(PlayerUser, Respawn);
}

bool ANetworkingPrototypeCharacter::Server_DropItem_Validate(AActor* PlayerUser, bool Respawn)
{
	return true;
}

void ANetworkingPrototypeCharacter::OnRep_IsAlive()
{
	GEngine->AddOnScreenDebugMessage
	(-1, 1.0f, FColor::Yellow, TEXT("OnRep IsAlive"));
	
	if (bIsAlive)
	{
		// Set custom collision on Capsule Comp
		GetCapsuleComponent()->SetCollisionProfileName("Pawn");

		// Set our 3P mesh to visible
		GetMesh()->SetVisibility(true);

		// Set our phone to not hidden in game
		GetPlayerPhone()->SetActorHiddenInGame(false);

		//
		if (IsLocallyControlled())
		{
			ANetworkingPrototypePlayerController* PC = Cast<ANetworkingPrototypePlayerController>(GetController());
			if (PC)
			{
				// Get the player state
				APlayerState* PS = PC->GetPlayerState<APlayerState>();

				// UnMute this player's proximity chat
				PC->Server_MuteTalkerInChannelForAll(false, PS, 0);

				// Remove the player from the ghost channel (3)
				PC->Server_SetVoiceChannel(3,false, PS);
			}
		}
	}
	else
	{
		// Set custom collision on Capsule Comp
		GetCapsuleComponent()->SetCollisionProfileName("IgnoreOnlyPawn");

		// Set our 3P mesh to visible
		GetMesh()->SetVisibility(false);

		// Set our phone to not hidden in game
		GetPlayerPhone()->SetActorHiddenInGame(true);

		//
		if (IsLocallyControlled())
		{
			ANetworkingPrototypePlayerController* PC = Cast<ANetworkingPrototypePlayerController>(GetController());
			if (PC)
			{
				// Get the player state
				APlayerState* PS = PC->GetPlayerState<APlayerState>();

				// Mute this player's proximity chat
				PC->Server_MuteTalkerInChannelForAll(true, PS, 0);

				// Leave any call the player is in
				GetPlayerPhone()->Server_LeaveCurrentPhoneChannel(PS);
				
				// Add the player to the ghost channel (3)
				PC->Server_SetVoiceChannel(3,true, PS);
			}
		}
	}
}

void ANetworkingPrototypeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// No Jumping Necessary
		// EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		// EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANetworkingPrototypeCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANetworkingPrototypeCharacter::Look);

		// Interacting
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ANetworkingPrototypeCharacter::Interact);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ANetworkingPrototypeCharacter::CancelHoldInteract);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ANetworkingPrototypeCharacter::CancelHoldInteract);

		// Voice Chat
		//EnhancedInputComponent->BindAction(VoiceChatAction, ETriggerEvent::Started, this, &ANetworkingPrototypeCharacter::StartTalking);
		//EnhancedInputComponent->BindAction(VoiceChatAction, ETriggerEvent::Canceled, this, &ANetworkingPrototypeCharacter::StopTalking);
		//EnhancedInputComponent->BindAction(VoiceChatAction, ETriggerEvent::Completed, this, &ANetworkingPrototypeCharacter::StopTalking);

		// Item Usage
		//EnhancedInputComponent->BindAction(UseItemAction, ETriggerEvent::Triggered, this, &ANetworkingPrototypeCharacter::ServerUseItem);
		EnhancedInputComponent->BindAction(UseItemAction, ETriggerEvent::Triggered, this, &ANetworkingPrototypeCharacter::UseItem);
		//EnhancedInputComponent->BindAction(UseItemAction, ETriggerEvent::Completed, this, &ANetworkingPrototypeCharacter::ServerEndUseItem);
		EnhancedInputComponent->BindAction(UseItemAction, ETriggerEvent::Completed, this, &ANetworkingPrototypeCharacter::EndUseItem);

		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &ANetworkingPrototypeCharacter::DropItemWrapper);

		// Alternate Item Use
		//EnhancedInputComponent->BindAction(AltUse, ETriggerEvent::Triggered, this, &ANetworkingPrototypeCharacter::ServerAltUseItem);
		EnhancedInputComponent->BindAction(AltUse, ETriggerEvent::Triggered, this, &ANetworkingPrototypeCharacter::AltUseItem);

		// Sprinting
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ANetworkingPrototypeCharacter::ServerStartSprinting);
		//Only Start Refilling Bar When Player Releases Sprint Button
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ANetworkingPrototypeCharacter::ServerResetSprintBarFill);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ANetworkingPrototypeCharacter::ServerResetSprintBarFill);

		// Phone
		EnhancedInputComponent->BindAction(PhoneAction, ETriggerEvent::Started, this, &ANetworkingPrototypeCharacter::TogglePhone);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

APlayerPhone* ANetworkingPrototypeCharacter::GetPlayerPhone() const
{
	return PlayerPhone;
}

void ANetworkingPrototypeCharacter::SetHeldItem(AItem* NewItem, ANetworkingPrototypeCharacter* Character)
{
	if (HasAuthority())
	{
		// This will call OnRep_ItemHeld on clients
		Character->ItemHeld = NewItem;

		// Explicitly broadcast OnItemHeldChanged for the server's own animations
		OnItemHeldChanged.Broadcast(ItemHeld);
	}
	else
	{
		// If not the server, ask the server to update ItemHeld
		Server_SetHeldItem(NewItem, Character);
	}
}

void ANetworkingPrototypeCharacter::Server_SetHeldItem_Implementation(AItem* NewItem,
	ANetworkingPrototypeCharacter* Character)
{
	// Server sets ItemHeld, triggers OnRep on clients
	SetHeldItem(NewItem, Character);
}

void ANetworkingPrototypeCharacter::SetUnlimitedSprint_Implementation(bool IsSprintUnlimited)
{
	UnlimitedSprint = IsSprintUnlimited;
}

void ANetworkingPrototypeCharacter::KillPlayer()
{
	GEngine->AddOnScreenDebugMessage
	(-1, 1.0f, FColor::Red, TEXT("Killing Player >:)"));
	HandleDeath();
}

void ANetworkingPrototypeCharacter::RevivePlayer(const FVector& RespawnLocation)
{
	GEngine->AddOnScreenDebugMessage
(-1, 1.0f, FColor::Green, TEXT("Reviving Player :)"));
	HandleRespawn(RespawnLocation);
}

void ANetworkingPrototypeCharacter::HandleRespawn(const FVector& RespawnLocation)
{
	APlayerState* LocalPS = GetPlayerState();
	if (LocalPS)
	{
		Server_SetIsAlive(true);
		
		// OnRep_IsAlive won't be called on Host, so we manually call it here
		if (HasAuthority())
		{
			OnRep_IsAlive();
		}
		
		Server_HandleRespawn(RespawnLocation, LocalPS);
	}

	// Optional local changes to player here such as a post process effect:
	// EnableInput(nullptr);
	// PostProcess(off);
}

void ANetworkingPrototypeCharacter::Server_HandleRespawn_Implementation(const FVector& RespawnLocation, APlayerState* PlayerStateToRespawn)
{
	// Only handle respawn if we are the server
	if (HasAuthority())
	{
		if (PlayerStateToRespawn)
		{
			ANetworkingPrototypeGameMode* QUGameMode = GetWorld()->GetAuthGameMode<ANetworkingPrototypeGameMode>();
			if (QUGameMode)
			{
				QUGameMode->RespawnDeadPlayer(RespawnLocation, PlayerStateToRespawn);
			}
		}
	}
}

void ANetworkingPrototypeCharacter::HandleDeath()
{
	// GEngine->AddOnScreenDebugMessage
	// (-1, 1.0f, FColor::Red, TEXT("Handle Death"));
	
	APlayerState* LocalPS = GetPlayerState();
	if (LocalPS)
	{
		Server_SetIsAlive(false);
		
		// OnRep_IsAlive won't be called on Host, so we manually call it here
		if (HasAuthority())
		{
			OnRep_IsAlive();
		}
		
		Server_HandleDeath(LocalPS);
	}

	// Optional local changes to player here such as a post process effect:
	// DisableInput(nullptr);
	// PostProcess(on);
}


void ANetworkingPrototypeCharacter::Server_HandleDeath_Implementation(APlayerState* PlayerStateToKill)
{
	// Only handle death if we are the server
	if (HasAuthority())
	{
	// 	GEngine->AddOnScreenDebugMessage
	// (-1, 1.0f, FColor::Red, TEXT("Server Handle Death"));
		
		if (PlayerStateToKill)
		{
			ANetworkingPrototypeGameMode* QUGameMode = GetWorld()->GetAuthGameMode<ANetworkingPrototypeGameMode>();
			if (QUGameMode)
			{
				QUGameMode->AddDeadPlayer(PlayerStateToKill);
			}
		}
	}
}

void ANetworkingPrototypeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void ANetworkingPrototypeCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ANetworkingPrototypeCharacter::Server_Interact_Implementation(AActor* InteractableItem)
{
	// Ensure interaction is processed by the server
	UE_LOG(LogTemplateCharacter, Log, TEXT("Server handling interaction."));

	// Process the interaction on the server
	if(InteractableItem)
	{
		// Get the interact interface on the actor sent through and call the "Interact" function server side
		IInteractableInterface* InteractInterface = Cast<IInteractableInterface>(InteractableItem);

		switch (InteractInterface->GetInteractType())
		{
		case E_InteractType::Tap:
				InteractInterface->Execute_Interact(InteractableItem, this);
			break;

		case E_InteractType::Hold:

				// GEngine->AddOnScreenDebugMessage
				// (-1, 1.0f, FColor::Green, TEXT("Start Held Interaction"));
							
				// Call the StartHold function on this interactable object
				InteractInterface->Execute_StartHold(InteractableItem, this);
			break;
			
		}
	}
}

void ANetworkingPrototypeCharacter::Interact()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("Interact Key Pressed!"));

	FVector Start = FirstPersonCameraComponent->GetComponentLocation();
	FVector ForwardVector = FirstPersonCameraComponent->GetForwardVector();
	FVector End = ((ForwardVector * 200.f) + Start);
    
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;

	// Debug line trace
	// if(GetWorld())
	// {
	// 	DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 1.0f, 0, 1.0f); // Line color: Green
	// }

	// Actual line trace
	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams))
	{
		// Store hit actor as a var
		AActor* Actor = HitResult.GetActor();

		// Compact version:
		// Check to see if the hit actor implements the InteractableInterface
		if(IInteractableInterface* InteractInterface = Cast<IInteractableInterface>(Actor))
		{
			if (InteractInterface->GetCanInteract())
			{
				if(HasAuthority())
				{
					switch (InteractInterface->GetInteractType())
					{
						case E_InteractType::Tap:
							// For Calling BP/CPP Implementation
							InteractInterface->Execute_Interact(Actor, this);
							// For CPP only Implementation
							//InteractInterface->InteractPure(this);
							break;

						case E_InteractType::Hold:

							// GEngine->AddOnScreenDebugMessage
							// (-1, 1.0f, FColor::Green, TEXT("Start Held Interaction"));
						
							// Call the StartHold function on this interactable object
							InteractInterface->Execute_StartHold(Actor, this);

							// If we don't have a buffer for this object yet, or the ref is different from the actor
							// we are now holding E on, keep a reference to it.
							if (!CurrentHoldProgressionActor && CurrentHoldProgressionActor != Actor)
							{
								CurrentHoldProgressionActor = Actor;
							}
							break;
					}
				}
				else
				{
					// If we the client are holding E on something
					if (InteractInterface->GetInteractType() == E_InteractType::Hold)
					{
						if (!CurrentHoldProgressionActor && CurrentHoldProgressionActor != Actor)
						{
							// Store the actor ref client side first
							CurrentHoldProgressionActor = Actor;
						}
					}
					// Client is requesting interaction
					Server_Interact(Actor);
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("Can't interact"));
			}
		}
	}
}

void ANetworkingPrototypeCharacter::Server_CancelHoldInteract_Implementation(AActor* InteractableItem)
{
	// If we were holding E on an actor
	if (InteractableItem)
	{
		IInteractableInterface* InteractableInterface = Cast<IInteractableInterface>(InteractableItem);
		
		if (InteractableInterface && (InteractableInterface->GetInteractType() == E_InteractType::Hold))
		{
			// Force call it's StopHold function
			InteractableInterface->Execute_StopHold(InteractableItem, this);	
		}
	}
}

void ANetworkingPrototypeCharacter::CancelHoldInteract()
{
	
	// GEngine->AddOnScreenDebugMessage
	// (-1, 1.0f, FColor::Red, TEXT("Cancel Held Interaction"));
	
	if (HasAuthority())
	{
		// If we were holding E on an actor
		if (CurrentHoldProgressionActor)
		{
			IInteractableInterface* InteractableInterface = Cast<IInteractableInterface>(CurrentHoldProgressionActor);
		
			if (InteractableInterface && (InteractableInterface->GetInteractType() == E_InteractType::Hold))
			{
				// Force call it's StopHold function
				InteractableInterface->Execute_StopHold(CurrentHoldProgressionActor, this);	
				CurrentHoldProgressionActor = nullptr;
			}
		}
	}
	else
	{
		if (CurrentHoldProgressionActor)
		{
			Server_CancelHoldInteract(CurrentHoldProgressionActor);
			CurrentHoldProgressionActor = nullptr;
		}
	}
}

void ANetworkingPrototypeCharacter::FocusCall()
{
	if (IsLocallyControlled())
	{
		FVector Start = FirstPersonCameraComponent->GetComponentLocation();
		FVector ForwardVector = FirstPersonCameraComponent->GetForwardVector();
		FVector End = ((ForwardVector * 200.f) + Start);
    
		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;

		// Actual line trace
		if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams))
		{
			// Store hit actor as a var
			AActor* Actor = HitResult.GetActor();

			// Compact version:
			// Check to see if the hit actor implements the InteractableInterface
			if(IInteractableInterface* InteractInterface = Cast<IInteractableInterface>(Actor))
			{
				if (InteractInterface->GetCanInteract())
				{
					FocusedObject = Actor;

					// Depending on the interact type, we show press or hold E widget
					switch (InteractInterface->GetInteractType())
					{
						case E_InteractType::Tap:
							// Show press E widget
							if (Popup)
							{
								if (!Popup->IsVisible())
								{
									Popup->SetVisibility(true);
								}
								Popup->SetWorldLocation(HitResult.Location - ForwardVector);

								// If we had a reference to a hold interactable, deref it
								if (HoldWidget)
								{
									HoldWidget->UpdateInteractableActor(nullptr);
								}
							}
							break;

						case E_InteractType::Hold:
							// Show Hold E widget
							if (HoldPopup)
							{
								if (!HoldPopup->IsVisible())
								{
									HoldPopup->SetVisibility(true);
								}
								
								HoldPopup->SetWorldLocation(HitResult.Location - ForwardVector);

								if (HoldWidget && (HoldWidget->GetInteractableActor() != Actor))
								{
									HoldWidget->UpdateInteractableActor(Actor);
								}
							}
							break;
					}
				}
				else
				{
					UnfocusCall();
				}
			}
			else
			{
				UnfocusCall();
			}
		}
		else
		{
			UnfocusCall();
		}
	}
}

void ANetworkingPrototypeCharacter::UnfocusCall()
{
	if (FocusedObject)
	{
		Popup->SetVisibility(false);
		Popup->SetWorldLocation(GetActorLocation());
		HoldPopup->SetVisibility(false);
		HoldPopup->SetWorldLocation(GetActorLocation());
		// If we had a reference to a hold interactable, deref it
		if (HoldWidget)
		{
			HoldWidget->UpdateInteractableActor(nullptr);
		}
		FocusedObject = nullptr;
	}
}

void ANetworkingPrototypeCharacter::StartTalking()
{
	if (GetWorld())
	{		
		// //VOIPTalkerComponent->OnTalkingBegin();
		// const APlayerController* PlayerController = Cast<APlayerController>(GetController());
		// if(PlayerController)
		// {
		// 	UOnsetVoipLocalPlayerSubsystem* OnsetVoipLocalPlayerSubsystem =
		// 		GetWorld()->GetSubsystem<UOnsetVoipLocalPlayerSubsystem>();
		// 	
		// 	if (!OnsetVoipLocalPlayerSubsystem->IsCapturingVoice())
		// 	{
		// 		OnsetVoipLocalPlayerSubsystem->ToggleVoiceCapture(!OnsetVoipLocalPlayerSubsystem->IsCapturingVoice());
		// 	}
		// 	
		// 	//PlayerController->ConsoleCommand("ToggleSpeaking 1");
		// 	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("Started Talking: Command Sent Successfully"));
		// }
		// else
		// {
		// 	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("No Player Controller"));
		// }
	}
}

void ANetworkingPrototypeCharacter::StopTalking()
{
	
	if (GetWorld())
	{
		//VOIPTalkerComponent->OnTalkingEnd();
		APlayerController* PlayerController = Cast<APlayerController>(GetController());
		if(PlayerController)
		{
			PlayerController->ConsoleCommand("ToggleSpeaking 0");
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("Stopped Talking: Command Sent Successfully"));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("No Player Controller"));
		}
	}
}

void ANetworkingPrototypeCharacter::DropItem(AActor* PlayerUser, bool Respawn)
{
	if (HasAuthority())
	{
		if (SlateHeld != nullptr)
		{
			DisableSlateSpeed();
			SlateHeld->DropItem(PlayerUser, Respawn);
			SlateHeld = nullptr;

			// Broadcast that we now are not  holding no item
			OnItemHeldChanged.Broadcast(ItemHeld);
		}
		else if (ItemHeld != nullptr)
		{
			ItemHeld->DropItem(PlayerUser, Respawn);
			ItemHeld = nullptr;

			// Broadcast that we now are holding no item
			OnItemHeldChanged.Broadcast(nullptr);
		}
	}
	else
	{
		Server_DropItem(PlayerUser, Respawn);
	}
}

void ANetworkingPrototypeCharacter::DropItemWrapper()
{
	DropItem(this, true);
}

void ANetworkingPrototypeCharacter::Server_SetIsAlive_Implementation(bool newAlive)
{
	GEngine->AddOnScreenDebugMessage
	(-1, 1.0f, FColor::Red, TEXT("Server_SetIsAlive"));
	bIsAlive = newAlive;
}

/** Sets up Stimulus Source for AI to detect player */
void ANetworkingPrototypeCharacter::SetupStimulusSource() const
{
	if (!StimulusSource)
	{
		UE_LOG(LogTemp, Display, TEXT("Stimulus Source is InValid"));
	}
	else
	{
		StimulusSource->bAutoRegister = true;
		StimulusSource->RegisterForSense(UAISense_Sight::StaticClass());
		StimulusSource->RegisterWithPerceptionSystem();
		UE_LOG(LogTemp, Display, TEXT("Stimulus Source registered Successfully"));
	}
	
}

void ANetworkingPrototypeCharacter::EnableSlateSpeed()
{
	NormalSpeed = SlateNormalSpeed;
	SprintingSpeed = SlateSprintingSpeed;
	OnRep_IsSprinting();
}

void ANetworkingPrototypeCharacter::DisableSlateSpeed()
{
	NormalSpeed = RegularNormalSpeed;
	SprintingSpeed = RegularSprintingSpeed;
	OnRep_IsSprinting();
}

void ANetworkingPrototypeCharacter::Multicast_ResetSprintBarFill_Implementation()
{
	if (HasAuthority())
	{
		SprintBarCanRefill = true;
		ServerStopSprinting();
	}
}

void ANetworkingPrototypeCharacter::Multicast_StartSprint_Implementation()
{
	if (HasAuthority())
	{
		CurrentlySprinting = true;
		SprintBarCanRefill = false;
		OnRep_IsSprinting();
		OnStartSprint.Broadcast();
	}
}

void ANetworkingPrototypeCharacter::Multicast_StopSprint_Implementation()
{
	if (HasAuthority())
	{
		CurrentlySprinting = false;
		OnRep_IsSprinting();
		OnStopSprint.Broadcast();
	}
}

// Server RPCs
void ANetworkingPrototypeCharacter::ServerResetSprintBarFill_Implementation()
{
	Multicast_ResetSprintBarFill();
}

void ANetworkingPrototypeCharacter::ServerStartSprinting_Implementation()
{
	Multicast_StartSprint();
}

void ANetworkingPrototypeCharacter::ServerStopSprinting_Implementation()
{
	Multicast_StopSprint();
}

