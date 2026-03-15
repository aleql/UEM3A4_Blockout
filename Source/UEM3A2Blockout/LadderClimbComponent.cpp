// LadderClimbComponent.cpp

#include "LadderClimbComponent.h"
#include "LadderActor.h"
#include "LadderAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UEM3A2Blockout.h"

ULadderClimbComponent::ULadderClimbComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void ULadderClimbComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ACharacter>(GetOwner());
}

UCharacterMovementComponent* ULadderClimbComponent::GetOwnerMovement() const
{
    return OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
}

// ── AnimInstance bridge ───────────────────────────────────────────────────────

void ULadderClimbComponent::PushAnimVars() const
{
    if (!OwnerCharacter) return;

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh) return;

    ULadderAnimInstance* AnimInst = Cast<ULadderAnimInstance>(Mesh->GetAnimInstance());
    if (!AnimInst) return; // AnimBP parent not set to ULadderAnimInstance — silently skip

    AnimInst->bIsClimbing = bIsClimbing;

    if (bIsClimbing && FMath::Abs(ClimbInput) > MinInputDeadzone)
    {
        AnimInst->ClimbSpeedSigned = ClimbInput * ClimbSpeedUnitsPerSecond;
    }
    else
    {
        AnimInst->ClimbSpeedSigned = 0.f;
    }
}

// ── Ladder reference ──────────────────────────────────────────────────────────

void ULadderClimbComponent::SetCurrentLadder(ALadderActor* Ladder)
{
    if (!bIsClimbing)
    {
        CurrentLadder = Ladder;
    }
}

// ── TryStartClimb ─────────────────────────────────────────────────────────────

void ULadderClimbComponent::TryStartClimb()
{
    if (bIsClimbing)              return;
    if (!CurrentLadder.IsValid()) return;
    if (!OwnerCharacter)          return;

    UCharacterMovementComponent* MoveComp = GetOwnerMovement();
    if (!MoveComp) return;

    const FVector Bottom     = CurrentLadder->GetBottomWorld();
    const FVector Top        = CurrentLadder->GetTopWorld();
    const float   Height     = FMath::Max(CurrentLadder->GetHeight(), 1.f);
    const FVector LadderDir  = (Top - Bottom).GetSafeNormal();
    const FVector FacingDir  = CurrentLadder->GetForwardDirection();
    const float   MeshOffset = CurrentLadder->LadderOffsetFromMesh;

    // ── Snap-distance guard ───────────────────────────────────────────────────
    // Reject if the character is too far from the ladder axis (e.g. approached from the side).
    const FVector CharPos   = OwnerCharacter->GetActorLocation();
    const FVector Closest   = FMath::ClosestPointOnSegment(CharPos, Bottom, Top);
    const float   DistToAxis = FVector::Dist(CharPos, Closest);

    if (DistToAxis > CurrentLadder->SnapDistance)
    {
        UE_LOG(LogUEM3A2Blockout, Verbose,
            TEXT("TryStartClimb rejected: distance %.1f > SnapDistance %.1f"),
            DistToAxis, CurrentLadder->SnapDistance);
        return;
    }

    // ── Compute initial ClimbAlpha (closest point on ladder axis) ─────────────
    const float Proj = FVector::DotProduct(CharPos - Bottom, LadderDir);
    ClimbAlpha       = FMath::Clamp(Proj / Height, 0.f, 1.f);

    // ── Snap character to ladder line, offset away from the mesh ─────────────
    const FVector SnapBase = FMath::Lerp(Bottom, Top, ClimbAlpha);
    const FVector SnapPos  = SnapBase + FacingDir * MeshOffset;
    OwnerCharacter->SetActorLocation(SnapPos, false, nullptr, ETeleportType::TeleportPhysics);

    // ── Face along FacingDirection ────────────────────────────────────────────
    OwnerCharacter->SetActorRotation(FacingDir.Rotation());

    // ── Disable normal movement and gravity ───────────────────────────────────
    SavedGravityScale       = MoveComp->GravityScale;
    MoveComp->GravityScale  = 0.f;
    MoveComp->SetMovementMode(MOVE_Flying);
    MoveComp->StopMovementImmediately();
    MoveComp->bOrientRotationToMovement       = false;
    OwnerCharacter->bUseControllerRotationYaw = false;

    bIsClimbing = true;
    ClimbInput  = 0.f;
}

// ── EndClimb ─────────────────────────────────────────────────────────────────

void ULadderClimbComponent::EndClimb(bool bUseStructuredExit)
{
    if (!bIsClimbing)    return;
    if (!OwnerCharacter) return;

    // ── Exit position ─────────────────────────────────────────────────────────
    if (bUseStructuredExit && CurrentLadder.IsValid())
    {
        const FVector FacingDir = CurrentLadder->GetForwardDirection();
        const FVector Bottom    = CurrentLadder->GetBottomWorld();
        const FVector Top       = CurrentLadder->GetTopWorld();

        FVector ExitPos;
        if (ClimbAlpha >= 1.f - KINDA_SMALL_NUMBER)
        {
            // Top exit: step forward off the ladder onto the platform above.
            ExitPos = Top + FacingDir * ExitForwardOffset;
        }
        else
        {
            // Bottom exit: step back from the ladder base to landing ground.
            ExitPos = Bottom + FacingDir * ExitBackwardOffset;
        }

        OwnerCharacter->SetActorLocation(ExitPos, false, nullptr, ETeleportType::TeleportPhysics);
    }

    // ── Restore movement ──────────────────────────────────────────────────────
    if (UCharacterMovementComponent* MoveComp = GetOwnerMovement())
    {
        MoveComp->GravityScale          = SavedGravityScale;
        MoveComp->SetMovementMode(MOVE_Walking);
        MoveComp->bOrientRotationToMovement = true;
    }

    // Matches the base character constructor default.
    OwnerCharacter->bUseControllerRotationYaw = false;

    bIsClimbing = false;
    ClimbInput  = 0.f;
    CurrentLadder.Reset();

    PushAnimVars(); // immediately clear AnimBP state so no one-frame glitch
}

// ── TickComponent ─────────────────────────────────────────────────────────────

void ULadderClimbComponent::TickComponent(
    float                        DeltaTime,
    ELevelTick                   TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Push animation variables every frame.
    // ClimbInput was set by DoMove earlier this frame (input runs before component tick).
    PushAnimVars();

    if (!bIsClimbing)
    {
        ClimbInput = 0.f; // ensure clear when not climbing
        return;
    }

    if (!CurrentLadder.IsValid())
    {
        EndClimb(false); // ladder removed mid-climb; restore in place
        return;
    }

    // ── Apply deadzone ────────────────────────────────────────────────────────
    const float FilteredInput = (FMath::Abs(ClimbInput) > MinInputDeadzone) ? ClimbInput : 0.f;

    // ── Advance ClimbAlpha ────────────────────────────────────────────────────
    const float Height     = FMath::Max(CurrentLadder->GetHeight(), 1.f);
    const float AlphaDelta = (FilteredInput * ClimbSpeedUnitsPerSecond * DeltaTime) / Height;
    const float NewAlpha   = FMath::Clamp(ClimbAlpha + AlphaDelta, 0.f, 1.f);

    // ── Auto-exit when the player pushes past an end ──────────────────────────
    if (NewAlpha >= 1.f && FilteredInput > 0.f)
    {
        ClimbAlpha = 1.f;
        EndClimb(true);
        return;
    }
    if (NewAlpha <= 0.f && FilteredInput < 0.f)
    {
        ClimbAlpha = 0.f;
        EndClimb(true);
        return;
    }

    ClimbAlpha = NewAlpha;

    // ── Set world position: ladder axis point + mesh offset ───────────────────
    const FVector Bottom     = CurrentLadder->GetBottomWorld();
    const FVector Top        = CurrentLadder->GetTopWorld();
    const FVector FacingDir  = CurrentLadder->GetForwardDirection();
    const float   MeshOffset = CurrentLadder->LadderOffsetFromMesh;

    const FVector NewPos = FMath::Lerp(Bottom, Top, ClimbAlpha) + FacingDir * MeshOffset;
    OwnerCharacter->SetActorLocation(NewPos, false, nullptr, ETeleportType::TeleportPhysics);
    OwnerCharacter->SetActorRotation(FacingDir.Rotation());

    // Suppress velocity so the character does not drift when MOVE_Flying.
    if (UCharacterMovementComponent* MoveComp = GetOwnerMovement())
    {
        MoveComp->StopMovementImmediately();
    }

    // Reset ClimbInput at end of tick.
    // DoMove will re-write it next frame if the player is still pressing W/S.
    // Without this reset the last held value would persist when input is released.
    ClimbInput = 0.f;
}
