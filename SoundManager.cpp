// Fill out your copyright notice in the Description page of Project Settings.


#include "SoundManager.h"

#include "Kismet/GameplayStatics.h"
#include "NetworkingPrototype/Ghost/GhostAIController.h"

// Sets default values
ASoundManager::ASoundManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

void ASoundManager::RegisterSoundEvent(const FSoundEvent& SoundEvent)
{
	if (!HasAuthority())
	{
		return;
	}

	// Store the sound event passed in
	SoundEvents.Add(SoundEvent);

	// Notify AI about the new sound
	NotifyAI();
}

void ASoundManager::NotifyAI()
{
	for (AActor* AIActor : RegisteredAIActors)
	{
		if (AIActor)
		{
			AGhostAIController* GhostAIController = Cast<AGhostAIController>(AIActor);
			if (GhostAIController)
			{
				for (const FSoundEvent& Event : SoundEvents)
				{
					GhostAIController->ProcessSoundEvent(Event);
				}
			}
		}
	}

	// Clear sound events after processing
	SoundEvents.Empty();
}

// Called when the game starts or when spawned
void ASoundManager::BeginPlay()
{
	Super::BeginPlay();

	// Server only
	if (HasAuthority())
	{
		RegisterAIActors();
	}
}

// Called every frame
void ASoundManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASoundManager::RegisterAIActors()
{
	// Find all Ghost AI actors in the world
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGhostAIController::StaticClass(), RegisteredAIActors);
}

