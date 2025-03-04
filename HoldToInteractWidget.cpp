// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkingPrototype/Widgets/HoldToInteractWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"


void UHoldToInteractWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (InteractableActor && ProgressBar && CurrentNumInteractorsText)
	{
		// Ensure the InteractableActor implements IInteractableInterface
		if (bIsActorInteractable)
		{
			// Use the Execute_ functions to call the interface methods
			float CurrentHeldTime = IInteractableInterface::Execute_GetCurrentHeldTime(InteractableActor);
			float TotalHoldTime = IInteractableInterface::Execute_GetTotalHoldTime(InteractableActor);
			float NumCurrentInteractingActors =  IInteractableInterface::Execute_GetCurrentNumInteractors(InteractableActor);

			if (TotalHoldTime > 0) // Avoid division by zero
			{
				ProgressBar->SetPercent(CurrentHeldTime / TotalHoldTime);
			}

			// Convert the NumCurrentInteractingActors float into an int, then into an FString
			FString InteractorsString = FString::Printf(TEXT("%d"), static_cast<int32>(NumCurrentInteractingActors));
			
			// Prepend "x" before the number
			FString DisplayText = "x" + InteractorsString;
			
			// Then into an FText, to then set as the text
			CurrentNumInteractorsText->SetText(FText::FromString(DisplayText));
		}
	}
}

void UHoldToInteractWidget::UpdateInteractableActor(AActor* NewActor)
{
	InteractableActor = NewActor;

	if (NewActor)
	{
		if (InteractableActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
		{
			bIsActorInteractable = true;
		}
		else
		{
			bIsActorInteractable = false;
		}
	}
}

AActor* UHoldToInteractWidget::GetInteractableActor()
{
	return InteractableActor;
}
