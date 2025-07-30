// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkingPrototype/Ghost/GhostAIController.h"

#include "Ghost.h"
#include "NiagaraCommon.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/UnitConversion.h"
#include "NetworkingPrototype/NetworkingPrototypeCharacter.h"
#include "NetworkingPrototype/GameStates/QueriesUnlimitedGameState.h"
#include "Perception/AISenseConfig_Sight.h"
#include "NetworkingPrototype/Managers/SoundManager.h"

AGhostAIController::AGhostAIController()
{
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("Behavior Tree Component"));
	BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("Blackboard Component"));


	/** AI PERCEPTION */
	GhostPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AI Perception Component"));
	//SetPerceptionComponent(*GhostPerceptionComponent);
	GhostSightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	
}

void AGhostAIController::Distract(ACharacter* Distractor) const
{
	// Only works if the ghost is haunting
	if (BlackboardComponent && BlackboardComponent->GetValueAsBool(isHaunting))
	{
		BlackboardComponent->SetValueAsBool(Distracted, true);
		BlackboardComponent->SetValueAsObject(TargetPlayer, Distractor);
		OwnerGhost->Server_PlayAudio(DistractSoundCue.Get());
		BlackboardComponent->SetValueAsBool(Distracted, false);
	}
}

void AGhostAIController::Undistract() const
{
	if(BlackboardComponent)
	{
		BlackboardComponent->SetValueAsBool(Distracted, false);
	}
}

void AGhostAIController::Stun(const float CustomDuration)
{
	if (BlackboardComponent)
	{
		float Duration;
		if (CustomDuration == 0)
		{
			
			Duration = BlackboardComponent->GetValueAsFloat(StunDuration);
		}
		else
		{
			Duration = CustomDuration;
			BlackboardComponent->SetValueAsFloat(StunDuration, CustomDuration);
		}

		OwnerGhost->Server_PlayAudio(StunSoundCue.Get());
		BlackboardComponent->SetValueAsBool(Stunned, true);
		// Set timer to reset Stun
		FTimerHandle StunTimeHandle;
		GetWorldTimerManager().SetTimer(
			StunTimeHandle,
			this,
			&AGhostAIController::StunReset,
			Duration,
			false
		);
	}
}

void AGhostAIController::StunReset() const
{
	if (BlackboardComponent)
	{
		BlackboardComponent->SetValueAsBool(Stunned, false);
	}
}

void AGhostAIController::ProcessSoundEvent(const FSoundEvent& SoundEvent)
{
	UE_LOG(LogTemp, Log, TEXT("AI heard a sound: Type=%s, Intensity=%f, Location=%s"),
	*SoundEvent.SoundType, SoundEvent.Intensity, *SoundEvent.Location.ToString());
	
	// Add custom logic for AI to react to sound:
	
	// If they are speaking and VERY loudly
	if ((SoundEvent.SoundType == "Voice") && (SoundEvent.Intensity > 0.4f))
	{
		// Example: Make AI look at the sound
		APawn* ControlledPawn = GetPawn();
		if (ControlledPawn)
		{
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(ControlledPawn->GetActorLocation(), SoundEvent.Location);
			ControlledPawn->SetActorRotation(LookAtRotation);

			// DISABLED FOR NOW need to playtest Intensity variance and figure out a way
			// to consistently define a "loud" sound no matter what mic/gain and in build
			// // If this noise was created by a character, chase them
			// if (SoundEvent.Character)
			// {
			// 	Distract(SoundEvent.Character, 3.0f);
			// }
		}
	}
	// If they are sprinting
	// else if ((SoundEvent.SoundType == "Footstep") && (SoundEvent.Intensity >= 15.0f))
	// {
	// 	// Example: Make AI look at the sound
	// 	APawn* ControlledPawn = GetPawn();
	// 	if (ControlledPawn)
	// 	{
	// 		FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(ControlledPawn->GetActorLocation(), SoundEvent.Location);
	// 		ControlledPawn->SetActorRotation(LookAtRotation);
	// 	}
	//
	// 	// If this noise was created by a character, chase them
	// 	if (SoundEvent.Character)
	// 	{
	// 		Distract(SoundEvent.Character, 3.0f);
	// 	}
	// }
}

bool AGhostAIController::IsActorSeen(const AActor* Actor) const
{
	const FActorPerceptionInfo* ActorPerceptionInfo = GhostPerceptionComponent->GetActorInfo(*Actor);
	if (ActorPerceptionInfo)
	{
		return true;
	}
	return false;
}

void AGhostAIController::StartHauntTimer(const float Duration)
{
	// Manifest Ghost
	OwnerGhost->Manifest();
	
	// Start Timer for Haunting
	GetWorld()->GetTimerManager().SetTimer(
		HauntingTimerHandle,
		this,
		&AGhostAIController::EndHauntTimer,
		Duration,
		false);

	// Get Current Time
	
}

void AGhostAIController::EndHauntTimer()
{
	if (GetBlackboardComponent()->GetValueAsBool(isHaunting))
	{
		GetBlackboardComponent()->SetValueAsBool(isHaunting, false);
		GetBlackboardComponent()->SetValueAsObject(TargetPlayer, nullptr);
		GetWorld()->GetTimerManager().ClearTimer(HauntingTimerHandle);

		// Set TargetedPlayer to null
		GetBlackboardComponent()->SetValueAsObject(TargetPlayer, nullptr);
	
		// Play Audio Cue
		PlayCalmingSound();
	
		OwnerGhost->TurnInvisible();
		// Set the CurrentGhostState to Patrolling
		OwnerGhost->SetGhostState(E_GhostState::Patrolling);

		GetBlackboardComponent()->SetValueAsBool(CanTeleport, true);
	}
}

void AGhostAIController::ActivateGhost()
{
	BlackboardComponent->SetValueAsBool(GhostActive, true);
}

FVector AGhostAIController::GetRoomLocationFromDG() const
{
	if (DungeonGenerator)
	{
		return DungeonGenerator->GetRandomLocation();
	}
	return FVector::ZeroVector;
}

void AGhostAIController::SetTargetPlayer(ANetworkingPrototypeCharacter* Player)
{
	GetBlackboardComponent()->SetValueAsObject(TargetPlayer, Player);
}

void AGhostAIController::KillingAPlayer()
{	
	OwnerGhost->Multicast_PlayKillAnim(this);
}

void AGhostAIController::PostKillAnim(UAnimMontage* Montage, bool bInterrupted)
{
	if (GetBlackboardComponent()->GetValueAsObject(TargetPlayer))
	{
		ANetworkingPrototypeCharacter* TargetCharacter =
			Cast<ANetworkingPrototypeCharacter>(GetBlackboardComponent()->GetValueAsObject(TargetPlayer));
		
		if (TargetCharacter)
		{
			TargetCharacter->KillPlayer();
			
			SetTargetPlayer(nullptr);
			GetBlackboardComponent()->SetValueAsBool(KillingPlayer, false);

			EndHauntTimer();
		}
	}
}

void AGhostAIController::BeginPlay()
{
	Super::BeginPlay();

	if(IsValid(BehaviorTree.Get()))
	{
		RunBehaviorTree(BehaviorTree.Get());
		BehaviorTreeComponent->StartTree(*BehaviorTree.Get());
	}

	/** Sets up perception system*/
	SetupPerceptionSystem();
	
	// Setting up Dynamics for AI
	GhostPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AGhostAIController::SenseSight);
	GhostPerceptionComponent->OnPerceptionUpdated.AddUniqueDynamic(this, &AGhostAIController::OnPerceptionUpdated);
	
	if (GhostPerceptionComponent)
	{
		// Check if perception is registered
		if (UAIPerceptionSystem::GetCurrent(GetWorld()))
		{
			UE_LOG(LogTemp, Warning, TEXT("✅ AI Perception System Exists!"));
			// Force registers component with AI Perception if not registered
			UAIPerceptionSystem::GetCurrent(GetWorld())->UpdateListener(*GhostPerceptionComponent);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("❌ AI Perception System is MISSING!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ AI Perception Component is NULL!"));
	}
}

void AGhostAIController::Tick(float deltaTime)
{
	Super::Tick(deltaTime);
}

void AGhostAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Make sure OwnerGhost is correctly set //
	OwnerGhost = Cast<AGhost>(InPawn);
	OwnerGhost->SetGhostAIController(this);

	if (IsValid(Blackboard.Get()) && IsValid(BehaviorTree.Get()))
	{
		Blackboard->InitializeBlackboard(*BehaviorTree.Get()->BlackboardAsset.Get());

		// Set Defaults
		Blackboard->SetValueAsFloat(AggroTimerName, AggroTimer);
		Blackboard->SetValueAsFloat(AggroRate, AggroIncreaseRate);
		Blackboard->SetValueAsFloat(AggroRateMultiplier, AggroMultiplier);
		Blackboard->SetValueAsFloat(HauntDurationName, HauntDuration);
	}

	// Try to find a Dungeon Generator
	if (AActor* DungeonGenActor = UGameplayStatics::GetActorOfClass(GetWorld(), ADungeonGenerator::StaticClass()))
	{
		DungeonGenerator = Cast<ADungeonGenerator>(DungeonGenActor);

		if (DungeonGenerator)
		{
			UE_LOG(LogTemp, Warning, TEXT("Dungeon Generator Found By GHOST!!"));
		}
	}

	// Set Ghost's GhostAIController
	//OwnerGhost->SetGhostAIController(this);

	// Start timer to register self to Sound Manager 
	GetWorld()->GetTimerManager().SetTimer(SoundManagerTimerHandle, this,
	&AGhostAIController::RegisterSelfToSoundManager,1.0f, false);
}

void AGhostAIController::SetupPerceptionSystem() const
{
	
	// Setting up Sight Sense
	if (GhostSightConfig)
	{
		
		// Setting up Sight configs
		GhostSightConfig->SightRadius = 1500.0f;
		GhostSightConfig->LoseSightRadius = GhostSightConfig->SightRadius + 50.f;
		GhostSightConfig->PeripheralVisionAngleDegrees = 80.0f;
		GhostSightConfig->SetMaxAge(5.0f);
		GhostSightConfig->AutoSuccessRangeFromLastSeenLocation = 20.0f;
		GhostSightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		GhostSightConfig->DetectionByAffiliation.bDetectEnemies = true;
		GhostSightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		
		//GhostPerceptionComponent->OnPerceptionUpdated.AddUniqueDynamic(this, &AGhostAIController::OnPerceptionUpdated);

		// Set Sight Config
		GhostPerceptionComponent->SetDominantSense(*GhostSightConfig->GetSenseImplementation());
		GhostPerceptionComponent->ConfigureSense(*GhostSightConfig);	
	}
}

void AGhostAIController::SenseSight(AActor* Actor, const FAIStimulus Stimulus)
{
	//UE_LOG(LogTemp, Display, TEXT("Sense Sight Triggered %s"), *Actor->GetName());
	//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, FString::Printf(TEXT("SenseSight Triggered")));
	if (auto* const Player = Cast<ANetworkingPrototypeCharacter>(Actor))
	{
		SeesPlayer = Stimulus.WasSuccessfullySensed();
		if (SeesPlayer && Stimulus.IsActive())
		{
			GetBlackboardComponent()->SetValueAsBool(CanSeePlayer, SeesPlayer);
			GetBlackboardComponent()->SetValueAsObject(TargetPlayer, Player);
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, FString::Printf(TEXT("I SEE YOU!! ")));
		}
		else
		{
			GetBlackboardComponent()->SetValueAsBool(CanSeePlayer, false);
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, FString::Printf(TEXT("Target Lost!")));
		}
	}
}

void AGhostAIController::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	for (AActor* Actor : UpdatedActors)
	{
		if (Actor)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI detected: %s"), *Actor->GetName());
		}
	}
}

void AGhostAIController::RegisterSelfToSoundManager()
{
	// Get our Game State to retrieve the stored SoundManager and register ourselves
	// as one of it's stored AIActors
	if (AQueriesUnlimitedGameState* QUGameState = GetWorld()->GetGameState<AQueriesUnlimitedGameState>())
	{
		QUGameState->SoundManager->RegisterAIActors(this);
	}
}

void AGhostAIController::PlayCalmingSound() const
{
	if (CalmingSoundCue != nullptr)
	{
		OwnerGhost->Server_PlayAudio(CalmingSoundCue.Get());
	}
}

