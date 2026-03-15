// LadderTeleporterActor.h
// Instant ladder teleporter — no movement mode changes, no animation logic.
// Bottom trigger teleports the character to TopTarget.
// Top trigger teleports the character to BottomTarget.
// A per-actor cooldown prevents ping-pong.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LadderTeleporterActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS(Blueprintable, BlueprintType)
class UEM3A2BLOCKOUT_API ALadderTeleporterActor : public AActor
{
    GENERATED_BODY()

public:

    ALadderTeleporterActor();

protected:

    virtual void BeginPlay() override;

    // ── Components ───────────────────────────────────────────────────────────

    /** Decorative ladder mesh. Assign a Static Mesh in the Details panel. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* LadderMesh;

    /**
     * Overlap box placed at the BASE of the ladder.
     * Any pawn that enters it is teleported to TopTarget.
     * Resize in the editor to fit the capsule.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    UBoxComponent* BottomTrigger;

    /**
     * Overlap box placed at the TOP of the ladder.
     * Any pawn that enters it is teleported to BottomTarget.
     * Resize in the editor to fit the capsule.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    UBoxComponent* TopTrigger;

    /**
     * World position where the character lands after descending (bottom exit).
     * Move this in the editor — do not rely on the trigger position.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* BottomTarget;

    /**
     * World position where the character lands after ascending (top exit).
     * Move this in the editor — do not rely on the trigger position.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* TopTarget;

    // ── Settings ─────────────────────────────────────────────────────────────

    /**
     * Seconds that must pass before the same actor can be teleported again.
     * Prevents instant ping-pong if the destination overlaps the opposite trigger.
     */
    UPROPERTY(EditAnywhere, Category = "Ladder", meta = (ClampMin = "0.0"))
    float TeleportCooldown = 0.3f;

    // ── Overlap callbacks ────────────────────────────────────────────────────

    UFUNCTION()
    void OnBottomOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult&    SweepResult);

    UFUNCTION()
    void OnTopOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult&    SweepResult);

private:

    /**
     * World-time stamp of the last teleport per actor.
     * Weak keys are invalidated when actors are destroyed, then removed by
     * CleanupStaleEntries() which runs after every successful teleport.
     */
    TMap<TWeakObjectPtr<AActor>, float> LastTeleportTimes;

    /**
     * Core teleport logic.
     * Validates the actor, checks the cooldown, teleports, stops movement,
     * and records the timestamp.
     */
    void TryTeleport(AActor* OtherActor, USceneComponent* Target);

    /** Removes entries whose actor has since been destroyed. */
    void CleanupStaleEntries();
};
