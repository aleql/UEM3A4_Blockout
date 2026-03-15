// FallObserverComponent.h
// Detects when the owning ACharacter has been falling for at least
// FallThresholdSeconds and prints a debug message. No Tick used.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "FallObserverComponent.generated.h"

// Forward declarations – full headers included in the .cpp only.
class ACharacter;
class UCharacterMovementComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API UFallObserverComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UFallObserverComponent();

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── Settings ──────────────────────────────────────────────────────────────

    /** Seconds the character must be falling before the message fires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver",
        meta = (ClampMin = "0.1"))
    float FallThresholdSeconds = 2.0f;

    /** Print a message on screen via GEngine. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver")
    bool bDebugToScreen = true;

    /** Also print to the Output Log. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver")
    bool bDebugToLog = false;

    /**
     * Text shown on screen when the fall threshold is reached.
     * Use {Time} anywhere in the string to insert FallThresholdSeconds.
     * Example: "Falling for {Time}+ seconds!"
     * Leave empty to use the built-in fallback string.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver")
    FText FallMessage = FText::FromString(TEXT("Has been falling for {Time}+ seconds!"));

    /** Color of the on-screen debug message. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver")
    FColor MessageColor = FColor::Red;

    /** Scale of the on-screen text (2.0 = twice the default size). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver",
        meta = (ClampMin = "0.5", ClampMax = "8.0"))
    float MessageScale = 2.0f;

    /** How many seconds the on-screen message stays visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver",
        meta = (ClampMin = "0.5"))
    float MessageDisplayDuration = 4.0f;

    /**
     * Optional UMG widget to show when the fall threshold is reached.
     * Assign a Blueprint child of UUserWidget in the Details panel.
     * The widget is added to the player screen and removed after
     * MessageDisplayDuration. Leave empty to use only the debug message.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallObserver")
    TSubclassOf<UUserWidget> FallMessageWidgetClass;

    // ── Runtime state ─────────────────────────────────────────────────────────

    UPROPERTY()
    ACharacter* OwnerCharacter = nullptr;

    UPROPERTY()
    UCharacterMovementComponent* MovementComp = nullptr;

    FTimerHandle FallTimerHandle;
    FTimerHandle HideWidgetTimerHandle;

    /** Currently displayed fall widget. Null when no widget is active. */
    UPROPERTY()
    TObjectPtr<UUserWidget> ActiveFallWidget;

    /** Prevents the message from printing more than once per continuous fall. */
    bool bHasReportedThisFall = false;

    // ── Internal ──────────────────────────────────────────────────────────────

    /** Bound to the character's OnMovementModeChanged delegate. */
    UFUNCTION()
    void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode,
                               uint8 PreviousCustomMode);

    /** Called by the timer after FallThresholdSeconds of continuous falling. */
    UFUNCTION()
    void OnFallThresholdReached();

    /** Removes the active fall widget and clears the hide timer. */
    void HideFallWidget();
};
