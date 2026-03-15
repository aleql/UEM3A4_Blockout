// RaiseCylinderTrigger.h
// One-shot vertical move trigger for UEM3A2Blockout.
// When the player walks into the BoxCollision the assigned TargetActor moves
// up or down (controlled by MoveDirection) over MoveDuration seconds.
// Fires once, then permanently disables itself.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "RaiseCylinderTrigger.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Direction enum – shown as a dropdown in the Details panel.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EVerticalMoveDirection : uint8
{
    Rise    UMETA(DisplayName = "Rise    (+Z)"),
    Descend UMETA(DisplayName = "Descend (-Z)")
};

// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom),
    meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API ARaiseCylinderTrigger : public AActor
{
    GENERATED_BODY()

public:

    ARaiseCylinderTrigger();

protected:

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ── Components ────────────────────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MoveTrigger|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* TriggerRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MoveTrigger|Components",
        meta = (AllowPrivateAccess = "true"))
    UBoxComponent* TriggerZone;

    // ── Designer settings ─────────────────────────────────────────────────────

    /** The actor to move. Assign an instance in the level. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTrigger|Settings")
    AActor* TargetActor = nullptr;

    /** Distance in centimeters the target travels along Z. Always positive. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Settings",
        meta = (ClampMin = "1.0", DisplayName = "Move Distance"))
    float MoveDistance = 300.f;

    /** Seconds the full move takes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Settings",
        meta = (ClampMin = "0.1"))
    float MoveDuration = 1.5f;

    /**
     * Rise  → target moves +Z (up)   by MoveDistance.
     * Descend → target moves -Z (down) by MoveDistance.
     * The sign is applied at trigger time; MoveDistance stays positive.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Settings")
    EVerticalMoveDirection MoveDirection = EVerticalMoveDirection::Rise;

    /**
     * Optional ease curve (0..1 in, 0..1 out).
     * Leave null for auto-generated linear interpolation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Settings")
    UCurveFloat* AlphaCurve = nullptr;

    // ── Rotation after raise ───────────────────────────────────────────────────

    /** If true, the target rotates after the vertical move finishes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Rotation")
    bool bEnableRotationAfterRaise = false;

    /** Degrees to rotate around the X axis (Roll) after the raise completes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Rotation",
        meta = (EditCondition = "bEnableRotationAfterRaise"))
    float RotationDeltaRoll = 0.f;

    /** Degrees to rotate around the Y axis (Pitch) after the raise completes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Rotation",
        meta = (EditCondition = "bEnableRotationAfterRaise"))
    float RotationDeltaPitch = 0.f;

    /** Degrees to rotate around the Z axis (Yaw) after the raise completes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Rotation",
        meta = (EditCondition = "bEnableRotationAfterRaise"))
    float RotationDeltaYaw = 0.f;

    /** Seconds the full rotation takes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Rotation",
        meta = (ClampMin = "0.1", EditCondition = "bEnableRotationAfterRaise"))
    float RotateDuration = 1.0f;

    /**
     * Optional ease curve for the rotation (0..1 in, 0..1 out).
     * Leave null to reuse AlphaCurve (or the auto-generated linear fallback).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Rotation",
        meta = (EditCondition = "bEnableRotationAfterRaise"))
    UCurveFloat* RotationAlphaCurve = nullptr;

    /** Print verbose logs to Output Log during the move. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveTrigger|Debug")
    bool bDebugLog = false;

    // ── Runtime state ─────────────────────────────────────────────────────────

    /** World-space start position of the target, cached on trigger. */
    FVector StartLocation = FVector::ZeroVector;

    /** World-space end position, computed at trigger time from MoveDirection. */
    FVector TargetLocation = FVector::ZeroVector;

    /** True while the timeline is playing – blocks re-entry. */
    bool bIsMoving = false;

    /** Latched after the first successful trigger – never resets. */
    bool bHasTriggered = false;

    /** World-space start rotation of the target, cached when rotation begins. */
    FRotator StartRotation = FRotator::ZeroRotator;

    /** World-space end rotation, computed when rotation begins. */
    FRotator TargetRotation = FRotator::ZeroRotator;

    /** True while the rotation timeline is playing. */
    bool bIsRotating = false;

    // ── Timelines ─────────────────────────────────────────────────────────────

    FTimeline MoveTimeline;
    FTimeline RotateTimeline;

    // ── Internal helpers ──────────────────────────────────────────────────────

    UCurveFloat* BuildLinearCurve();

    // ── Callbacks ─────────────────────────────────────────────────────────────

    UFUNCTION()
    void OnTimelineUpdate(float Alpha);

    UFUNCTION()
    void OnTimelineFinished();

    UFUNCTION()
    void OnRotateTimelineUpdate(float Alpha);

    UFUNCTION()
    void OnRotateTimelineFinished();

    UFUNCTION()
    void OnTriggerOverlapBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult&    SweepResult);
};
