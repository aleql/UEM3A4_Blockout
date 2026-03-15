// LadderActor.h
// Place in the level for every climbable ladder.
// Overlap volume registers the ladder with ULadderClimbComponent on the character.
// Adjust BottomPoint / TopPoint and the FacingDirection arrow per instance in the editor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LadderActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UArrowComponent;

UCLASS(Blueprintable, BlueprintType)
class UEM3A2BLOCKOUT_API ALadderActor : public AActor
{
    GENERATED_BODY()

public:

    ALadderActor();

protected:

    virtual void BeginPlay() override;

    // ── Components ───────────────────────────────────────────────────────────

    /** Visible ladder mesh. Assign a Static Mesh in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* LadderMesh;

    /**
     * Box that detects when the player is near enough to start climbing.
     * Automatically calls SetCurrentLadder on ULadderClimbComponent.
     * Resize in the editor to match your mesh footprint.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    UBoxComponent* InteractVolume;

    /**
     * World position where the character's root sits at the BASE of the ladder.
     * Move this in the editor to align with the bottom rung.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* BottomPoint;

    /**
     * World position where the character's root sits at the TOP of the ladder.
     * Move this in the editor to align with the top rung / platform edge.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* TopPoint;

    /**
     * Arrow pointing AWAY from the ladder face toward where the player stands.
     * The character is rotated to face this direction and offset forward by
     * LadderOffsetFromMesh while climbing.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Components",
        meta = (AllowPrivateAccess = "true"))
    UArrowComponent* FacingDirection;

    // ── Overlap callbacks ────────────────────────────────────────────────────

    UFUNCTION()
    void OnInteractVolumeBegin(
        UPrimitiveComponent* OverlappedComp,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult&    SweepResult);

    UFUNCTION()
    void OnInteractVolumeEnd(
        UPrimitiveComponent* OverlappedComp,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex);

public:

    // ── Settings (tune per ladder instance in the Details panel) ─────────────

    /**
     * Maximum distance (cm) from the BottomPoint→TopPoint axis that TryStartClimb
     * will accept. Secondary guard in addition to the overlap volume.
     * Raise if the overlap volume is wide and side-approaches should still climb.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Settings",
        meta = (ClampMin = "1.0"))
    float SnapDistance = 100.f;

    /**
     * Distance (cm) the character capsule centre is held away from the ladder mesh
     * in the FacingDirection during climbing.
     * Increase to prevent capsule penetration; decrease to hug the rungs.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Settings",
        meta = (ClampMin = "0.0"))
    float LadderOffsetFromMesh = 40.f;

    // ── Getters used by ULadderClimbComponent ────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Ladder")
    FVector GetBottomWorld() const;

    UFUNCTION(BlueprintCallable, Category = "Ladder")
    FVector GetTopWorld() const;

    /** Distance in cm between BottomPoint and TopPoint. */
    UFUNCTION(BlueprintCallable, Category = "Ladder")
    float GetHeight() const;

    /** Unit forward vector of FacingDirection (points away from the ladder face). */
    UFUNCTION(BlueprintCallable, Category = "Ladder")
    FVector GetForwardDirection() const;
};
