// ReturnToStartComponent.cpp
// Caches the owner's start transform and binds a key to teleport back to it.
//
// Design notes
// ─────────────
// • Input is bound via the owning actor's InputComponent (legacy binding API).
//   This works correctly even in UE5 projects that use Enhanced Input for the
//   main character actions because the binding is added to the actor's own
//   InputComponent, not to an Enhanced Input Mapping Context.  The owning
//   actor must receive input, which is guaranteed by calling
//   Actor->EnableInput(PlayerController) here in BeginPlay.
//
// • No Tick is used. The only runtime work is the key callback.
//
// • StopMovementImmediately() is called after teleporting the Character so the
//   character does not continue moving (or fall) with leftover velocity.

#include "ReturnToStartComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogReturnToStart, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UReturnToStartComponent::UReturnToStartComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UReturnToStartComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
    {
        UE_LOG(LogReturnToStart, Warning,
            TEXT("[ReturnToStartComponent] No valid owner found. Component inactive."));
        return;
    }

    // ── Cache start transform ─────────────────────────────────────────────────
    StartTransform = Owner->GetActorTransform();

    if (bDebugLog)
        UE_LOG(LogReturnToStart, Log,
            TEXT("[ReturnToStartComponent] StartTransform cached: %s"),
            *StartTransform.ToHumanReadableString());

    // ── Cache optional character pointer ──────────────────────────────────────
    OwnerCharacter = Cast<ACharacter>(Owner);

    // ── Input binding ─────────────────────────────────────────────────────────
    if (!bEnableInput)
        return;

    // Resolve the player controller.
    APawn* OwnerPawn = Cast<APawn>(Owner);
    if (!OwnerPawn)
    {
        UE_LOG(LogReturnToStart, Warning,
            TEXT("[ReturnToStartComponent] Owner '%s' is not a Pawn. "
                 "Cannot bind input. bEnableInput has no effect."),
            *Owner->GetName());
        return;
    }

    OwnerController = Cast<APlayerController>(OwnerPawn->GetController());
    if (!OwnerController)
    {
        UE_LOG(LogReturnToStart, Warning,
            TEXT("[ReturnToStartComponent] Owner '%s' has no PlayerController yet. "
                 "Input will not be bound. If using deferred possession, call "
                 "EnableInput manually after the controller is assigned."),
            *Owner->GetName());
        return;
    }

    // Enable the actor to receive input events.
    // This is safe to call even if Enhanced Input is in use for other actions.
    Owner->EnableInput(OwnerController);

    // Bind the key on the actor's InputComponent.
    // EnableInput above guarantees InputComponent is valid.
    if (Owner->InputComponent)
    {
        Owner->InputComponent->BindKey(
            ResetKey,
            IE_Pressed,
            this,
            &UReturnToStartComponent::HandleReturnToStart);

        if (bDebugLog)
            UE_LOG(LogReturnToStart, Log,
                TEXT("[ReturnToStartComponent] Bound key '%s' on '%s'."),
                *ResetKey.GetDisplayName().ToString(),
                *Owner->GetName());
    }
    else
    {
        UE_LOG(LogReturnToStart, Warning,
            TEXT("[ReturnToStartComponent] InputComponent is null after EnableInput. "
                 "Key binding skipped."));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// EndPlay
// ─────────────────────────────────────────────────────────────────────────────

void UReturnToStartComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Disable input so stale bindings do not linger if the actor is re-used.
    if (IsValid(OwnerController))
    {
        AActor* Owner = GetOwner();
        if (IsValid(Owner))
        {
            Owner->DisableInput(OwnerController);
        }
    }

    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleReturnToStart
// ─────────────────────────────────────────────────────────────────────────────

void UReturnToStartComponent::HandleReturnToStart()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
    {
        UE_LOG(LogReturnToStart, Warning,
            TEXT("[ReturnToStartComponent] HandleReturnToStart: owner is invalid."));
        return;
    }

    if (bDebugLog)
        UE_LOG(LogReturnToStart, Log,
            TEXT("[ReturnToStartComponent] '%s' pressed. Teleporting '%s' to %s."),
            *ResetKey.GetDisplayName().ToString(),
            *Owner->GetName(),
            *StartTransform.ToHumanReadableString());

    // ── Teleport ──────────────────────────────────────────────────────────────
    // bSweep=false + TeleportPhysics means no collision test during the move
    // and physics velocity is zeroed immediately by the engine.
    Owner->SetActorTransform(
        StartTransform,
        /*bSweep=*/false,
        /*OutHit=*/nullptr,
        ETeleportType::TeleportPhysics);

    // ── Stop residual character movement ──────────────────────────────────────
    if (IsValid(OwnerCharacter))
    {
        UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
        if (MoveComp)
        {
            MoveComp->StopMovementImmediately();
        }
    }

    // ── Clear root component physics velocity if it simulates physics ─────────
    UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
    if (RootPrim && RootPrim->IsSimulatingPhysics())
    {
        RootPrim->SetPhysicsLinearVelocity(FVector::ZeroVector);
        RootPrim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    if (bDebugLog)
        UE_LOG(LogReturnToStart, Log,
            TEXT("[ReturnToStartComponent] Teleport complete. New location: %s"),
            *Owner->GetActorLocation().ToString());
}
