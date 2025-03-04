// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NetworkingPrototype/NetworkingPrototypeCharacter.h"
#include "NetworkingPrototype/Interfaces/ItemInterface.h"
#include "Item.generated.h"

class ANetworkingPrototypeGameMode;
class APickup;

DECLARE_DELEGATE_OneParam(EndUseItemDelegate, ANetworkingPrototypeCharacter*);
DECLARE_DELEGATE_OneParam(OnPickupDelegate, ANetworkingPrototypeCharacter*);
DECLARE_DELEGATE(AltUseDelegate);

UCLASS()
class NETWORKINGPROTOTYPE_API AItem : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AItem();
	AItem(const int& id, const FName& name, const FText& description);;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// =========== Properties ===========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	int ID;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	FText Description;

	// Pointer to the GameMode, to determine which Pickup is connected to which Item
	ANetworkingPrototypeGameMode* GameMode = nullptr;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void UseItem(AActor* PlayerUser);

	UFUNCTION(BlueprintCallable)
	void DropItem(AActor* PlayerUser, bool Respawn);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(AActor* PlayerUser, bool Respawn);

	UFUNCTION(BlueprintCallable)
	virtual E_ClickType GetClickType();

	UFUNCTION(BlueprintCallable)
	int GetItemID();

	UFUNCTION(BlueprintCallable)
	FName GetItemName();
	
	EndUseItemDelegate OnEndUseItem;
	OnPickupDelegate OnPickup;
	AltUseDelegate OnAltUse;

private:

	UPROPERTY(Replicated)
	TSubclassOf<APickup> ItemToDrop = nullptr;
	
};
