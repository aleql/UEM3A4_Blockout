// CameraShakeTrigger.h
// One-shot trigger: waits TriggerDelay seconds after the player enters,
// shakes the camera for ShakeDuration seconds, then stops.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraShakeBase.h"
#include "CameraShakeTrigger.generated.h"

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom),
	meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API ACameraShakeTrigger : public AActor
{
	GENERATED_BODY()

public:
	ACameraShakeTrigger();

protected:
	virtual void BeginPlay() override;

	// ── Components ────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger|Components",
		meta = (AllowPrivateAccess = "true"))
	USceneComponent* TriggerRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger|Components",
		meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerZone;

	// ── Designer settings ─────────────────────────────────────────────────────

	/** Camera shake Blueprint to play (assign a UCameraShakeBase child in Details). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings")
	TSubclassOf<UCameraShakeBase> ShakeClass;

	/** Seconds to wait after the player enters before the shake starts. 0 = immediate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings",
		meta = (ClampMin = "0.0"))
	float TriggerDelay = 0.0f;

	/** How long (seconds) the shake plays before being stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings",
		meta = (ClampMin = "0.1"))
	float ShakeDuration = 1.0f;

	/** Intensity multiplier passed to the camera shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings",
		meta = (ClampMin = "0.0"))
	float ShakeScale = 1.0f;

	/** Print log lines when the trigger fires, shake starts, and shake stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Debug")
	bool bDebugLog = false;

	// ── Runtime state ─────────────────────────────────────────────────────────

	bool bHasTriggered = false;

	FTimerHandle DelayTimerHandle;
	FTimerHandle StopTimerHandle;

	// ── Callbacks ─────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnTriggerOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor*              OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                OtherBodyIndex,
		bool                 bFromSweep,
		const FHitResult&    SweepResult);

	void StartShake();
	void StopShake();
};
