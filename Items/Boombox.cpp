#include "Boombox.h"

#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "NetworkingPrototype/GameStates/QueriesUnlimitedGameState.h"
#include "NetworkingPrototype/Ghost/Ghost.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogBoombox);

ABoombox::ABoombox()
{
	// Setting item ID
	ID = 3;
	
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Make sure this item replicates
	bReplicates = true;

	// Create the root Component
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Create the Static Mesh
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	// Parent the Root with the Mesh
	StaticMesh->SetupAttachment(Root);

	// Default create an audio component
	BoomboxAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BoomboxAudioComp"));
	BoomboxAudioComponent->SetupAttachment(StaticMesh);
	BoomboxAudioComponent->bAutoActivate = false;
	BoomboxAudioComponent->SetIsReplicated(true);
}

void ABoombox::BeginPlay()
{
	Super::BeginPlay();

	OnAltUse.BindUObject(this, &ABoombox::OnAltFireUse);
	// Bind the OnPickup delegate to our Event
	OnPickup.BindUObject(this, &ABoombox::OnPickupItem);
}

void ABoombox::OnCDChosen(const FCDData& CDData)
{
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Emerald,TEXT("Swap CD"));
	
	// Set our new CDData through the server
	Server_SetCurrentCDData(CDData);

	// Put down the CDCase
	BPInsertCDIntoBoombox();
}

void ABoombox::ToggleCDSelectionMode()
{
	if (mUserCharacter && !bCDCaseUp)
	{
		SetShowMouse(true);
		Server_SetCDCaseUp(true);
	}
	else if (mUserCharacter && bCDCaseUp)
	{
		SetShowMouse(false);
		Server_SetCDCaseUp(false);
	}
}

void ABoombox::SetShowMouse(bool bShowMouse)
{
	APlayerController* PC = Cast<APlayerController>(mUserCharacter->GetController());
	if (PC)
	{
		if (bShowMouse)
		{
			PC->SetShowMouseCursor(true);
			PC->SetInputMode(FInputModeGameAndUI());
			PC->bEnableMouseOverEvents = true;
			PC->bEnableClickEvents = true;
		}
		else
		{
			PC->SetShowMouseCursor(false);
			PC->SetInputMode(FInputModeGameOnly());
			PC->bEnableMouseOverEvents = false;
			PC->bEnableClickEvents = false;
		}
	}
}

void ABoombox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

/*
 * @brief Use the Boombox client side.
 * Reveal hidden things only to the owner of this item when
 * @param user The actor that is using the Boombox
 */
void ABoombox::UseItem(AActor* user)
{
	Super::UseItem(user);
	
	if (!user)
	{
		UE_LOG(LogBoombox, Error, TEXT("Passed in user is null!"));
		return;
	}

	if (!bCDCaseUp)
	{
		// Continue some functionality server side such as animations to play
		// to all clients
		Server_UseItem(user);
	}
}

void ABoombox::OnAltFireUse()
{
	if (mUserCharacter && !bCDCaseUp)
	{
		BPPullUpCDCase();	
	}
	else if (mUserCharacter && bCDCaseUp)
	{
		BPPutDownCDCase();
	}
}

void ABoombox::OnPickupItem(ANetworkingPrototypeCharacter* Interactor)
{
	Server_OnPickupItem(Interactor);
}

void ABoombox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABoombox, mUserCharacter);
	DOREPLIFETIME(ABoombox, UnlimitedSprintDuration);
	DOREPLIFETIME(ABoombox, CDCaseActor);
	DOREPLIFETIME(ABoombox, mCurrentCDData);
	DOREPLIFETIME(ABoombox, bCDCaseUp);
}

void ABoombox::Server_OnPickupItem_Implementation(ANetworkingPrototypeCharacter* Interactor)
{
	mUserCharacter = Interactor;
	
	const FVector SpawnLoc = GetActorLocation() + FVector(0, 0, 10); // Slightly above mesh
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	CDCaseActor = GetWorld()->SpawnActor<ACDCaseActor>(CDCaseClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	OnRep_CDCaseActor();
}

void ABoombox::OnRep_CDCaseActor()
{
	// This isn't being called on clients for some reason
	if (mUserCharacter)
	{
		if (mUserCharacter->IsLocallyControlled())
		{
			const EAttachmentRule LocationRule = EAttachmentRule::SnapToTarget;
			const EAttachmentRule RotationRule = EAttachmentRule::KeepRelative;
			const EAttachmentRule ScaleRule = EAttachmentRule::KeepRelative;

			const FAttachmentTransformRules AttachmentTransformRules(LocationRule, RotationRule, ScaleRule, true);
			
			mUserCharacter->AttachToMesh1P(CDCaseActor, AttachmentTransformRules, FName(TEXT("GripPoint_l")));

			if (!CDCaseActor->CDActors.IsEmpty())
			{
				for (ACDActor* CDActor : CDCaseActor->CDActors)
				{
					if (CDActor)
					{
						CDActor->OnCDSelected.AddDynamic(this, &ABoombox::OnCDChosen);
					}
				}

				// Default select the first CD as equipped
				if (CDCaseActor->CDActors[1])
				{
					Server_SetCurrentCDData(CDCaseActor->CDActors[1]->CDInfo);
				}
			}
		}
		else
		{
			const EAttachmentRule LocationRule = EAttachmentRule::SnapToTarget;
			const EAttachmentRule RotationRule = EAttachmentRule::KeepRelative;
			const EAttachmentRule ScaleRule = EAttachmentRule::KeepRelative;

			const FAttachmentTransformRules AttachmentTransformRules(LocationRule, RotationRule, ScaleRule, true);

			mUserCharacter->AttachToMesh3P(CDCaseActor, AttachmentTransformRules, FName(TEXT("GripPoint_l")));
		}
	}
}

void ABoombox::Server_SetCurrentCDData_Implementation(const FCDData& NewCDData)
{
	mCurrentCDData = NewCDData;
}

void ABoombox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	if (CDCaseActor /*&& CDCaseActor->IsPendingKillPending()*/)
	{
		CDCaseActor->Destroy();
	}
}

void ABoombox::Server_SetCDCaseUp_Implementation(bool bNewCaseUp)
{
	bCDCaseUp = bNewCaseUp;
	OnRep_CDCaseUp();
}

void ABoombox::EnableUnlimitedSprint_Implementation(ANetworkingPrototypeCharacter* MyCharacter)
{
	StopUnlimitedSprintDelegate.BindUFunction(this, FName("DisableUnlimitedSprint"), MyCharacter);
	GetWorldTimerManager().SetTimer(UnlimitedSprintTimerHandle, StopUnlimitedSprintDelegate, UnlimitedSprintDuration, false);
	MyCharacter->SetUnlimitedSprint(true);

	// Tell our subscribers that the music has started
	OnMusicStateChanged.Broadcast(true);
}

void ABoombox::DisableUnlimitedSprint_Implementation(ANetworkingPrototypeCharacter* MyCharacter)
{
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Emerald, FString(TEXT("Unlimited Sprint Ended!!!!")));
	GetWorldTimerManager().ClearTimer(UnlimitedSprintTimerHandle);
	MyCharacter->SetUnlimitedSprint(false);

	// Tell our subscribers that the music has ended
	OnMusicStateChanged.Broadcast(false);
}

void ABoombox::OnRep_CDCaseUp()
{
	if (bCDCaseUp)
	{
		CDCaseActor->HideCaseAndCDs(false);
	}
	else
	{
		CDCaseActor->HideCaseAndCDs(true);
	}
}

/*
 * @brief Use the Boombox server side.
 * Calls Multicast_UseItem() so that every client
 * can see that the owner of this item is playing
 * a specific animation.
 */
void ABoombox::Server_UseItem_Implementation(AActor* user)
{
	AQueriesUnlimitedGameState* GS = GetWorld()->GetGameState<AQueriesUnlimitedGameState>();
	if (GS && !GS->IsToolOnCooldown(EToolType::Boombox))
	{
		// Check to see if this passed in user is the right type
		ANetworkingPrototypeCharacter* PassedInUser = Cast<ANetworkingPrototypeCharacter>(user);
		if (!PassedInUser)
		{
			UE_LOG(LogBoombox, Error, TEXT("Passed in user is not of type ANetworkingPrototypeCharacter!"));
			return;
		}

		// Set the user through the server if it hasn't already been set
		if (PassedInUser != mUserCharacter)
		{
			mUserCharacter = PassedInUser;
		}

		// Start the global cooldown for the Boombox
		GS->SetToolUsed(EToolType::Boombox);

		if (mCurrentCDData.Genre == E_MusicGenre::HeavyMetal)
		{
			// TEMP get the ghost from anywhere and distract it
			AActor* GhostActor = UGameplayStatics::GetActorOfClass(GetWorld(), AGhost::StaticClass());
			if (!GhostActor)
			{
				return;
			}

			AGhost* Ghost = Cast<AGhost>(GhostActor);
			if (!Ghost)
			{
				return;
			}

			if (Ghost->IsHaunting())
			{
				// If haunting, distract the ghost
				Ghost->Distract(mUserCharacter);
			}
			else
			{
				// Else, up the haunting multiplier to a new one for a set duration
				Ghost->OverridePassiveMultiplierForDuration(8.0f, 3.0f);
			}
	
			if (mUserCharacter != nullptr)
			{
				EnableUnlimitedSprint(mUserCharacter);
			}
		}

		//
		Multicast_UseItem(user);
	}
	else
	{
		// It's on cooldown
	}
}

/*
 * @brief Use the Boombox multicasted server side.
 * Run any functionality that should run for all clients
 * when the owner of this item uses it. Such as playing
 * an animation on the owner's character server wide.
 */
void ABoombox::Multicast_UseItem_Implementation(AActor* user)
{
	if (!mUserCharacter)
	{
		UE_LOG(LogBoombox, Error, TEXT("Multicast: mUserCharacter is not valid!"));
		return;
	}

	if (BoomboxAudioComponent)
	{
		if (mCurrentCDData.Track)
		{
			BoomboxAudioComponent->SetSound(mCurrentCDData.Track);
			BoomboxAudioComponent->Play();
		}
	}
	
	// Play Boombox's use animation here
	if (mUserCharacter->IsLocallyControlled())
	{
		// Tell our user and subscribers that we pressed the boom box play button
		OnBoomBoxButtonPressed.Broadcast(true);
	}
	else
	{
		// this is where to play the 3rd person animation for all other clients to see that aren't the
		// owning client
	}
}

