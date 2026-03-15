// ReturnToStartComponent.h
// Caches the owning actor's transform at BeginPlay and teleports back to it
// when the player presses ResetKey (default: O).
//
// Attach to BP_ThirdPersonCharacter via Add Component in the Blueprint editor.
// No Tick used.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"          // FKey, EKeys
#include "ReturnToStartComponent.generated.h"

// Forward declarations
class ACharacter;
class APlayerController;
class UCharacterMovementComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API UReturnToStartComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UReturnToStartComponent();

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── Settings ──────────────────────────────────────────────────────────────

    /** Enable or disable the reset key entirely. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return To Start")
    bool bEnableInput = true;

    /**
     * Key the player presses to teleport back to the start.
     * Default: O.  Change in the Details panel – no recompile needed.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return To Start")
    FKey ResetKey = EKeys::O;

    /** Log events to the Output Log. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return To Start")
    bool bDebugLog = false;

    // ── Read-only state ────────────────────────────────────────────────────────

    /** World transform of the owning actor captured at BeginPlay. Read-only. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Return To Start")
    FTransform StartTransform;

    // ── Cached pointers ────────────────────────────────────────────────────────

    UPROPERTY()
    ACharacter* OwnerCharacter = nullptr;

    UPROPERTY()
    APlayerController* OwnerController = nullptr;

    // ── Input callback ─────────────────────────────────────────────────────────

    /** Fires when ResetKey is pressed. Teleports the owner to StartTransform. */
    void HandleReturnToStart();
};
