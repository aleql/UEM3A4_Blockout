// OneWayDoubleDoor.cpp
// One-way, one-shot double door – Option A: external StaticMeshActors.
//
// Design notes
// ─────────────
// • No door mesh lives inside this actor. LeftDoorActor and RightDoorActor are
//   AStaticMeshActor instances placed separately in the level and referenced
//   via EditInstanceOnly properties. This keeps level streaming and asset
//   management fully in the designer's hands.
//
// • Option A rotates each actor directly via SetActorRotation. The actor's
//   world origin IS the rotation pivot, so each door leaf's origin must be
//   placed at its physical hinge edge in the level editor. If the pivot is
//   centred the leaf will orbit around its centre rather than swing – there is
//   no hinge-offset correction in this option by design.
//
// • Mobility is validated at BeginPlay. If a leaf's RootComponent is not
//   Movable we force it to Movable and log a warning, rather than silently
//   failing mid-animation.
//
// • AllowedDirection falls back to the DirectionArrow world forward if the
//   designer leaves it at FVector::ZeroVector (recommended workflow).
//
// • FTimeline drives the animation. Tick is off by default and enabled only
//   for the duration of the open animation, then disabled permanently.
//
// • After opening, the overlap delegate is unbound and TriggerZone collision
//   is disabled – zero runtime cost once the door has opened.

#include "OneWayDoubleDoor.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogDoubleDoor, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AOneWayDoubleDoor::AOneWayDoubleDoor()
{
    PrimaryActorTick.bCanEverTick          = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // ── Root ─────────────────────────────────────────────────────────────────
    DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
    SetRootComponent(DoorRoot);

    // ── Trigger box ──────────────────────────────────────────────────────────
    TriggerZone = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerZone"));
    TriggerZone->SetupAttachment(DoorRoot);
    TriggerZone->SetBoxExtent(FVector(120.f, 120.f, 90.f));

    TriggerZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerZone->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    TriggerZone->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
    TriggerZone->SetGenerateOverlapEvents(true);

    // ── Direction arrow (editor-only visualiser) ──────────────────────────────
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(DoorRoot);
    DirectionArrow->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
    DirectionArrow->ArrowColor    = FColor::Green;
    DirectionArrow->ArrowSize     = 2.0f;
    DirectionArrow->bHiddenInGame = true;
    DirectionArrow->SetVisibility(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AOneWayDoubleDoor::BeginPlay()
{
    Super::BeginPlay();

    // ── Validate door actor references ────────────────────────────────────────
    bActorsValid = true;

    if (!IsValid(LeftDoorActor))
    {
        UE_LOG(LogDoubleDoor, Warning,
            TEXT("[%s] LeftDoorActor is not assigned. Assign it in the Details panel. "
                 "Door will not open."), *GetName());
        bActorsValid = false;
    }

    if (!IsValid(RightDoorActor))
    {
        UE_LOG(LogDoubleDoor, Warning,
            TEXT("[%s] RightDoorActor is not assigned. Assign it in the Details panel. "
                 "Door will not open."), *GetName());
        bActorsValid = false;
    }

    if (!bActorsValid)
        return; // Skip all further setup – overlap delegate not bound.

    // ── Mobility enforcement ──────────────────────────────────────────────────
    // SetActorRotation only moves actors with Movable root components.
    // Force Movable and warn so the designer knows the level needs saving.
    auto EnforceMobility = [this](AStaticMeshActor* DoorActor, const TCHAR* Side)
    {
        UStaticMeshComponent* Mesh = DoorActor->GetStaticMeshComponent();
        if (!Mesh)
            return;

        if (Mesh->Mobility != EComponentMobility::Movable)
        {
            UE_LOG(LogDoubleDoor, Warning,
                TEXT("[%s] %s door actor '%s' StaticMeshComponent is not Movable. "
                     "Forcing Movable now. Re-save the level to persist this change."),
                *GetName(), Side, *DoorActor->GetName());

            Mesh->SetMobility(EComponentMobility::Movable);
        }
    };

    EnforceMobility(LeftDoorActor,  TEXT("Left"));
    EnforceMobility(RightDoorActor, TEXT("Right"));

    // ── Cache initial (closed) world rotations ────────────────────────────────
    LeftStartRotation  = LeftDoorActor->GetActorRotation();
    RightStartRotation = RightDoorActor->GetActorRotation();

    if (bDebugLog)
    {
        UE_LOG(LogDoubleDoor, Log,
            TEXT("[%s] LeftStartRotation=%s  RightStartRotation=%s"),
            *GetName(),
            *LeftStartRotation.ToString(),
            *RightStartRotation.ToString());
    }

    // ── Resolve allowed direction ─────────────────────────────────────────────
    if (!AllowedDirection.IsNearlyZero())
    {
        ResolvedAllowedDirection = AllowedDirection.GetSafeNormal();
    }
    else
    {
        ResolvedAllowedDirection = DirectionArrow->GetForwardVector();
    }

    if (bDebugLog)
        UE_LOG(LogDoubleDoor, Log,
            TEXT("[%s] AllowedDirection resolved to %s"),
            *GetName(), *ResolvedAllowedDirection.ToString());

    // ── Build curve ───────────────────────────────────────────────────────────
    if (!OpenCurve)
    {
        OpenCurve = BuildLinearCurve();
        UE_LOG(LogDoubleDoor, Warning,
            TEXT("[%s] No OpenCurve assigned – using generated linear curve."), *GetName());
    }

    // ── Wire FTimeline ────────────────────────────────────────────────────────
    FOnTimelineFloat UpdateDelegate;
    UpdateDelegate.BindUFunction(this, FName("OnTimelineUpdate"));
    OpenTimeline.AddInterpFloat(OpenCurve, UpdateDelegate);

    FOnTimelineEvent FinishedDelegate;
    FinishedDelegate.BindUFunction(this, FName("OnTimelineFinished"));
    OpenTimeline.SetTimelineFinishedFunc(FinishedDelegate);

    float CurveMin = 0.f, CurveMax = 1.f;
    OpenCurve->GetTimeRange(CurveMin, CurveMax);
    const float CurveLength = CurveMax - CurveMin;
    if (OpenDuration > KINDA_SMALL_NUMBER && CurveLength > KINDA_SMALL_NUMBER)
    {
        OpenTimeline.SetPlayRate(CurveLength / OpenDuration);
    }

    OpenTimeline.SetLooping(false);

    // ── Bind overlap ──────────────────────────────────────────────────────────
    TriggerZone->OnComponentBeginOverlap.AddDynamic(
        this, &AOneWayDoubleDoor::OnTriggerOverlapBegin);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void AOneWayDoubleDoor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    OpenTimeline.TickTimeline(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTriggerOverlapBegin
// ─────────────────────────────────────────────────────────────────────────────

void AOneWayDoubleDoor::OnTriggerOverlapBegin(
    UPrimitiveComponent* /*OverlappedComponent*/,
    AActor*              OtherActor,
    UPrimitiveComponent* /*OtherComp*/,
    int32                /*OtherBodyIndex*/,
    bool                 /*bFromSweep*/,
    const FHitResult&    /*SweepResult*/)
{
    if (bDebugLog)
        UE_LOG(LogDoubleDoor, Log, TEXT("[%s] Overlap detected from %s"),
            *GetName(), *GetNameSafe(OtherActor));

    // ── One-shot latch ────────────────────────────────────────────────────────
    if (bHasOpened)
        return;

    // ── Actors must still be valid ────────────────────────────────────────────
    if (!bActorsValid || !IsValid(LeftDoorActor) || !IsValid(RightDoorActor))
    {
        UE_LOG(LogDoubleDoor, Warning,
            TEXT("[%s] Door actor references are invalid at overlap time – ignoring."),
            *GetName());
        return;
    }

    // ── Must be a Pawn ────────────────────────────────────────────────────────
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
        return;

    // Restrict to the local player only.
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || PC->GetPawn() != Pawn)
        return;

    // ── Side check ────────────────────────────────────────────────────────────
    const FVector ToPlayer = (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    const float   Dot      = FVector::DotProduct(ToPlayer, ResolvedAllowedDirection);

    if (bDebugLog)
        UE_LOG(LogDoubleDoor, Log,
            TEXT("[%s] Dot=%.3f  Threshold=%.3f  %s"),
            *GetName(), Dot, SideDotThreshold,
            Dot >= SideDotThreshold ? TEXT("PASS") : TEXT("FAIL – wrong side"));

    if (Dot < SideDotThreshold)
        return;

    // ── Latch, unbind, disable collision ─────────────────────────────────────
    bHasOpened = true;

    TriggerZone->OnComponentBeginOverlap.RemoveDynamic(
        this, &AOneWayDoubleDoor::OnTriggerOverlapBegin);

    TriggerZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (bDebugLog)
        UE_LOG(LogDoubleDoor, Log, TEXT("[%s] Opening door."), *GetName());

    // ── Start animation ───────────────────────────────────────────────────────
    SetActorTickEnabled(true);
    OpenTimeline.PlayFromStart();
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTimelineUpdate
// ─────────────────────────────────────────────────────────────────────────────

void AOneWayDoubleDoor::OnTimelineUpdate(float Alpha)
{
    // Guard: actors may have been deleted after the door started opening.
    if (!IsValid(LeftDoorActor) || !IsValid(RightDoorActor))
    {
        OpenTimeline.Stop();
        SetActorTickEnabled(false);
        UE_LOG(LogDoubleDoor, Warning,
            TEXT("[%s] Door actor became invalid during open animation – stopping."),
            *GetName());
        return;
    }

    // Left leaf: swing outward by +OpenAngleDegrees * Alpha from its start yaw.
    const FRotator NewLeftRot(
        LeftStartRotation.Pitch,
        LeftStartRotation.Yaw + OpenAngleDegrees * Alpha,
        LeftStartRotation.Roll);

    // Right leaf: swing outward by -OpenAngleDegrees * Alpha from its start yaw.
    const FRotator NewRightRot(
        RightStartRotation.Pitch,
        RightStartRotation.Yaw - OpenAngleDegrees * Alpha,
        RightStartRotation.Roll);

    LeftDoorActor->SetActorRotation(NewLeftRot);
    RightDoorActor->SetActorRotation(NewRightRot);

    if (bDebugLog)
        UE_LOG(LogDoubleDoor, Verbose,
            TEXT("[%s] Open update  Alpha=%.3f  LeftYaw=%.2f  RightYaw=%.2f"),
            *GetName(), Alpha, NewLeftRot.Yaw, NewRightRot.Yaw);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTimelineFinished
// ─────────────────────────────────────────────────────────────────────────────

void AOneWayDoubleDoor::OnTimelineFinished()
{
    // Snap to exact final rotation to eliminate floating-point accumulation.
    if (IsValid(LeftDoorActor))
    {
        LeftDoorActor->SetActorRotation(FRotator(
            LeftStartRotation.Pitch,
            LeftStartRotation.Yaw + OpenAngleDegrees,
            LeftStartRotation.Roll));
    }

    if (IsValid(RightDoorActor))
    {
        RightDoorActor->SetActorRotation(FRotator(
            RightStartRotation.Pitch,
            RightStartRotation.Yaw - OpenAngleDegrees,
            RightStartRotation.Roll));
    }

    // Done forever.
    SetActorTickEnabled(false);

    if (bDebugLog)
        UE_LOG(LogDoubleDoor, Log, TEXT("[%s] Door fully open."), *GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildLinearCurve
// ─────────────────────────────────────────────────────────────────────────────

UCurveFloat* AOneWayDoubleDoor::BuildLinearCurve()
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
