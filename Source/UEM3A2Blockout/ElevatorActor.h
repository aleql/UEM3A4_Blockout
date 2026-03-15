// ElevatorActor.h
// Pure C++ elevator actor for UEM3A2Blockout.
// Moves a platform between two points using a FTimeline driven by a UCurveFloat.
// Triggered by a BoxComponent overlap, guarded by a cooldown, toggles up / down.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "ElevatorActor.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API AElevatorActor : public AActor
{
    GENERATED_BODY()

public:

    // ─── Construction ─────────────────────────────────────────────────────────
    AElevatorActor();

protected:

    // ─── AActor overrides ─────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ─── Components ───────────────────────────────────────────────────────────

    /** Invisible scene root – keep at origin so relative-space math is clean. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elevator|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* ElevatorRoot;

    /** The visible moving platform. Assign a Static Mesh in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elevator|Components",
        meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* PlatformMesh;

    /** Box trigger that the player walks into to activate the elevator. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elevator|Components",
        meta = (AllowPrivateAccess = "true"))
    UBoxComponent* TriggerZone;

    /** Optional decorative button mesh – no logic attached. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elevator|Components",
        meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* ButtonMesh;

    // ─── Designer-exposed settings ─────────────────────────────────────────────

    /** How far (Z) the platform travels upward from its start position (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Settings",
        meta = (ClampMin = "1.0", UIMin = "1.0"))
    float MoveHeight = 300.f;

    /** Time in seconds the full one-way trip takes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Settings",
        meta = (ClampMin = "0.1", UIMin = "0.1"))
    float MoveDuration = 1.5f;

    /** Seconds to wait after a trip finishes before the trigger can fire again. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Settings",
        meta = (ClampMin = "0.0", UIMin = "0.0"))
    float CooldownSeconds = 1.0f;

    /** When true the platform starts rising automatically on BeginPlay,
     *  without requiring a player overlap. The trigger still works normally
     *  for subsequent trips once the cooldown expires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Settings")
    bool bAutoStartOnBeginPlay = false;

    /**
     * When true the platform stores and moves in relative (component) space.
     * This is correct when the owning level or a parent actor can rotate;
     * the platform will rotate with it rather than drifting in world space.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Settings")
    bool bUseRelativeSpace = true;

    /**
     * Attach the player character to the platform while it is moving so
     * they ride it without manual physics push.
     * WARNING: attachment changes the character's movement mode – set to false
     * if you are using a custom movement component that doesn't tolerate it.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Settings")
    bool bAttachCharacterWhileMoving = false;

    /**
     * Optional ease curve (0..1 input, 0..1 output).
     * Leave null for a linear interpolation.
     * Assign a UCurveFloat asset in the editor for smooth ease-in / ease-out.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Settings")
    UCurveFloat* AlphaCurve;

    /** Print verbose logs to Output Log while the elevator is active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator|Debug")
    bool bDebugLog = false;

    // ─── Runtime state ─────────────────────────────────────────────────────────

    /** The rest (bottom) position cached on BeginPlay. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Elevator|State")
    FVector StartLocation;

    /** The raised (top) position computed on BeginPlay. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Elevator|State")
    FVector TargetLocation;

    /** True while the platform is at (or heading back from) the top. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Elevator|State")
    bool bAtTop = false;

    /** False while a trip or cooldown is in progress. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Elevator|State")
    bool bCanTrigger = true;

    // ─── Timeline ──────────────────────────────────────────────────────────────

    /** The C++ Timeline that drives the interpolation. Ticked via Tick(). */
    FTimeline MoveTimeline;

    // ─── Timers ────────────────────────────────────────────────────────────────

    FTimerHandle CooldownTimerHandle;

    // ─── Attached character (optional ride) ────────────────────────────────────

    UPROPERTY()
    ACharacter* AttachedCharacter = nullptr;

    // ─── Internal helpers ──────────────────────────────────────────────────────

    /** Builds a linear 0→1 UCurveFloat at runtime when AlphaCurve is null. */
    UCurveFloat* BuildLinearCurve();

    /** Sets the platform to a given alpha position (0 = bottom, 1 = top). */
    void ApplyAlpha(float Alpha);

    // ─── Callbacks ─────────────────────────────────────────────────────────────

    /** Called every timeline tick with alpha in [0..1]. */
    UFUNCTION()
    void OnTimelineUpdate(float Alpha);

    /** Called when the timeline finishes. */
    UFUNCTION()
    void OnTimelineFinished();

    /** Re-enables the trigger after the cooldown expires. */
    UFUNCTION()
    void ResetTrigger();

    /** BoxComponent overlap – entry point for the activation logic. */
    UFUNCTION()
    void OnTriggerOverlapBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult&    SweepResult);

public:

    /**
     * Remote toggle entry point.
     * Called by AElevatorRemoteTrigger (or any other external actor) to toggle
     * the elevator direction.  Respects the same cooldown and moving guards as
     * the built-in overlap trigger.  Pass the instigating character so
     * bAttachCharacterWhileMoving still works from a remote call.
     * Passing nullptr for InstigatorCharacter skips attachment.
     */
    UFUNCTION(BlueprintCallable, Category = "Elevator")
    void RequestToggle(ACharacter* InstigatorCharacter = nullptr);
};
