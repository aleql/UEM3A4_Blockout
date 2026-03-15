// SlideAndHideTrigger.h
// One-shot trigger: slides a target actor along the X axis over a duration.
// Fires once, then permanently disables itself (same pattern as RaiseCylinderTrigger).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "SlideAndHideTrigger.generated.h"

UENUM(BlueprintType)
enum class ESlideDirection : uint8
{
	PositiveX UMETA(DisplayName = "Positive X  (+X)"),
	NegativeX UMETA(DisplayName = "Negative X  (-X)")
};

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom),
	meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API ASlideAndHideTrigger : public AActor
{
	GENERATED_BODY()

public:
	ASlideAndHideTrigger();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Components ────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger|Components",
		meta = (AllowPrivateAccess = "true"))
	USceneComponent* TriggerRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger|Components",
		meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerZone;

	// ── Designer settings ─────────────────────────────────────────────────────

	/** The actor to slide. Assign a level instance in the Details panel. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Trigger|Settings")
	AActor* TargetActor = nullptr;

	/** Distance in centimeters the target travels along X. Always positive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings",
		meta = (ClampMin = "1.0", DisplayName = "Move Distance"))
	float MoveDistance = 300.f;

	/** Seconds the full slide takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings",
		meta = (ClampMin = "0.1"))
	float MoveDuration = 1.5f;

	/**
	 * PositiveX → target moves +X by MoveDistance.
	 * NegativeX → target moves -X by MoveDistance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings")
	ESlideDirection MoveDirection = ESlideDirection::PositiveX;

	/**
	 * Optional ease curve (0..1 in, 0..1 out).
	 * Leave null for auto-generated linear interpolation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Settings")
	UCurveFloat* AlphaCurve = nullptr;

	/** Print verbose logs to the Output Log during the slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Debug")
	bool bDebugLog = false;

	// ── Runtime state ─────────────────────────────────────────────────────────

	FVector StartLocation  = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;

	bool bIsMoving     = false;
	bool bHasTriggered = false;

	// ── Timeline ──────────────────────────────────────────────────────────────

	FTimeline MoveTimeline;

	// ── Internal helpers ──────────────────────────────────────────────────────

	UCurveFloat* BuildLinearCurve();

	// ── Callbacks ─────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnTriggerOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor*              OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                OtherBodyIndex,
		bool                 bFromSweep,
		const FHitResult&    SweepResult);

	UFUNCTION()
	void OnTimelineUpdate(float Alpha);

	UFUNCTION()
	void OnTimelineFinished();
};
