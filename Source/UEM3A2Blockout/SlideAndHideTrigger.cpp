// SlideAndHideTrigger.cpp
// One-shot trigger: slides a target actor along the X axis.
//
// Design notes
// ─────────────
// • Identical FTimeline pattern to RaiseCylinderTrigger; only the axis differs.
// • Movement is in world-space X. If you need actor-local X, rotate the target
//   actor so its local X aligns with the desired world direction.
// • bHasTriggered latched immediately on first valid overlap; delegate unbound
//   for zero runtime cost afterward.

#include "SlideAndHideTrigger.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlideAndHideTrigger, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ASlideAndHideTrigger::ASlideAndHideTrigger()
{
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // enabled only while sliding

	// ── Root ─────────────────────────────────────────────────────────────────
	TriggerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TriggerRoot"));
	SetRootComponent(TriggerRoot);

	// ── Trigger box ──────────────────────────────────────────────────────────
	TriggerZone = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerZone"));
	TriggerZone->SetupAttachment(TriggerRoot);
	TriggerZone->SetBoxExtent(FVector(100.f, 100.f, 50.f));

	TriggerZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerZone->SetGenerateOverlapEvents(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ASlideAndHideTrigger::BeginPlay()
{
	Super::BeginPlay();

	// ── Ensure we have a valid curve ──────────────────────────────────────────
	if (!AlphaCurve)
	{
		AlphaCurve = BuildLinearCurve();
		UE_LOG(LogSlideAndHideTrigger, Warning,
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

	float CurveMinTime = 0.f;
	float CurveMaxTime = 1.f;
	AlphaCurve->GetTimeRange(CurveMinTime, CurveMaxTime);
	const float CurveLength = CurveMaxTime - CurveMinTime;
	if (MoveDuration > KINDA_SMALL_NUMBER && CurveLength > KINDA_SMALL_NUMBER)
	{
		MoveTimeline.SetPlayRate(CurveLength / MoveDuration);
	}

	MoveTimeline.SetLooping(false);

	// ── Bind overlap ──────────────────────────────────────────────────────────
	TriggerZone->OnComponentBeginOverlap.AddDynamic(
		this, &ASlideAndHideTrigger::OnTriggerOverlapBegin);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void ASlideAndHideTrigger::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	MoveTimeline.TickTimeline(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTriggerOverlapBegin
// ─────────────────────────────────────────────────────────────────────────────

void ASlideAndHideTrigger::OnTriggerOverlapBegin(
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
		UE_LOG(LogSlideAndHideTrigger, Warning,
			TEXT("[%s] TargetActor is null – assign it in the Details panel. Aborting."),
			*GetName());
		return;
	}

	// ── Latch immediately so no re-entry is possible ──────────────────────────
	bHasTriggered = true;
	bIsMoving     = true;

	TriggerZone->OnComponentBeginOverlap.RemoveDynamic(
		this, &ASlideAndHideTrigger::OnTriggerOverlapBegin);

	// ── Cache world-space start / target ──────────────────────────────────────
	StartLocation = TargetActor->GetActorLocation();

	const float XOffset = (MoveDirection == ESlideDirection::PositiveX)
		? MoveDistance : -MoveDistance;
	TargetLocation = StartLocation + FVector(XOffset, 0.f, 0.f);

	if (bDebugLog)
	{
		UE_LOG(LogSlideAndHideTrigger, Log,
			TEXT("[%s] Triggered by '%s' – sliding '%s' from %s to %s over %.2fs."),
			*GetName(),
			*Character->GetName(),
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

void ASlideAndHideTrigger::OnTimelineUpdate(float Alpha)
{
	if (!IsValid(TargetActor))
	{
		MoveTimeline.Stop();
		SetActorTickEnabled(false);
		bIsMoving = false;
		UE_LOG(LogSlideAndHideTrigger, Warning,
			TEXT("[%s] TargetActor became invalid during slide – stopping."), *GetName());
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
		UE_LOG(LogSlideAndHideTrigger, Verbose,
			TEXT("[%s] Slide update  Alpha=%.3f  Loc=%s"),
			*GetName(), Alpha, *NewLocation.ToString());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTimelineFinished
// ─────────────────────────────────────────────────────────────────────────────

void ASlideAndHideTrigger::OnTimelineFinished()
{
	bIsMoving = false;
	SetActorTickEnabled(false);

	if (!IsValid(TargetActor))
		return;

	// Snap to exact target position
	TargetActor->SetActorLocation(
		TargetLocation,
		/*bSweep=*/false,
		/*OutHit=*/nullptr,
		ETeleportType::TeleportPhysics);

	if (bDebugLog)
	{
		UE_LOG(LogSlideAndHideTrigger, Log,
			TEXT("[%s] Slide complete – '%s' is now at %s."),
			*GetName(), *TargetActor->GetName(), *TargetLocation.ToString());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildLinearCurve
// ─────────────────────────────────────────────────────────────────────────────

UCurveFloat* ASlideAndHideTrigger::BuildLinearCurve()
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
