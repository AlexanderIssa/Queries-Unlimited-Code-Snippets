// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkingPrototype/Ghost/Ghost.h"

#include "BasicDoor.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Slate/SGameLayerManager.h"

// Sets default values
AGhost::AGhost()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	/** Setting up Root Component */
	RootComponent = GetCapsuleComponent();

	// Setting up Mesh Attachment
	GetMesh()->SetupAttachment(GetCapsuleComponent());

	/** Ensuring Components are set up correctly */

	// Setting up Capsule Component and Overlap event
	GetCapsuleComponent()->SetRelativeLocation(FVector::Zero());
	
	// Setting Mesh Location to match Capsule Collier
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -(GetCapsuleComponent()->GetScaledCapsuleHalfHeight())));

	// Create and setup Sphere Component //
	SphereComponent = CreateDefaultSubobject<USphereComponent>(FName("DoorDetector"));
	SphereComponent->SetupAttachment(GetMesh());
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereComponent->SetSphereRadius(50.0f);
	SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AGhost::OnOverlapBegin);

	GetMesh()->SetIsReplicated(true);

	// Default create an audio component
	GhostAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("GhostAudioComp"));
	// Set Audio Parent to the Ghost skeleton
	GhostAudioComponent->SetupAttachment(GetMesh());
	GhostAudioComponent->bAutoActivate = false;
	// Allow 3D sound
	GhostAudioComponent->bAllowSpatialization = true;

	// Replicate Actor to be able to transfer data to clients
	bReplicates = true;
}

/**
 * Function changes the Ghost's visibility and Collision configs to ignore players.
 * ** Add VFX effects when available ** 
 */
void AGhost::TurnInvisible_Implementation() const
{
	GetMesh()->SetVisibility(false);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECR_Ignore);
}

/**
 * Manifest the Ghost (Turn her visible and reset Collision)
 * ** Add VFX effects when available ** 
 */
void AGhost::Manifest_Implementation() const
{
	GetMesh()->SetVisibility(true);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECR_MAX);
}

void AGhost::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ABasicDoor* Door = Cast<ABasicDoor>(OtherActor))
	{
		if (!Door->IsOpen() && Door->GhostInteractable)
		{
			Door->ForceToggleDoor(this);
		}
	}

	// When Haunting and overlap with a player, kill them instead of target and EndHaunt
	if (GetGhostState() == E_GhostState::Chasing)
	{
		if (ANetworkingPrototypeCharacter* Player = Cast<ANetworkingPrototypeCharacter>(OtherActor))
		{
			if (GhostAIController && Player->GetIsAlive())
			{
				// ALEX DISABLED FOR NOW SINCE IT MESSES UP ORDER OF OPERATIONS
				// This will redirect the player to the new player that it overlaps with so
				// they can die instead of killing a random player
				GhostAIController->SetTargetPlayer(Player);
				
			}
		}
	}
}

E_GhostState AGhost::GetGhostState() const
{
	return CurrentGhostState;
}

void AGhost::SetGhostState(E_GhostState NewGhostState)
{
	if (HasAuthority())
	{
		if ((NewGhostState != CurrentGhostState))
		{
			CurrentGhostState = NewGhostState;
			OnRep_CurrentGhostState();
		}
	}
	else
	{
		Server_SetGhostState(NewGhostState);
	}
}

void AGhost::Server_SetGhostState_Implementation(E_GhostState NewGhostState)
{
	SetGhostState(NewGhostState);
}


// Called when the game starts or when spawned
void AGhost::BeginPlay()
{
	Super::BeginPlay();

	// Make Ghost invisible when Patrolling
	TurnInvisible();

	GhostAIController = Cast<AGhostAIController>(GetController());
	if (HasAuthority())
	{
		GhostAIController->SetReplicates(true);
	}
	if (GhostAIController != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Turquoise, FString(TEXT("GhostAIController Setup Complete")));
	}
	
}

// Called every frame
void AGhost::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AGhost::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AGhost::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGhost, CurrentGhostState);
	DOREPLIFETIME(AGhost, GhostAudioComponent);
}

void AGhost::Distract(ACharacter* Distractor)
{
	if (GhostAIController)
	{
		GetGhostAIController()->Distract(Distractor);
	}
}

void AGhost::EndDistract()
{
	if(GetGhostAIController())
	{
		GetGhostAIController()->Undistract();
	}
}

void AGhost::Stun(const float Duration)
{
	// TODO: Fix issue where Ghost AI Controller isn't retrievable from client
	if(GetGhostAIController())
	{
		GetGhostAIController()->Stun(Duration);
	}
}


void AGhost::Server_PlayAudio_Implementation(USoundBase* SoundCue)
{
	Multicast_PlayAudio(SoundCue);
}

void AGhost::Multicast_PlayAudio_Implementation(USoundBase* SoundCue)
{
	if (SoundCue != nullptr)
	{
		GhostAudioComponent->SetSound(SoundCue);
		GhostAudioComponent->Play();
	}
}

void AGhost::Server_Teleport_Implementation(const FVector& TeleportLocation)
{
	SetActorLocation(TeleportLocation);
}

void AGhost::Server_GetGhostAIController_Implementation(AGhostAIController* output)
{
	output = GetGhostAIController();
}

AGhostAIController* AGhost::GetGhostAIController()
{
	if (!GhostAIController)
	{
		Server_GetGhostAIController(GhostAIController);
	}
	return GhostAIController;
}

void AGhost::SetGhostAIController(AGhostAIController* NewGhostAIController)
{
	GhostAIController = NewGhostAIController;
}

void AGhost::PlayKillAnimWithDelegate(AGhostAIController* OwningController)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && KillPlayerMontage && !AnimInstance->Montage_IsPlaying(KillPlayerMontage))
	{
		// Alex: THIS TOOK ME FOREVER, you have to play the montage BEFORE you bind the
		// delegates to properly bind them
		AnimInstance->Montage_Play(KillPlayerMontage);
		
		FOnMontageBlendingOutStarted BlendingOutDelegate;
		BlendingOutDelegate.BindUObject(OwningController, &AGhostAIController::PostKillAnim);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, KillPlayerMontage);
	}
}

bool AGhost::IsHaunting()
{
	if (GhostAIController && HasAuthority())
	{
		return GhostAIController->GetBlackboardComponent()->GetValueAsBool(TEXT("isHaunting"));
	}

	return false;
}

void AGhost::ForceHaunt(AActor* NewTarget)
{
	if (GhostAIController && HasAuthority())
	{
		// Only force a haunt if the ghost is not stunned and is not haunting
		if (!GhostAIController->GetBlackboardComponent()->GetValueAsBool(TEXT("Stunned"))
			&& !GhostAIController->GetBlackboardComponent()->GetValueAsBool(TEXT("isHaunting")))
		{
			GhostAIController->GetBlackboardComponent()->SetValueAsFloat(TEXT("AggroMeter"), 0.5f);
			GhostAIController->GetBlackboardComponent()->SetValueAsBool(TEXT("CanTeleport"), true);
		
			if (NewTarget != nullptr)
			{
				GhostAIController->GetBlackboardComponent()->SetValueAsObject(TEXT("TargettedPlayer"), NewTarget);
			}
		}
	}
}

void AGhost::SetPassiveMultiplier(float NewMultiplier)
{
	if (GhostAIController && HasAuthority())
	{
		GhostAIController->GetBlackboardComponent()->SetValueAsFloat(TEXT("PassiveMultiplier"), NewMultiplier);
	}
}

void AGhost::OverridePassiveMultiplierForDuration(float NewMultiplier, float Duration)
{
	if (GhostAIController && HasAuthority())
	{
		DefaultPassiveHauntingMultiplier = GhostAIController->GetBlackboardComponent()->GetValueAsFloat(TEXT("PassiveMultiplier"));

		SetPassiveMultiplier(NewMultiplier);

		if (OverrideHauntingMultiplierTimerHandle.IsValid())
		{
			// Clear the existing timer
			GetWorldTimerManager().ClearTimer(OverrideHauntingMultiplierTimerHandle);
		}

		GetWorldTimerManager().SetTimer(OverrideHauntingMultiplierTimerHandle, this, &AGhost::ResetPassiveMultiplier, Duration, false);
	}
}

void AGhost::ResetPassiveMultiplier()
{
	if (GhostAIController && HasAuthority())
	{
		SetPassiveMultiplier(DefaultPassiveHauntingMultiplier);
	}
}

void AGhost::Multicast_PlayKillAnim_Implementation(AGhostAIController* OwningController)
{
	// Only the server sets the blend out delegate
	if (HasAuthority())
	{
		// Play the Kill player sound
		Server_PlayAudio(KillPlayerSound);
		// Play the kill player anim montage and bind the delegate
		PlayKillAnimWithDelegate(OwningController);
	}
	else
	{
		// Every other client just plays the montage
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && KillPlayerMontage && !AnimInstance->Montage_IsPlaying(KillPlayerMontage))
		{
			AnimInstance->Montage_Play(KillPlayerMontage);
		}
	}
}

void AGhost::OnRep_CurrentGhostState()
{
	// Let any subscribers know about the new GhostState.
	// EX: Anim BPs
	OnGhostStateChanged.Broadcast(CurrentGhostState);
}

