// OneWayDoubleDoor.h
// One-way, one-shot double door – Option A: external StaticMeshActors.
//
// The trigger actor itself owns no door mesh. Instead the designer places two
// AStaticMeshActor leaves in the level and assigns them via LeftDoorActor /
// RightDoorActor in the Details panel.
//
// OPTION A ASSUMPTION:
//   Each door leaf actor must have its PIVOT (origin) at its hinge edge.
//   Rotation is applied directly to the actor via SetActorRotation.
//   If the pivot is centered the swing will look incorrect – no hinge
//   correction is provided in this option. Use a modelling tool to move the
//   pivot to the hinge side before importing, or set the actor's origin
//   to the hinge position via "Set Actor Pivot Offset" in the level editor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMeshActor.h"
#include "OneWayDoubleDoor.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom),
    meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API AOneWayDoubleDoor : public AActor
{
    GENERATED_BODY()

public:

    AOneWayDoubleDoor();

protected:

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ─── Components (kept on the trigger actor itself) ────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* DoorRoot;

    /** Activation volume. Resize and reposition in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components",
        meta = (AllowPrivateAccess = "true"))
    UBoxComponent* TriggerZone;

    /**
     * Editor-only arrow showing the ALLOWED approach direction.
     * Rotate this component so it points toward the side the player
     * must come from.  At runtime its forward vector becomes
     * ResolvedAllowedDirection (unless AllowedDirection is overridden).
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components",
        meta = (AllowPrivateAccess = "true"))
    UArrowComponent* DirectionArrow;

    // ─── External door actor references ──────────────────────────────────────

    /**
     * The left door leaf placed in the level.
     * REQUIREMENT: The actor's world pivot must be at its hinge edge.
     * Pick the instance from the level using the eyedropper in the Details panel.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Door|Actors")
    AStaticMeshActor* LeftDoorActor = nullptr;

    /**
     * The right door leaf placed in the level.
     * REQUIREMENT: The actor's world pivot must be at its hinge edge.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Door|Actors")
    AStaticMeshActor* RightDoorActor = nullptr;

    // ─── Settings ─────────────────────────────────────────────────────────────

    /**
     * World-space unit vector defining the "allowed" approach side.
     * Leave at zero (default) to automatically use the DirectionArrow forward.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Settings")
    FVector AllowedDirection = FVector::ZeroVector;

    /**
     * Minimum dot product between AllowedDirection and the player-to-door
     * vector required to accept the overlap.
     * 0.0 = full hemisphere.  0.2 = default.  1.0 = exactly head-on.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Settings",
        meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float SideDotThreshold = 0.2f;

    /** Yaw degrees each leaf swings. Left = +value, Right = -value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Settings",
        meta = (ClampMin = "1.0", ClampMax = "180.0"))
    float OpenAngleDegrees = 90.f;

    /** Duration of the full open animation in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Settings",
        meta = (ClampMin = "0.1"))
    float OpenDuration = 1.0f;

    /**
     * Optional ease curve (0..1 in, 0..1 out).
     * Leave null for a runtime-generated linear interpolation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Settings")
    UCurveFloat* OpenCurve = nullptr;

    /** Verbose logging to Output Log. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Debug")
    bool bDebugLog = false;

    // ─── Runtime state ─────────────────────────────────────────────────────────

    /** Latched true on first successful trigger. Never resets. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|State")
    bool bHasOpened = false;

    /**
     * Set false in BeginPlay if either door actor reference is null or
     * their mesh components are not Movable.  Blocks the overlap from firing.
     */
    bool bActorsValid = false;

    /** World rotation of LeftDoorActor at BeginPlay (closed state). */
    FRotator LeftStartRotation;

    /** World rotation of RightDoorActor at BeginPlay (closed state). */
    FRotator RightStartRotation;

    /** Resolved direction used for the side check (built in BeginPlay). */
    FVector ResolvedAllowedDirection;

    // ─── Timeline ──────────────────────────────────────────────────────────────

    FTimeline OpenTimeline;

    // ─── Internal helpers ──────────────────────────────────────────────────────

    UCurveFloat* BuildLinearCurve();

    // ─── Callbacks ─────────────────────────────────────────────────────────────

    UFUNCTION()
    void OnTimelineUpdate(float Alpha);

    UFUNCTION()
    void OnTimelineFinished();

    UFUNCTION()
    void OnTriggerOverlapBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult&    SweepResult);
};
