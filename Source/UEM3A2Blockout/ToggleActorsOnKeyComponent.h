// ToggleActorsOnKeyComponent.h
// Toggles a group of level actors (hidden, collision, tick) when the player
// presses ToggleKey (default: P).
//
// Two ways to define the group (both can be used together):
//   A) Manual list: assign actors directly to TargetActors.
//   B) Tag-based:   set TargetTag; actors with that tag are collected at BeginPlay.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "ToggleActorsOnKeyComponent.generated.h"

class APlayerController;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API UToggleActorsOnKeyComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UToggleActorsOnKeyComponent();

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── Toggle key ────────────────────────────────────────────────────────────

    /** Key that triggers the toggle. Default: P. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
    FKey ToggleKey = EKeys::P;

    // ── Initial state ─────────────────────────────────────────────────────────

    /**
     * If true, target actors start in their normal (enabled) state.
     * If false, they start hidden / collision-off / tick-off at BeginPlay.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
    bool bStartEnabled = true;

    // ── Which properties to toggle ────────────────────────────────────────────

    /** Call SetActorHiddenInGame on each target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
    bool bToggleHiddenInGame = true;

    /** Call SetActorEnableCollision on each target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
    bool bToggleCollision = true;

    /** Call SetActorTickEnabled on each target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
    bool bToggleTick = true;

    /** Print state changes to the Output Log. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
    bool bDebugLog = false;

    // ── Group definition: Method A – Manual list ──────────────────────────────

    /**
     * Actors to toggle. Assign level instances via the eyedropper in the
     * Details panel. Soft references so they survive level streaming.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle|Targets")
    TArray<TSoftObjectPtr<AActor>> TargetActors;

    // ── Group definition: Method B – Tag-based auto-collect ───────────────────

    /**
     * If not None, all actors in the level that have this tag are added to the
     * runtime list at BeginPlay (in addition to any actors in TargetActors).
     * Tag your actors in Details → Actor → Tags.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle|Targets")
    FName TargetTag = NAME_None;

    // ── Runtime state ─────────────────────────────────────────────────────────

    /** Current enabled state of the group. Flips on every key press. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Toggle")
    bool bCurrentlyEnabled = true;

    /** Resolved list of actors built in BeginPlay from both methods. */
    UPROPERTY()
    TArray<AActor*> RuntimeTargets;

    /** Cached controller for DisableInput on EndPlay. */
    UPROPERTY()
    APlayerController* OwnerController = nullptr;

    // ── Internal ──────────────────────────────────────────────────────────────

    /** Applies the current bCurrentlyEnabled state to all RuntimeTargets. */
    void ApplyStateToTargets();

    /** Fires when ToggleKey is pressed. */
    void OnTogglePressed();
};
