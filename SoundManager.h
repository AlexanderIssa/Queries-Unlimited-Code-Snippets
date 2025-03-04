// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundManager.generated.h"

// Struct to store sound data
USTRUCT(BlueprintType)
struct FSoundEvent
{
	GENERATED_BODY()

	// The character who is responsible for creating this noise
	UPROPERTY(BlueprintReadWrite)
	ACharacter* Character;
	
	UPROPERTY(BlueprintReadWrite)
	FVector Location;

	UPROPERTY(BlueprintReadWrite)
	float Intensity;

	UPROPERTY(BlueprintReadWrite)
	FString SoundType;

	// Default constructor
	FSoundEvent()
		: Character(nullptr), Location(FVector::ZeroVector), Intensity(0.0f), SoundType(TEXT("None")) {}
};

UCLASS()
class NETWORKINGPROTOTYPE_API ASoundManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASoundManager();

	// Add a sound event to be processed
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void RegisterSoundEvent(const FSoundEvent& SoundEvent);

	// Notify AI of sound events
	void NotifyAI();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	// List of AI to notify
	TArray<AActor*> RegisteredAIActors;

	// Array to store sound events
	TArray<FSoundEvent> SoundEvents;

	// Register AI actors
	void RegisterAIActors();

};
