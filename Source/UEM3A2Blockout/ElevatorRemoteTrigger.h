// ElevatorRemoteTrigger.h
// A no-visual "lever" BoxCollision that remotely calls RequestToggle() on a
// target AElevatorActor when the player walks into it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "ElevatorActor.h"
#include "ElevatorRemoteTrigger.generated.h"

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom),
    meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API AElevatorRemoteTrigger : public AActor
{
    GENERATED_BODY()

public:

    AElevatorRemoteTrigger();

protected:

    virtual void BeginPlay() override;

    // ── Components ────────────────────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RemoteTrigger|Components",
        meta = (AllowPrivateAccess = "true"))
    USceneComponent* TriggerRoot;

    /** The activation volume. Resize the box extent in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RemoteTrigger|Components",
        meta = (AllowPrivateAccess = "true"))
    UBoxComponent* TriggerZone;

    // ── Settings ──────────────────────────────────────────────────────────────

    /** The elevator instance in the level that this trigger controls. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "RemoteTrigger|Settings")
    AElevatorActor* TargetElevator = nullptr;

    /**
     * When true the trigger fires once and then disables itself permanently.
     * When false (default) it fires every time the player enters, subject to
     * the elevator's own cooldown.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RemoteTrigger|Settings")
    bool bOneShot = false;

    /** Print verbose logs to Output Log. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RemoteTrigger|Debug")
    bool bDebugLog = false;

    // ── Runtime state ─────────────────────────────────────────────────────────

    /** Latched to true after the first successful fire when bOneShot is true. */
    bool bFired = false;

    // ── Callbacks ─────────────────────────────────────────────────────────────

    UFUNCTION()
    void OnTriggerOverlapBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult&    SweepResult);
};
