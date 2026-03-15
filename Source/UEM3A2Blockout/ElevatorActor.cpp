// ElevatorActor.cpp
// Pure C++ elevator implementation for UEM3A2Blockout.
//
// Design notes
// ─────────────
// • FTimeline (not Tick-manual lerp) is used so the curve-driven interpolation
//   stays frame-rate-independent, supports variable speeds, and never drifts.
//   FTimeline still requires Tick to be enabled on the owning Actor – we only
//   enable Tick when a trip is active and disable it when idle (performance).
//
// • bUseRelativeSpace moves the PlatformMesh by its *relative* location so
//   the whole elevator can live under a rotating level-streaming sub-level or
//   a rotating parent Blueprint without the platform drifting in world space.
//
// • When AlphaCurve is null we generate a single-segment linear UCurveFloat
//   at runtime via BuildLinearCurve().  Assign a curve asset in the editor
//   for ease-in / ease-out.

#include "ElevatorActor.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Logging category local to this file
DEFINE_LOG_CATEGORY_STATIC(LogElevator, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AElevatorActor::AElevatorActor()
{
    // Enable Tick – we will enable/disable dynamically at runtime.
    // The timeline MUST be advanced via Tick; disabling Tick stops the timeline.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false; // off until first trip

    // ── Scene root ───────────────────────────────────────────────────────────
    ElevatorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ElevatorRoot"));
    SetRootComponent(ElevatorRoot);

    // ── Platform mesh ────────────────────────────────────────────────────────
    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(ElevatorRoot);
    PlatformMesh->SetMobility(EComponentMobility::Movable);

    // Collision: the platform itself should block Pawns so the character stands
    // on it; keep defaults (BlockAll) and let the designer refine in the editor.
    PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // ── Trigger box ──────────────────────────────────────────────────────────
    TriggerZone = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerZone"));
    TriggerZone->SetupAttachment(PlatformMesh);
    TriggerZone->SetBoxExtent(FVector(100.f, 100.f, 50.f));

    // Query only – no physical impact.
    TriggerZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerZone->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    TriggerZone->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
    TriggerZone->SetGenerateOverlapEvents(true);

    // ── Button mesh (decorative) ──────────────────────────────────────────────
    ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
    ButtonMesh->SetupAttachment(PlatformMesh);
    ButtonMesh->SetMobility(EComponentMobility::Movable);
    // No collision needed on the button visual.
    ButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::BeginPlay()
{
    Super::BeginPlay();

    // ── Cache start / target locations ────────────────────────────────────────
    if (bUseRelativeSpace)
    {
        // Store the PlatformMesh relative location (relative to ElevatorRoot).
        // When the parent rotates, these vectors rotate with it – correct.
        StartLocation  = PlatformMesh->GetRelativeLocation();
        TargetLocation = StartLocation + FVector(0.f, 0.f, MoveHeight);
    }
    else
    {
        // Absolute world-space positions.
        StartLocation  = GetActorLocation();
        TargetLocation = StartLocation + FVector(0.f, 0.f, MoveHeight);
    }

    if (bDebugLog)
    {
        UE_LOG(LogElevator, Log, TEXT("[%s] BeginPlay – Start=%s  Target=%s  bRelative=%s"),
            *GetName(),
            *StartLocation.ToString(),
            *TargetLocation.ToString(),
            bUseRelativeSpace ? TEXT("true") : TEXT("false"));
    }

    // ── Ensure a valid curve exists ───────────────────────────────────────────
    if (!AlphaCurve)
    {
        AlphaCurve = BuildLinearCurve();
        UE_LOG(LogElevator, Warning,
            TEXT("[%s] No AlphaCurve assigned – using generated linear curve. "
                 "Assign a UCurveFloat asset in the editor for ease-in/out."),
            *GetName());
    }

    // ── Wire the FTimeline ────────────────────────────────────────────────────
    // Bind the per-frame update callback.
    FOnTimelineFloat UpdateDelegate;
    UpdateDelegate.BindUFunction(this, FName("OnTimelineUpdate"));
    MoveTimeline.AddInterpFloat(AlphaCurve, UpdateDelegate);

    // Bind the finished callback.
    FOnTimelineEvent FinishedDelegate;
    FinishedDelegate.BindUFunction(this, FName("OnTimelineFinished"));
    MoveTimeline.SetTimelineFinishedFunc(FinishedDelegate);

    // Scale the timeline playback rate so the trip always takes MoveDuration
    // seconds regardless of the curve's key range (which is typically 0..1).
    // PlayRate = CurveLength / MoveDuration  →  trip takes MoveDuration seconds.
    float CurveMinTime = 0.f;
    float CurveMaxTime = 1.f;
    AlphaCurve->GetTimeRange(CurveMinTime, CurveMaxTime);
    const float CurveLength  = CurveMaxTime - CurveMinTime;
    if (MoveDuration > KINDA_SMALL_NUMBER && CurveLength > KINDA_SMALL_NUMBER)
    {
        MoveTimeline.SetPlayRate(CurveLength / MoveDuration);
    }

    MoveTimeline.SetLooping(false);

    // ── Bind overlap delegate ─────────────────────────────────────────────────
    TriggerZone->OnComponentBeginOverlap.AddDynamic(
        this, &AElevatorActor::OnTriggerOverlapBegin);

    // ── Auto-start ────────────────────────────────────────────────────────────
    if (bAutoStartOnBeginPlay)
    {
        bCanTrigger = false;
        SetActorTickEnabled(true);
        MoveTimeline.Play();

        if (bDebugLog)
            UE_LOG(LogElevator, Log, TEXT("[%s] AutoStart – rising on BeginPlay."), *GetName());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick  –  advances the FTimeline (only runs while a trip is active)
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    MoveTimeline.TickTimeline(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTriggerOverlapBegin
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::OnTriggerOverlapBegin(
    UPrimitiveComponent* /*OverlappedComponent*/,
    AActor*              OtherActor,
    UPrimitiveComponent* /*OtherComp*/,
    int32                /*OtherBodyIndex*/,
    bool                 /*bFromSweep*/,
    const FHitResult&    /*SweepResult*/)
{
    // Must be the local player character.
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character)
        return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || PC->GetPawn() != Character)
        return;

    RequestToggle(Character);
}

// ─────────────────────────────────────────────────────────────────────────────
// RequestToggle  –  public remote entry point (also used by built-in overlap)
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::RequestToggle(ACharacter* InstigatorCharacter)
{
    // ── Guard: cooldown / in-flight ───────────────────────────────────────────
    if (!bCanTrigger)
    {
        if (bDebugLog)
            UE_LOG(LogElevator, Log, TEXT("[%s] RequestToggle blocked – cooldown active."), *GetName());
        return;
    }

    if (bDebugLog)
        UE_LOG(LogElevator, Log, TEXT("[%s] RequestToggle – bAtTop=%s  instigator=%s"),
            *GetName(),
            bAtTop ? TEXT("true") : TEXT("false"),
            InstigatorCharacter ? *InstigatorCharacter->GetName() : TEXT("none"));

    // ── Lock trigger ──────────────────────────────────────────────────────────
    bCanTrigger = false;

    // ── Enable Tick so the timeline advances ──────────────────────────────────
    SetActorTickEnabled(true);

    // ── Play forward or reverse ───────────────────────────────────────────────
    if (!bAtTop)
    {
        MoveTimeline.Play();
    }
    else
    {
        MoveTimeline.Reverse();
    }

    // ── Optional character attachment ─────────────────────────────────────────
    if (bAttachCharacterWhileMoving && IsValid(InstigatorCharacter))
    {
        AttachedCharacter = InstigatorCharacter;

        FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, false);
        InstigatorCharacter->AttachToComponent(PlatformMesh, AttachRules);

        if (bDebugLog)
            UE_LOG(LogElevator, Log, TEXT("[%s] Attached %s to platform."),
                *GetName(), *InstigatorCharacter->GetName());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTimelineUpdate  –  alpha in [0..1] delivered by the FTimeline each tick
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::OnTimelineUpdate(float Alpha)
{
    ApplyAlpha(Alpha);
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyAlpha  –  translate alpha → world / relative position
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::ApplyAlpha(float Alpha)
{
    const FVector NewLocation = FMath::Lerp(StartLocation, TargetLocation, Alpha);

    if (bUseRelativeSpace)
    {
        // Move the PlatformMesh relative to ElevatorRoot.
        // When ElevatorRoot rotates (because the owning level rotates) the
        // relative Z axis rotates too, so the platform still goes "up" in the
        // level's local frame – correct for rotating sub-levels.
        PlatformMesh->SetRelativeLocation(NewLocation);
    }
    else
    {
        // Absolute world position.  TeleportPhysics skips collision sweeping so
        // the platform is never blocked by static geometry mid-trip.
        SetActorLocation(NewLocation, /*bSweep=*/false, /*OutHit=*/nullptr,
                         ETeleportType::TeleportPhysics);
    }

    if (bDebugLog)
    {
        UE_LOG(LogElevator, Verbose,
            TEXT("[%s] TimelineUpdate  Alpha=%.3f  Loc=%s"),
            *GetName(), Alpha, *NewLocation.ToString());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTimelineFinished
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::OnTimelineFinished()
{
    // Snap to the exact endpoint to avoid floating-point accumulation.
    ApplyAlpha(bAtTop ? 0.f : 1.f);  // Before flip: bAtTop reflects OLD state.

    // Flip state.
    bAtTop = !bAtTop;

    if (bDebugLog)
        UE_LOG(LogElevator, Log, TEXT("[%s] Timeline finished – bAtTop now %s"),
            *GetName(), bAtTop ? TEXT("true") : TEXT("false"));

    // ── Detach character ──────────────────────────────────────────────────────
    if (bAttachCharacterWhileMoving && IsValid(AttachedCharacter))
    {
        FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, false);
        AttachedCharacter->DetachFromActor(DetachRules);

        if (bDebugLog)
            UE_LOG(LogElevator, Log, TEXT("[%s] Detached %s from platform."),
                *GetName(), *AttachedCharacter->GetName());

        AttachedCharacter = nullptr;
    }

    // ── Stop Tick until next trip ─────────────────────────────────────────────
    SetActorTickEnabled(false);

    // ── Start cooldown ────────────────────────────────────────────────────────
    GetWorldTimerManager().SetTimer(
        CooldownTimerHandle,
        this,
        &AElevatorActor::ResetTrigger,
        CooldownSeconds,
        /*bLooping=*/false);
}

// ─────────────────────────────────────────────────────────────────────────────
// ResetTrigger  –  called by the cooldown timer
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorActor::ResetTrigger()
{
    bCanTrigger = true;

    if (bDebugLog)
        UE_LOG(LogElevator, Log, TEXT("[%s] Cooldown expired – trigger re-enabled."), *GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildLinearCurve  –  runtime-generated linear 0→1 UCurveFloat fallback
// ─────────────────────────────────────────────────────────────────────────────

UCurveFloat* AElevatorActor::BuildLinearCurve()
{
    // NewObject with the Actor as outer so it lives as long as we do.
    UCurveFloat* Curve = NewObject<UCurveFloat>(this, TEXT("GeneratedLinearCurve"));

    // Two keys: (0,0) and (1,1) with linear tangents.
    FRichCurveKey KeyStart(0.f, 0.f);
    KeyStart.InterpMode = RCIM_Linear;

    FRichCurveKey KeyEnd(1.f, 1.f);
    KeyEnd.InterpMode = RCIM_Linear;

    Curve->FloatCurve.Keys.Add(KeyStart);
    Curve->FloatCurve.Keys.Add(KeyEnd);

    return Curve;
}
