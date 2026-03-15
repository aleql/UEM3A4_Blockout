// ToggleActorsOnKeyComponent.cpp
// Toggles a group of level actors (hidden, collision, tick) on key press.
//
// Design notes
// ─────────────
// • BeginPlay resolves both the manual TargetActors list (TSoftObjectPtr
//   resolved to raw pointers) and the tag-based sweep (GetAllActorsWithTag).
//   Both sources are merged into RuntimeTargets – duplicates are harmless
//   because each actor is still only toggled once per press.
//
// • ApplyStateToTargets centralises all three property calls so BeginPlay
//   (initial state) and OnTogglePressed (subsequent presses) share one code
//   path.
//
// • Input is bound via Actor->InputComponent using the legacy BindKey API.
//   This coexists with Enhanced Input without conflict because they use
//   separate components on the input stack.
//
// • No Tick. The only per-frame work ever done is inside OnTogglePressed which
//   is driven by the input system.

#include "ToggleActorsOnKeyComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"            // TActorIterator / GetAllActorsWithTag

DEFINE_LOG_CATEGORY_STATIC(LogToggleActors, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UToggleActorsOnKeyComponent::UToggleActorsOnKeyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UToggleActorsOnKeyComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
    {
        UE_LOG(LogToggleActors, Warning,
            TEXT("[ToggleActorsOnKey] No valid owner. Component inactive."));
        return;
    }

    // ── Initialise enabled state ──────────────────────────────────────────────
    bCurrentlyEnabled = bStartEnabled;

    // ── Build RuntimeTargets: Method A – manual list ──────────────────────────
    for (const TSoftObjectPtr<AActor>& SoftRef : TargetActors)
    {
        AActor* Resolved = SoftRef.Get(); // resolves if the actor is in memory
        if (IsValid(Resolved) && Resolved != Owner)
        {
            RuntimeTargets.AddUnique(Resolved);
        }
    }

    // ── Build RuntimeTargets: Method B – tag sweep ────────────────────────────
    if (TargetTag != NAME_None)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            TArray<AActor*> Tagged;
            UGameplayStatics::GetAllActorsWithTag(World, TargetTag, Tagged);

            for (AActor* Actor : Tagged)
            {
                if (IsValid(Actor) && Actor != Owner)
                {
                    RuntimeTargets.AddUnique(Actor);
                }
            }

            if (bDebugLog)
                UE_LOG(LogToggleActors, Log,
                    TEXT("[ToggleActorsOnKey] Tag '%s' found %d actor(s)."),
                    *TargetTag.ToString(), Tagged.Num());
        }
    }

    if (bDebugLog)
        UE_LOG(LogToggleActors, Log,
            TEXT("[ToggleActorsOnKey] RuntimeTargets resolved: %d actor(s). "
                 "StartEnabled=%s."),
            RuntimeTargets.Num(),
            bStartEnabled ? TEXT("true") : TEXT("false"));

    // ── Apply initial state ───────────────────────────────────────────────────
    ApplyStateToTargets();

    // ── Input binding ─────────────────────────────────────────────────────────
    APawn* OwnerPawn = Cast<APawn>(Owner);
    if (!OwnerPawn)
    {
        UE_LOG(LogToggleActors, Warning,
            TEXT("[ToggleActorsOnKey] Owner '%s' is not a Pawn. "
                 "Key binding skipped."),
            *Owner->GetName());
        return;
    }

    OwnerController = Cast<APlayerController>(OwnerPawn->GetController());
    if (!OwnerController)
    {
        // Fallback: try the global player 0 controller.
        OwnerController = UGameplayStatics::GetPlayerController(Owner, 0);
    }

    if (!OwnerController)
    {
        UE_LOG(LogToggleActors, Warning,
            TEXT("[ToggleActorsOnKey] No PlayerController found for '%s'. "
                 "Key binding skipped."),
            *Owner->GetName());
        return;
    }

    // EnableInput creates the actor's InputComponent if it doesn't exist yet
    // and pushes it onto the controller's input stack.
    Owner->EnableInput(OwnerController);

    if (Owner->InputComponent)
    {
        Owner->InputComponent->BindKey(
            ToggleKey,
            IE_Pressed,
            this,
            &UToggleActorsOnKeyComponent::OnTogglePressed);

        if (bDebugLog)
            UE_LOG(LogToggleActors, Log,
                TEXT("[ToggleActorsOnKey] Key '%s' bound on '%s'."),
                *ToggleKey.GetDisplayName().ToString(),
                *Owner->GetName());
    }
    else
    {
        UE_LOG(LogToggleActors, Warning,
            TEXT("[ToggleActorsOnKey] InputComponent is null after EnableInput. "
                 "Key binding skipped."));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// EndPlay
// ─────────────────────────────────────────────────────────────────────────────

void UToggleActorsOnKeyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AActor* Owner = GetOwner();
    if (IsValid(Owner) && IsValid(OwnerController))
    {
        Owner->DisableInput(OwnerController);
    }

    RuntimeTargets.Empty();

    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTogglePressed
// ─────────────────────────────────────────────────────────────────────────────

void UToggleActorsOnKeyComponent::OnTogglePressed()
{
    bCurrentlyEnabled = !bCurrentlyEnabled;

    if (bDebugLog)
        UE_LOG(LogToggleActors, Log,
            TEXT("[ToggleActorsOnKey] Toggle pressed. New state: %s."),
            bCurrentlyEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));

    ApplyStateToTargets();
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyStateToTargets
// ─────────────────────────────────────────────────────────────────────────────

void UToggleActorsOnKeyComponent::ApplyStateToTargets()
{
    int32 ToggleCount = 0;

    for (AActor* Actor : RuntimeTargets)
    {
        // Skip actors that were destroyed after BeginPlay.
        if (!IsValid(Actor))
            continue;

        if (bToggleHiddenInGame)
        {
            // Hidden = true means invisible → disabled.
            Actor->SetActorHiddenInGame(!bCurrentlyEnabled);
        }

        if (bToggleCollision)
        {
            Actor->SetActorEnableCollision(bCurrentlyEnabled);
        }

        if (bToggleTick)
        {
            Actor->SetActorTickEnabled(bCurrentlyEnabled);
        }

        ++ToggleCount;
    }

    if (bDebugLog)
        UE_LOG(LogToggleActors, Log,
            TEXT("[ToggleActorsOnKey] Applied state '%s' to %d/%d actor(s). "
                 "(HiddenInGame=%s | Collision=%s | Tick=%s)"),
            bCurrentlyEnabled ? TEXT("ENABLED") : TEXT("DISABLED"),
            ToggleCount,
            RuntimeTargets.Num(),
            bToggleHiddenInGame ? TEXT("yes") : TEXT("no"),
            bToggleCollision    ? TEXT("yes") : TEXT("no"),
            bToggleTick         ? TEXT("yes") : TEXT("no"));
}
