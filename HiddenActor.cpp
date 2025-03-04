// Fill out your copyright notice in the Description page of Project Settings.


#include "HiddenActor.h"

// Sets default values
AHiddenActor::AHiddenActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AHiddenActor::BeginPlay()
{
	Super::BeginPlay();

	// Set the actor to be hidden in game by default
	SetActorHiddenInGame(true);
}

// Called every frame
void AHiddenActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

