// Fill out your copyright notice in the Description page of Project Settings.


#include "GhostBTTask_FollowPlayerWithNav.h"

#include "AIController.h"
#include "GhostAIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UGhostBTTask_FollowPlayerWithNav::UGhostBTTask_FollowPlayerWithNav(const FObjectInitializer& ObjectInitializer)
{
	NodeName = TEXT("Follow Obsession Player");
}

EBTNodeResult::Type UGhostBTTask_FollowPlayerWithNav::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

void UGhostBTTask_FollowPlayerWithNav::UpdateLastKnownLocation(UBehaviorTreeComponent& OwnerComp) const
{
	AActor* TargetPlayerActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetPlayer));
	if (TargetPlayerActor != nullptr)
	{
		const FVector PlayerLastSeenLocation = TargetPlayerActor->GetActorLocation();
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("PlayerLastSeenLocation: ") + PlayerLastSeenLocation.ToString());
		OwnerComp.GetBlackboardComponent()->SetValueAsVector(TargetLocation, PlayerLastSeenLocation);
	}
}

void UGhostBTTask_FollowPlayerWithNav::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	if (TaskResult == EBTNodeResult::Succeeded)
	{
		
		if (ANetworkingPrototypeCharacter* CaughtPlayer = Cast<ANetworkingPrototypeCharacter>(
			OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetPlayer)))
		{
			AGhostAIController* GhostAIController = Cast<AGhostAIController>(OwnerComp.GetOwner());
			APawn* GhostPawn = GhostAIController->GetPawn();

			APlayerController* CaughtPlayerController = Cast<APlayerController>(CaughtPlayer->GetController());
			
			if (GhostAIController && GhostPawn && CaughtPlayerController)
			{
				// Set KillingPlayer to true
				OwnerComp.GetBlackboardComponent()->SetValueAsBool(KillingPlayer, true);

				// Tell the caught player to face towards the ghost
				CaughtPlayer->Client_CameraLookAtLocation_Wrapper(GhostPawn->GetActorLocation(), CaughtPlayerController);

				// ALEX: I WANT TO DO THE REST AFTER THE PLAYER TURNS TO LOOK AT THE GHOST BUT
				// THE CODE DOESNT FINISH IN TIME AND THE GHOST TELEPORTS TO WHEREVER THE
				// PLAYER IS LOOKING (INCLUDING WALLS), PLANNING TO CALL A SERVER RPC TO DO THE
				// FOLLOWING LOGIC IN THE CLIENT RPC AFTER THE ROTATION IS DONE.
				// I ENDED UP PROTOTYPING IT IN BP FOR NOW, WILL CONVERT TO CPP LATER
				
				// // Calculate the vector to tell the player's camera to look at
				// FVector CameraTargetVector(CaughtPlayer->GhostKillLocation->GetComponentLocation().X,
				// 	CaughtPlayer->GhostKillLocation->GetComponentLocation().Y,
				// 	(GhostPawn->GetActorLocation().Z + KillCameraZOffset));
				//
				// // Calculate the vector to teleport the ghost to
				// FVector GhostTargetLocation(CameraTargetVector.X, CameraTargetVector.Y,
				// 	GhostPawn->GetActorLocation().Z);
				// // Calculate the rotator for the ghost to rotate to
				// FRotator GhostTargetRotation(GhostPawn->GetActorRotation().Pitch,
				// CaughtPlayer->GhostKillLocation->GetComponentRotation().Yaw,
				// GhostPawn->GetActorRotation().Roll);
				//
				// // Move the ghost to the proper kill location
				// GhostPawn->SetActorLocation(GhostTargetLocation);
				// // Rotate the ghost to the proper rotation to stare at the killed player's camera
				// GhostPawn->SetActorRotation(GhostTargetRotation);
				//
				// // Tell the caught player to look at the newly positioned ghost
				// CaughtPlayer->Client_CameraLookAtLocation_Wrapper(CameraTargetVector, false);

				// Kill the caught player and End Haunt Timer
				if (AGhostAIController* GhostAI = Cast<AGhostAIController>(OwnerComp.GetAIOwner()))
				{
					GhostAI->KillingAPlayer();
				}
			}
		}
	}
	else
	{
		UpdateLastKnownLocation(OwnerComp);
	}
	
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}