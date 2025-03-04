// Fill out your copyright notice in the Description page of Project Settings.

#include "Item.h"
#include "NetworkingPrototype/NetworkingPrototypeGameMode.h"
#include "Pickup.h"
#include "Net/UnrealNetwork.h"
#include "Windows/WindowsApplication.h"

// Sets default values
AItem::AItem() : ID(0), Name(TEXT("Item")), Description(FText::FromString("An item"))
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Replicate all items
	SetReplicates(true);
}

AItem::AItem(const int& id, const FName& name, const FText& description) : ID(id), Name(name), Description(description)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Replicate all items
	SetReplicates(true);
}

// Called when the game starts or when spawned
void AItem::BeginPlay()
{
	Super::BeginPlay();
	
	GameMode = Cast<ANetworkingPrototypeGameMode>(GetWorld()->GetAuthGameMode());
	
}

// Called every frame
void AItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AItem::UseItem(AActor* PlayerUser)
{
}

void AItem::DropItem(AActor* PlayerUser, bool Respawn)
{
	// Spawn Dropped item in world
	if (HasAuthority())
	{
		if (GameMode == NULL)
		{
			return;
		}
		ItemToDrop = GameMode->GetPickupofItemID(ID);
		if (ItemToDrop)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				if (Respawn)
				{
					// Specify location and rotation of the new actor to be 10 Units in front of user
					const FVector SpawnLocation = PlayerUser->GetActorLocation() + (PlayerUser->GetActorForwardVector() * 50);
					const FRotator SpawnRotation = PlayerUser->GetActorRotation();

					// Spawn parameters
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

					// Spawn Item to be dropped
					APickup* DroppedItem = World->SpawnActor<APickup>(ItemToDrop, SpawnLocation, SpawnRotation, SpawnParams);
				}
				Destroy();
			}
		}
		ItemToDrop = nullptr;
	}
	else
	{
		Server_DropItem_Implementation(PlayerUser, Respawn);
	}
}

void AItem::Server_DropItem_Implementation(AActor* PlayerUser, bool Respawn)
{
	// For some reason even though the owner doesn't have authority they still run this (Makes no fuckin sense to me)
	if (HasAuthority())
		DropItem(PlayerUser, Respawn);
}

E_ClickType AItem::GetClickType()
{
	return E_ClickType::Tap;
}

int AItem::GetItemID()
{
	return ID;
}

FName AItem::GetItemName()
{
	return Name;
}

void AItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AItem, ItemToDrop);
}

