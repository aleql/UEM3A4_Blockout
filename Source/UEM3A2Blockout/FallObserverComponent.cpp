// FallObserverComponent.cpp
// Detects prolonged falling on the owning ACharacter using movement mode
// delegates and a single timer. No Tick.
//
// Design notes
// ─────────────
// • OnMovementModeChanged is the correct UE5 hook: it fires synchronously
//   when the CharacterMovementComponent transitions between modes, so there is
//   no polling and no Tick needed.
//
// • The timer is one-shot. It fires once after FallThresholdSeconds. If the
//   character lands before the timer fires it is cleared and nothing prints.
//
// • bHasReportedThisFall prevents the message from repeating if the character
//   stays in MOVE_Falling after the threshold (e.g. falling into a pit).
//   It resets when the character lands so subsequent falls are reported again.
//
// • All timer work is cleaned up in EndPlay to avoid stale callbacks if the
//   actor is destroyed while a timer is pending.

#include "FallObserverComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFallObserver, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UFallObserverComponent::UFallObserverComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UFallObserverComponent::BeginPlay()
{
    Super::BeginPlay();

    // ── Resolve owner and movement component ──────────────────────────────────
    OwnerCharacter = Cast<ACharacter>(GetOwner());

    if (!OwnerCharacter)
    {
        UE_LOG(LogFallObserver, Warning,
            TEXT("[FallObserverComponent] Owner '%s' is not an ACharacter. "
                 "Component will be inactive."),
            *GetNameSafe(GetOwner()));
        return;
    }

    MovementComp = OwnerCharacter->GetCharacterMovement();

    if (!MovementComp)
    {
        UE_LOG(LogFallObserver, Warning,
            TEXT("[FallObserverComponent] Owner '%s' has no CharacterMovementComponent. "
                 "Component will be inactive."),
            *OwnerCharacter->GetName());
        return;
    }

    // ── Bind movement mode delegate ───────────────────────────────────────────
    // ACharacter::OnMovementModeChanged broadcasts whenever the movement mode
    // transitions (Walking → Falling, Falling → Walking, etc.).
    OwnerCharacter->MovementModeChangedDelegate.AddDynamic(
        this, &UFallObserverComponent::OnMovementModeChanged);

    UE_LOG(LogFallObserver, Log,
        TEXT("[FallObserverComponent] Attached to '%s'. Threshold=%.1fs."),
        *OwnerCharacter->GetName(), FallThresholdSeconds);
}

// ─────────────────────────────────────────────────────────────────────────────
// EndPlay  –  always clean up the timer
// ─────────────────────────────────────────────────────────────────────────────

void UFallObserverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear any pending timers so they cannot fire after the actor is gone.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FallTimerHandle);
        World->GetTimerManager().ClearTimer(HideWidgetTimerHandle);
    }

    HideFallWidget();

    // Unbind the delegate if the character is still alive.
    if (IsValid(OwnerCharacter))
    {
        OwnerCharacter->MovementModeChangedDelegate.RemoveDynamic(
            this, &UFallObserverComponent::OnMovementModeChanged);
    }

    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnMovementModeChanged
// ─────────────────────────────────────────────────────────────────────────────

void UFallObserverComponent::OnMovementModeChanged(ACharacter*    /*Character*/,
                                                    EMovementMode  PrevMovementMode,
                                                    uint8          /*PreviousCustomMode*/)
{
    if (!IsValid(MovementComp))
        return;

    const EMovementMode CurrentMode = MovementComp->MovementMode;

    if (CurrentMode == MOVE_Falling)
    {
        // ── Character just started falling ────────────────────────────────────
        // Start a one-shot timer. If the character is still falling when it
        // fires, we report the long fall.
        UWorld* World = GetWorld();
        if (!World)
            return;

        // Don't restart if we are already timing (e.g. mode flicker).
        if (!World->GetTimerManager().IsTimerActive(FallTimerHandle))
        {
            World->GetTimerManager().SetTimer(
                FallTimerHandle,
                this,
                &UFallObserverComponent::OnFallThresholdReached,
                FallThresholdSeconds,
                /*bLooping=*/false);

            UE_LOG(LogFallObserver, Verbose,
                TEXT("[FallObserverComponent] Fall started. Timer set for %.1fs."),
                FallThresholdSeconds);
        }
    }
    else
    {
        // ── Character left MOVE_Falling (landed, grabbed wall, etc.) ──────────
        UWorld* World = GetWorld();
        if (World)
        {
            World->GetTimerManager().ClearTimer(FallTimerHandle);
        }

        // Reset the gate so the next fall can report again.
        if (bHasReportedThisFall)
        {
            bHasReportedThisFall = false;

            UE_LOG(LogFallObserver, Verbose,
                TEXT("[FallObserverComponent] Character landed. Report gate reset."));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnFallThresholdReached
// ─────────────────────────────────────────────────────────────────────────────

void UFallObserverComponent::OnFallThresholdReached()
{
    // Safety: owner or movement comp may have been destroyed before the timer fired.
    if (!IsValid(OwnerCharacter) || !IsValid(MovementComp))
    {
        UE_LOG(LogFallObserver, Warning,
            TEXT("[FallObserverComponent] Timer fired but owner/movement is invalid."));
        return;
    }

    // Double-check the character is still actually falling.
    if (!MovementComp->IsFalling())
    {
        // Already landed between the timer firing and this callback executing.
        return;
    }

    // Gate: only report once per continuous fall.
    if (bHasReportedThisFall)
        return;

    bHasReportedThisFall = true;

    // ── Build the display string ──────────────────────────────────────────────
    // Replace the {Time} token with the actual threshold value (1 decimal place).
    const FString TimeString = FString::Printf(TEXT("%.1f"), FallThresholdSeconds);

    FString ResolvedMessage;
    if (FallMessage.IsEmpty())
    {
        // Fallback if the designer cleared the field entirely.
        ResolvedMessage = FString::Printf(
            TEXT("[%s] Has been falling for %.1f+ seconds!"),
            *OwnerCharacter->GetName(),
            FallThresholdSeconds);
    }
    else
    {
        ResolvedMessage = FString::Printf(
            TEXT("[%s] %s"),
            *OwnerCharacter->GetName(),
            *FallMessage.ToString().Replace(TEXT("{Time}"), *TimeString));
    }

    // ── On-screen message (scaled) ────────────────────────────────────────────
    if (bDebugToScreen && GEngine)
    {
        // Use the five-parameter overload that accepts a text scale (FVector2D).
        // Key = -1  → never overwrites a previous line, each call adds a new entry.
        GEngine->AddOnScreenDebugMessage(
            /*Key=*/          -1,
            /*TimeToDisplay=*/MessageDisplayDuration,
            /*Color=*/        MessageColor,
            /*Text=*/         ResolvedMessage,
            /*bNewerOnTop=*/  true,
            /*TextScale=*/    FVector2D(MessageScale, MessageScale));
    }

    if (bDebugToLog)
    {
        UE_LOG(LogFallObserver, Warning, TEXT("%s"), *ResolvedMessage);
    }

    // ── UMG widget ────────────────────────────────────────────────────────────
    if (FallMessageWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetOwner(), 0);
        if (PC)
        {
            HideFallWidget(); // remove any leftover from a previous fall

            ActiveFallWidget = CreateWidget<UUserWidget>(PC, FallMessageWidgetClass);
            if (ActiveFallWidget)
            {
                ActiveFallWidget->AddToPlayerScreen(0);

                GetWorld()->GetTimerManager().SetTimer(
                    HideWidgetTimerHandle,
                    this,
                    &UFallObserverComponent::HideFallWidget,
                    MessageDisplayDuration,
                    /*bLooping=*/false);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HideFallWidget
// ─────────────────────────────────────────────────────────────────────────────

void UFallObserverComponent::HideFallWidget()
{
    if (ActiveFallWidget)
    {
        ActiveFallWidget->RemoveFromParent();
        ActiveFallWidget = nullptr;
    }
}
