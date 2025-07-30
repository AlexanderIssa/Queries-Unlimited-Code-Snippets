// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "GhostBTTask_FollowPlayerWithNav.generated.h"

/**
 * 
 */
UCLASS()
class NETWORKINGPROTOTYPE_API UGhostBTTask_FollowPlayerWithNav : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	/** Constructor */
	explicit UGhostBTTask_FollowPlayerWithNav(const FObjectInitializer& ObjectInitializer);
	/** Execute Node */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:

	// Simplified method to be able to reach Blackboard variables
	FName TargetPlayer = TEXT("TargettedPlayer");
	FName KillingPlayer = TEXT("KillingPlayer");
	FName CanSeePlayer = TEXT("CanSeePlayer");
	FName TargetLocation = TEXT("TargetLocation");

	UPROPERTY(EditAnywhere)
	FBlackboardKeySelector SelfActor;

	UPROPERTY(EditAnywhere)
	float MinimumTeleportDistance = 50;

	UPROPERTY(EditAnywhere)
	float KillCameraZOffset = 0;
	
	/**
	 * @description: Makes sure the player is still in sight,
	 * once it is not, updates last known location and stops execution of the Move.
	 * @param OwnerComp 
	 * @return True if target player is still detected by AI Controller through Sight, otherwise false
	 */
	void UpdateLastKnownLocation(UBehaviorTreeComponent& OwnerComp) const;

	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
};
