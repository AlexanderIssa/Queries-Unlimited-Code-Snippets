// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CDCaseActor.h"
#include "Item.h"
#include "Boombox.generated.h"


// Declare the log category
DECLARE_LOG_CATEGORY_EXTERN(LogBoombox, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBPBoomBoxButtonPressed, bool, ButtonPressed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBPMusicStateChanged, bool, IsMusicPlaying);

/**
 * 
 */
UCLASS()
class NETWORKINGPROTOTYPE_API ABoombox final: public AItem
{
	GENERATED_BODY()

public:	
	// Sets default values for this actor's properties
	ABoombox();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnCDChosen(const FCDData& CDData);

	UFUNCTION(BlueprintCallable)
	void ToggleCDSelectionMode();

	UFUNCTION(BlueprintCallable)
	void SetShowMouse(bool bShowMouse);
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Implementing the Item Interface
	virtual void UseItem(AActor* user) override;
	void OnAltFireUse();
	void OnPickupItem(ANetworkingPrototypeCharacter* Interactor);
	virtual E_ClickType GetClickType() override { return E_ClickType::Tap; }

	// Replication functions
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// BP event delegate to trigger when the character presses the BoomBox's Play button
	UPROPERTY(BlueprintAssignable, Category = "Blueprint Events")
	FBPBoomBoxButtonPressed OnBoomBoxButtonPressed;

	// BP event delegate to trigger when the BoomBox's music ends
	UPROPERTY(BlueprintAssignable, Category = "Blueprint Events")
	FBPMusicStateChanged OnMusicStateChanged;

	//
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, Category = "CD")
	TSubclassOf<ACDCaseActor> CDCaseClass;

	//
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CDCaseActor, Category = "CD")
	ACDCaseActor* CDCaseActor;
	UFUNCTION()
	void OnRep_CDCaseActor();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void BPPullUpCDCase();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void BPPutDownCDCase();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void BPInsertCDIntoBoombox();

	UFUNCTION(Server, Reliable)
	void Server_SetCurrentCDData(const FCDData& NewCDData);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	
	// Networked Functions:
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_UseItem(AActor* user);

	UFUNCTION(Server, Reliable)
	void Server_UseItem(AActor* user);

	UFUNCTION(Server, Reliable)
	void Server_OnPickupItem(ANetworkingPrototypeCharacter* Interactor);

	UFUNCTION(Server, Reliable)
	void Server_SetCDCaseUp(bool bNewCaseUp);
	
	
	// Member Properties

	// Root component
	UPROPERTY(EditAnywhere)
	USceneComponent* Root;

	// Mesh for Boombox
	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Audio, meta = (AllowPrivateAccess = "true"))
	UAudioComponent* BoomboxAudioComponent;

	// Reference to our owning Character
	// Replicated so the server always knows who is using this item
	UPROPERTY(Replicated)
	ANetworkingPrototypeCharacter* mUserCharacter;

	UPROPERTY(Replicated)
	FCDData mCurrentCDData;

	FTimerHandle UnlimitedSprintTimerHandle;
	FTimerDelegate StopUnlimitedSprintDelegate;

	UFUNCTION(Server, Reliable)
	void EnableUnlimitedSprint(ANetworkingPrototypeCharacter* MyCharacter);
	UFUNCTION(Server, Reliable)
	void DisableUnlimitedSprint(ANetworkingPrototypeCharacter* MyCharacter);

	UPROPERTY(Replicated)
	float UnlimitedSprintDuration = 5.0f;

	UPROPERTY(ReplicatedUsing=OnRep_CDCaseUp)
	bool bCDCaseUp = false;
	UFUNCTION()
	void OnRep_CDCaseUp();
};