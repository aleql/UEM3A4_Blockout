// LadderAnimInstance.h
// C++ AnimInstance base class that owns the ladder state variables.
// Set this as the Parent Class of your character's Animation Blueprint.
// ULadderClimbComponent writes bIsClimbing and ClimbSpeedSigned every frame,
// so the AnimBP graph only needs to read them — no event-graph wiring required.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LadderAnimInstance.generated.h"

UCLASS()
class UEM3A2BLOCKOUT_API ULadderAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:

    /**
     * True while the character is attached to a ladder.
     * Use this to drive state-machine transitions:
     *   Locomotion → Climb when true
     *   Climb → Locomotion when false
     */
    UPROPERTY(BlueprintReadOnly, Category = "Ladder")
    bool bIsClimbing = false;

    /**
     * Signed climb speed in cm/s. Written by ULadderClimbComponent each tick.
     * Use this to pick the animation inside the Climb state:
     *   >  threshold → anim_Ladder_Up
     *   < -threshold → anim_Ladder_Down
     *   near 0       → anim_Ladder_Idle
     * A threshold of 20 works well with the default ClimbSpeedUnitsPerSecond of 150.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Ladder")
    float ClimbSpeedSigned = 0.f;
};
