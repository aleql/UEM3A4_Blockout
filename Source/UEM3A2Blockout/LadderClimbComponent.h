// LadderClimbComponent.h
// ActorComponent that handles all ladder-climbing logic.
// Attach to the ThirdPersonCharacter (created automatically in the C++ constructor).
//
// Animation integration (push model):
//   ULadderClimbComponent writes bIsClimbing and ClimbSpeedSigned directly into
//   ULadderAnimInstance every TickComponent call.  The AnimBP just reads them.
//   Set the AnimBP parent class to ULadderAnimInstance in the editor.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LadderClimbComponent.generated.h"

class ALadderActor;
class ACharacter;
class UCharacterMovementComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API ULadderClimbComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    ULadderClimbComponent();

    virtual void TickComponent(
        float                        DeltaTime,
        ELevelTick                   TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ── Public API ───────────────────────────────────────────────────────────

    /** Called by ALadderActor's overlap volume to register or clear a nearby ladder. */
    void SetCurrentLadder(ALadderActor* Ladder);

    /** Called when the player presses E. Starts climbing if a valid ladder is near. */
    void TryStartClimb();

    /**
     * Exits climbing mode and restores normal walking.
     * @param bUseStructuredExit  true  → teleport character to a clean exit position.
     *                            false → restore movement in place (e.g. ladder removed).
     */
    void EndClimb(bool bUseStructuredExit);

    // ── State ────────────────────────────────────────────────────────────────

    /** True while the character is attached to a ladder. */
    UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Ladder|State")
    bool bIsClimbing = false;

    /** Normalised position on the ladder: 0 = bottom, 1 = top. */
    UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Ladder|State")
    float ClimbAlpha = 0.f;

    /**
     * Directional input fed from DoMove each frame: +1 up, -1 down, 0 idle.
     * Automatically reset to 0 at the end of each TickComponent so it clears
     * when the player releases the key.
     */
    UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Ladder|State")
    float ClimbInput = 0.f;

    // ── Settings ─────────────────────────────────────────────────────────────

    /** Climb speed in cm/s. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Settings",
        meta = (ClampMin = "10.0"))
    float ClimbSpeedUnitsPerSecond = 150.f;

    /**
     * ClimbInput values with absolute magnitude below this are treated as zero.
     * Prevents controller drift from triggering climb or ruining the idle animation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Settings",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinInputDeadzone = 0.1f;

    /**
     * Distance (cm) the character is pushed in FacingDirection on TOP exit.
     * Should be larger than LadderOffsetFromMesh so the character clears the ladder.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Settings",
        meta = (ClampMin = "0.0"))
    float ExitForwardOffset = 80.f;

    /**
     * Distance (cm) the character is pushed in FacingDirection on BOTTOM exit
     * (i.e. they step back from the base of the ladder).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Settings",
        meta = (ClampMin = "0.0"))
    float ExitBackwardOffset = 60.f;

    // ── Internal ladder reference ─────────────────────────────────────────────

    TWeakObjectPtr<ALadderActor> CurrentLadder;

protected:

    virtual void BeginPlay() override;

private:

    UPROPERTY()
    ACharacter* OwnerCharacter = nullptr;

    float SavedGravityScale = 1.f;

    UCharacterMovementComponent* GetOwnerMovement() const;

    /**
     * Writes bIsClimbing and ClimbSpeedSigned into the character's ULadderAnimInstance.
     * Called every TickComponent so the AnimBP always has fresh values.
     * Does nothing if the AnimInstance is not a ULadderAnimInstance subclass.
     */
    void PushAnimVars() const;
};
