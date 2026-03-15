// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupActor.generated.h"

class USceneComponent;
class USphereComponent;
class UPickupNotificationWidget;

UCLASS()
class UEM3A2BLOCKOUT_API APickupActor : public AActor
{
	GENERATED_BODY()

public:
	APickupActor();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	void Collect(AActor* Collector);
	void HideNotification();

	// --- Components ---

	UPROPERTY(VisibleAnywhere, Category = "Pickup|Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category = "Pickup|Components")
	USphereComponent* OverlapSphere;

	// --- Configuration ---

	/** The actor in the scene that represents this pickup visually. It will be hidden on collection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	AActor* TargetActor;

	/** Display name shown in the pickup notification. Example: "Matcha Powder" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	FText ItemName;

	/** Notification message format. {0} is replaced by ItemName. Example: "{0} acquired", "{0} collected", "You picked up {0}" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	FText NotificationFormat;

	/** How long (seconds) the notification stays on screen before hiding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.5", ClampMax = "10.0"))
	float NotificationDuration;

	/** Widget Blueprint class to spawn as the pickup notification. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UPickupNotificationWidget> NotificationWidgetClass;

	/** If true, the actor destroys itself after being collected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	bool bDestroyOnPickup;

	// --- Runtime state ---

	bool bAlreadyCollected;
	UPickupNotificationWidget* ActiveWidget;
	FTimerHandle NotificationTimerHandle;
};
