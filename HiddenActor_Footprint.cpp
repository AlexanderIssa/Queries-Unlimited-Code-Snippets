// Fill out your copyright notice in the Description page of Project Settings.


#include "HiddenActor_Footprint.h"
#include "NetworkingPrototype/AnimNotifies/FootprintSpawnNotify.h"

void AHiddenActor_Footprint::BeginPlay()
{
	Super::BeginPlay();

	// Setup auto destroy timer here
	
}

AHiddenActor_Footprint::AHiddenActor_Footprint()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Create the root Component
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Create the box collider component
	BoxColliderComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollider"));
	BoxColliderComp->SetupAttachment(Root);

	// Create the box collider component
	FootprintDecalComp = CreateDefaultSubobject<UDecalComponent>(TEXT("FootprintDecal"));
	FootprintDecalComp->SetupAttachment(Root);
	
}

void AHiddenActor_Footprint::ChangeFootprintColor(int PlayerIdx, E_FootEmum FootSelection) const
{
	if (!FootprintDecalComp || !RightFootDecalMat || !LeftFootDecalMat)
	{
		return;
	}

	constexpr FLinearColor Red = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	constexpr FLinearColor Green = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	constexpr FLinearColor Blue = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	constexpr FLinearColor Pink = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
	
	switch (FootSelection)
	{
	case E_FootEmum::LeftFoot:
		FootprintDecalComp->SetDecalMaterial(LeftFootDecalMat);
		break;

	case E_FootEmum::RightFoot:
		FootprintDecalComp->SetDecalMaterial(RightFootDecalMat);
		break;
	}

	switch (PlayerIdx)
	{
	case 0:
		FootprintDecalComp->SetDecalColor(Red);
		break;
		
	case 1:
		FootprintDecalComp->SetDecalColor(Green);
		break;

	case 2:
		FootprintDecalComp->SetDecalColor(Blue);
		break;

	case 3:
		FootprintDecalComp->SetDecalColor(Pink);
		break;
	}
}
