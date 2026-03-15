// RaiseCylinderTrigger.cpp
// One-shot vertical move trigger for UEM3A2Blockout.
//
// Design notes
// ─────────────
// • FTimeline (not manual Tick lerp) for frame-rate-independent, curve-driven
//   movement. Tick is disabled at start and re-enabled only during the move.
// • MoveDirection (Rise / Descend) flips the Z sign on TargetLocation at
//   trigger time. Everything else – timeline, lerp, snap – is identical.
// • bHasTriggered is latched forever on first fire – the overlap delegate is
//   also unbound so there is zero runtime cost after triggering.
// • TargetActor is moved via SetActorLocation + TeleportPhysics so no physics
//   sweep can block the move.

#include "RaiseCylinderTrigger.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaiseTrigger, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ARaiseCylinderTrigger::ARaiseCylinderTrigger()
{
    PrimaryActorTick.bCanEverTick    = true;
    PrimaryActorTick.bStartWithTickEnabled = false; // enabled only while moving

    // ── Root ─────────────────────────────────────────────────────────────────
    TriggerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TriggerRoot"));
    SetRootComponent(TriggerRoot);

    // ── Trigger box ──────────────────────────────────────────────────────────
    TriggerZone = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerZone"));
    TriggerZone->SetupAttachment(TriggerRoot);
    TriggerZone->SetBoxExtent(FVector(100.f, 100.f, 50.f));

    TriggerZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerZone->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    TriggerZone->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
    TriggerZone->SetGenerateOverlapEvents(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ARaiseCylinderTrigger::BeginPlay()
{
    Super::BeginPlay();

    // ── Ensure we have a valid curve ──────────────────────────────────────────
    if (!AlphaCurve)
    {
        AlphaCurve = BuildLinearCurve();
        UE_LOG(LogRaiseTrigger, Warning,
            TEXT("[%s] No AlphaCurve assigned – using generated linear curve."),
            *GetName());
    }

    // ── Wire FTimeline ────────────────────────────────────────────────────────
    FOnTimelineFloat UpdateDelegate;
    UpdateDelegate.BindUFunction(this, FName("OnTimelineUpdate"));
    MoveTimeline.AddInterpFloat(AlphaCurve, UpdateDelegate);

    FOnTimelineEvent FinishedDelegate;
    FinishedDelegate.BindUFunction(this, FName("OnTimelineFinished"));
    MoveTimeline.SetTimelineFinishedFunc(FinishedDelegate);

    // Scale playback rate so the trip always takes exactly MoveDuration seconds.
    float CurveMinTime = 0.f;
    float CurveMaxTime = 1.f;
    AlphaCurve->GetTimeRange(CurveMinTime, CurveMaxTime);
    const float CurveLength = CurveMaxTime - CurveMinTime;
    if (MoveDuration > KINDA_SMALL_NUMBER && CurveLength > KINDA_SMALL_NUMBER)
    {
        MoveTimeline.SetPlayRate(CurveLength / MoveDuration);
    }

    MoveTimeline.SetLooping(false);

    // ── Wire RotateTimeline ───────────────────────────────────────────────────
    // Resolved at trigger time, not here, because RotationAlphaCurve may be null
    // and we want to fall back to AlphaCurve (which may also be built above).
    // We defer binding until StartRotation() so the correct curve is available.

    // ── Bind overlap ──────────────────────────────────────────────────────────
    TriggerZone->OnComponentBeginOverlap.AddDynamic(
        this, &ARaiseCylinderTrigger::OnTriggerOverlapBegin);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void ARaiseCylinderTrigger::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    MoveTimeline.TickTimeline(DeltaTime);
    RotateTimeline.TickTimeline(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTriggerOverlapBegin
// ─────────────────────────────────────────────────────────────────────────────

void ARaiseCylinderTrigger::OnTriggerOverlapBegin(
    UPrimitiveComponent* /*OverlappedComponent*/,
    AActor*              OtherActor,
    UPrimitiveComponent* /*OtherComp*/,
    int32                /*OtherBodyIndex*/,
    bool                 /*bFromSweep*/,
    const FHitResult&    /*SweepResult*/)
{
    // ── One-shot latch ────────────────────────────────────────────────────────
    if (bHasTriggered)
        return;

    // ── Ignore while already moving (safety – shouldn't be reachable) ─────────
    if (bIsMoving)
        return;

    // ── Must be the local player character ────────────────────────────────────
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character)
        return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || PC->GetPawn() != Character)
        return;

    // ── Defensive: TargetActor must be valid ──────────────────────────────────
    if (!IsValid(TargetActor))
    {
        UE_LOG(LogRaiseTrigger, Warning,
            TEXT("[%s] TargetActor is null – assign it in the Details panel. Aborting."),
            *GetName());
        return;
    }

    // ── Latch immediately so no re-entry is possible ──────────────────────────
    bHasTriggered = true;
    bIsMoving     = true;

    // Unbind the overlap delegate – zero cost from here on.
    TriggerZone->OnComponentBeginOverlap.RemoveDynamic(
        this, &ARaiseCylinderTrigger::OnTriggerOverlapBegin);

    // ── Cache world-space start / target ──────────────────────────────────────
    StartLocation = TargetActor->GetActorLocation();

    // Apply direction sign here – MoveDistance is always a positive value.
    const float ZOffset   = (MoveDirection == EVerticalMoveDirection::Rise) ? MoveDistance : -MoveDistance;
    TargetLocation        = StartLocation + FVector(0.f, 0.f, ZOffset);

    if (bDebugLog)
    {
        UE_LOG(LogRaiseTrigger, Log,
            TEXT("[%s] Triggered by %s – %s '%s' from %s to %s over %.2fs"),
            *GetName(),
            *Character->GetName(),
            MoveDirection == EVerticalMoveDirection::Rise ? TEXT("raising") : TEXT("lowering"),
            *TargetActor->GetName(),
            *StartLocation.ToString(),
            *TargetLocation.ToString(),
            MoveDuration);
    }

    // ── Start the timeline ────────────────────────────────────────────────────
    SetActorTickEnabled(true);
    MoveTimeline.PlayFromStart();
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTimelineUpdate
// ─────────────────────────────────────────────────────────────────────────────

void ARaiseCylinderTrigger::OnTimelineUpdate(float Alpha)
{
    if (!IsValid(TargetActor))
    {
        // Target was destroyed mid-move – stop cleanly.
        MoveTimeline.Stop();
        SetActorTickEnabled(false);
        bIsMoving = false;
        UE_LOG(LogRaiseTrigger, Warning,
            TEXT("[%s] TargetActor became invalid during move – stopping."), *GetName());
        return;
    }

    const FVector NewLocation = FMath::Lerp(StartLocation, TargetLocation, Alpha);

    TargetActor->SetActorLocation(
        NewLocation,
        /*bSweep=*/false,
        /*OutHit=*/nullptr,
        ETeleportType::TeleportPhysics);

    if (bDebugLog)
    {
        UE_LOG(LogRaiseTrigger, Verbose,
            TEXT("[%s] Move update  Alpha=%.3f  Loc=%s"),
            *GetName(), Alpha, *NewLocation.ToString());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTimelineFinished
// ─────────────────────────────────────────────────────────────────────────────

void ARaiseCylinderTrigger::OnTimelineFinished()
{
    // Snap to exact target to eliminate any floating-point accumulation.
    if (IsValid(TargetActor))
    {
        TargetActor->SetActorLocation(
            TargetLocation,
            /*bSweep=*/false,
            /*OutHit=*/nullptr,
            ETeleportType::TeleportPhysics);
    }

    bIsMoving = false;

    if (bDebugLog)
    {
        UE_LOG(LogRaiseTrigger, Log,
            TEXT("[%s] Move complete. Final location: %s"),
            *GetName(),
            IsValid(TargetActor) ? *TargetActor->GetActorLocation().ToString() : TEXT("(destroyed)"));
    }

    // ── Start rotation phase if configured ───────────────────────────────────
    const bool bWantRotation = bEnableRotationAfterRaise
        && (FMath::Abs(RotationDeltaRoll)  > KINDA_SMALL_NUMBER
            || FMath::Abs(RotationDeltaPitch) > KINDA_SMALL_NUMBER
            || FMath::Abs(RotationDeltaYaw) > KINDA_SMALL_NUMBER);

    if (!bWantRotation || !IsValid(TargetActor))
    {
        SetActorTickEnabled(false);
        return;
    }

    // Resolve which curve to use for rotation.
    UCurveFloat* CurveToUse = RotationAlphaCurve ? RotationAlphaCurve : AlphaCurve;
    if (!CurveToUse)
    {
        CurveToUse = BuildLinearCurve();
    }

    FOnTimelineFloat RotateUpdateDelegate;
    RotateUpdateDelegate.BindUFunction(this, FName("OnRotateTimelineUpdate"));
    RotateTimeline.AddInterpFloat(CurveToUse, RotateUpdateDelegate);

    FOnTimelineEvent RotateFinishedDelegate;
    RotateFinishedDelegate.BindUFunction(this, FName("OnRotateTimelineFinished"));
    RotateTimeline.SetTimelineFinishedFunc(RotateFinishedDelegate);

    float CurveMinTime = 0.f, CurveMaxTime = 1.f;
    CurveToUse->GetTimeRange(CurveMinTime, CurveMaxTime);
    const float CurveLength = CurveMaxTime - CurveMinTime;
    if (RotateDuration > KINDA_SMALL_NUMBER && CurveLength > KINDA_SMALL_NUMBER)
    {
        RotateTimeline.SetPlayRate(CurveLength / RotateDuration);
    }

    RotateTimeline.SetLooping(false);

    StartRotation = TargetActor->GetActorRotation();
    // FRotator(Pitch, Yaw, Roll) — X axis = Roll, Y axis = Pitch, Z axis = Yaw.
    TargetRotation = StartRotation + FRotator(RotationDeltaPitch, RotationDeltaYaw, RotationDeltaRoll);

    bIsRotating = true;

    if (bDebugLog)
    {
        UE_LOG(LogRaiseTrigger, Log,
            TEXT("[%s] Starting rotation from %s to %s over %.2fs"),
            *GetName(),
            *StartRotation.ToString(),
            *TargetRotation.ToString(),
            RotateDuration);
    }

    RotateTimeline.PlayFromStart();
    // Tick remains enabled — RotateTimeline.TickTimeline is called each frame.
}

// ─────────────────────────────────────────────────────────────────────────────
// OnRotateTimelineUpdate
// ─────────────────────────────────────────────────────────────────────────────

void ARaiseCylinderTrigger::OnRotateTimelineUpdate(float Alpha)
{
    if (!IsValid(TargetActor))
    {
        RotateTimeline.Stop();
        SetActorTickEnabled(false);
        bIsRotating = false;
        UE_LOG(LogRaiseTrigger, Warning,
            TEXT("[%s] TargetActor became invalid during rotation – stopping."), *GetName());
        return;
    }

    const FRotator NewRotation = FMath::Lerp(StartRotation, TargetRotation, Alpha);

    TargetActor->SetActorRotation(
        NewRotation,
        ETeleportType::TeleportPhysics);

    if (bDebugLog)
    {
        UE_LOG(LogRaiseTrigger, Verbose,
            TEXT("[%s] Rotate update  Alpha=%.3f  Rot=%s"),
            *GetName(), Alpha, *NewRotation.ToString());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnRotateTimelineFinished
// ─────────────────────────────────────────────────────────────────────────────

void ARaiseCylinderTrigger::OnRotateTimelineFinished()
{
    // Snap to exact target rotation.
    if (IsValid(TargetActor))
    {
        TargetActor->SetActorRotation(
            TargetRotation,
            ETeleportType::TeleportPhysics);
    }

    bIsRotating = false;
    SetActorTickEnabled(false); // no more work to do, ever

    if (bDebugLog)
    {
        UE_LOG(LogRaiseTrigger, Log,
            TEXT("[%s] Rotation complete. Final rotation: %s"),
            *GetName(),
            IsValid(TargetActor) ? *TargetActor->GetActorRotation().ToString() : TEXT("(destroyed)"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildLinearCurve
// ─────────────────────────────────────────────────────────────────────────────

UCurveFloat* ARaiseCylinderTrigger::BuildLinearCurve()
{
    UCurveFloat* Curve = NewObject<UCurveFloat>(this, TEXT("GeneratedLinearCurve"));

    FRichCurveKey KeyStart(0.f, 0.f);
    KeyStart.InterpMode = RCIM_Linear;

    FRichCurveKey KeyEnd(1.f, 1.f);
    KeyEnd.InterpMode = RCIM_Linear;

    Curve->FloatCurve.Keys.Add(KeyStart);
    Curve->FloatCurve.Keys.Add(KeyEnd);

    return Curve;
}
