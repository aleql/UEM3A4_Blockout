// Copyright Epic Games, Inc. All Rights Reserved.

#include "PickupActor.h"
#include "PickupNotificationWidget.h"

#include "Components/SphereComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UEM3A2Blockout.h"

APickupActor::APickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapSphere"));
	OverlapSphere->SetupAttachment(Root);
	OverlapSphere->SetSphereRadius(100.f);
	OverlapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OverlapSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	OverlapSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	OverlapSphere->SetGenerateOverlapEvents(true);

	TargetActor = nullptr;
	ItemName = FText::FromString(TEXT("Item"));
	NotificationFormat = FText::FromString(TEXT("{0} acquired"));
	NotificationDuration = 2.5f;
	bDestroyOnPickup = true;
	bAlreadyCollected = false;
	ActiveWidget = nullptr;
}

void APickupActor::BeginPlay()
{
	Super::BeginPlay();
	OverlapSphere->OnComponentBeginOverlap.AddDynamic(this, &APickupActor::OnOverlapBegin);
}

void APickupActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bAlreadyCollected)
	{
		return;
	}

	if (!OtherActor || !OtherActor->IsA<ACharacter>())
	{
		return;
	}

	// Only react to the locally controlled player
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !Character->IsPlayerControlled())
	{
		return;
	}

	Collect(OtherActor);
}

void APickupActor::Collect(AActor* Collector)
{
	bAlreadyCollected = true;

	// Disable collision immediately to prevent duplicate triggers
	OverlapSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverlapSphere->SetGenerateOverlapEvents(false);

	// Hide the target actor in the scene
	if (TargetActor)
	{
		TargetActor->SetActorHiddenInGame(true);
		TargetActor->SetActorEnableCollision(false);
	}

	// Build and show the notification widget
	if (NotificationWidgetClass)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			ActiveWidget = CreateWidget<UPickupNotificationWidget>(PC, NotificationWidgetClass);
			if (ActiveWidget)
			{
				FText Message = FText::Format(NotificationFormat, ItemName);
				ActiveWidget->SetPickupText(Message);
				ActiveWidget->AddToViewport();
				ActiveWidget->ShowNotification(true);

				GetWorldTimerManager().SetTimer(
					NotificationTimerHandle,
					this,
					&APickupActor::HideNotification,
					NotificationDuration,
					false
				);
			}
		}
	}
	else
	{
		UE_LOG(LogUEM3A2Blockout, Warning,
			TEXT("APickupActor (%s): NotificationWidgetClass is not set. No notification will appear."),
			*GetName());

		// No widget — destroy immediately if configured to do so
		if (bDestroyOnPickup)
		{
			Destroy();
		}
	}
	// If a widget was spawned, destruction (when configured) happens inside HideNotification
	// so the timer is not cancelled before it fires.
}

void APickupActor::HideNotification()
{
	if (ActiveWidget)
	{
		ActiveWidget->ShowNotification(false);
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}

	if (bDestroyOnPickup)
	{
		Destroy();
	}
}
