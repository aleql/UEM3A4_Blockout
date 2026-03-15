// TwoPointSplinePatrolComponent.cpp
#include "TwoPointSplinePatrolComponent.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UTwoPointSplinePatrolComponent::UTwoPointSplinePatrolComponent()
{
	// Tick enabled only while patrolling. Toggled by Start/StopPatrol.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void UTwoPointSplinePatrolComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerActor = GetOwner();

	if (bDisablePatrol)
	{
		return;
	}

	// ---- Spline auto-detection -----------------------------------------

	if (!PatrolSpline && OwnerActor)
	{
		PatrolSpline = OwnerActor->FindComponentByClass<USplineComponent>();
		if (PatrolSpline)
		{
			UE_LOG(LogTemp, Log,
				TEXT("TwoPointSplinePatrolComponent [%s]: Auto-detected PatrolSpline '%s'."),
				*OwnerActor->GetName(), *PatrolSpline->GetName());
		}
	}

	if (!PatrolSpline)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TwoPointSplinePatrolComponent [%s]: No PatrolSpline assigned and none found on "
				"owner. Patrol will not start."),
			*GetOwner()->GetName());
		return;
	}

	if (PatrolSpline->GetNumberOfSplinePoints() < 2)
	{
		UE_LOG(LogTemp, Error,
			TEXT("TwoPointSplinePatrolComponent [%s]: PatrolSpline '%s' has %d point(s); "
				"at least 2 are required."),
			*GetOwner()->GetName(),
			*PatrolSpline->GetName(),
			PatrolSpline->GetNumberOfSplinePoints());
		return;
	}

	CacheSplinePoints();

	// ---- Flipbook auto-detection ----------------------------------------

	ResolveFlipbookComponent();

	// ---- Auto-start -----------------------------------------------------

	if (!bStartPaused)
	{
		if (StartDelay > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				StartDelayTimerHandle,
				this,
				&UTwoPointSplinePatrolComponent::StartPatrol,
				StartDelay,
				/*bLooping=*/false);
		}
		else
		{
			StartPatrol();
		}
	}
}

// ---------------------------------------------------------------------------
// EndPlay
// ---------------------------------------------------------------------------

void UTwoPointSplinePatrolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitTimerHandle);
		World->GetTimerManager().ClearTimer(ResumeAfterCombatTimer);
		World->GetTimerManager().ClearTimer(StartDelayTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// TickComponent
// ---------------------------------------------------------------------------

void UTwoPointSplinePatrolComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Gate 1: patrol must be desired
	if (!bWantsToPatrol)
	{
		return;
	}

	// Gate 2: combat override blocks movement (but keeps tick alive so debug
	// drawing still works and state can be inspected)
	if (bCombatOverrideEnabled && IsCombatActive())
	{
		// Draw debug even during combat so endpoints remain visible
#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			DrawDebugSphere(GetWorld(), CachedPointA, 25.0f, 8, FColor::Green,  false, -1.0f, 0, 2.0f);
			DrawDebugSphere(GetWorld(), CachedPointB, 25.0f, 8, FColor::Red,    false, -1.0f, 0, 2.0f);
			DrawDebugLine(GetWorld(), CachedPointA, CachedPointB, FColor::Orange, false, -1.0f, 0, 1.5f);
		}
#endif
		return;
	}

	// Gate 3: owner must be valid
	if (!OwnerActor || !IsSplineValid())
	{
		return;
	}

	// Gate 4: endpoint wait
	if (bIsWaiting)
	{
		return;
	}

	// Refresh cached positions for moving splines
	if (bUseSplineLocalSpace)
	{
		CacheSplinePoints();
	}

	const FVector CurrentLocation = OwnerActor->GetActorLocation();
	const FVector TargetPos       = GetCurrentTargetPosition();
	const FVector ToTarget        = TargetPos - CurrentLocation;
	const float   DistToTarget    = ToTarget.Size();

	// ---- Arrival -------------------------------------------------------
	if (DistToTarget <= ArriveDistance)
	{
		OwnerActor->SetActorLocation(TargetPos);
		BeginWaitAtEnd();   // switches to Idle flipbook inside
		return;
	}

	// ---- Movement -------------------------------------------------------
	const float   StepSize = FMath::Min(MoveSpeed * DeltaTime, DistToTarget);
	const FVector MoveDir  = ToTarget.GetSafeNormal();
	const FVector NewLoc   = CurrentLocation + MoveDir * StepSize;

	OwnerActor->SetActorLocation(NewLoc);
	UpdateFlipbookForMoving();

	// ---- Rotation (yaw-only) -------------------------------------------
	if (bRotateToMovementYaw && !MoveDir.IsNearlyZero())
	{
		const FRotator TargetRot(0.0f, MoveDir.Rotation().Yaw, 0.0f);

		if (RotationInterpSpeed > 0.0f)
		{
			const FRotator Smoothed = FMath::RInterpTo(
				OwnerActor->GetActorRotation(), TargetRot, DeltaTime, RotationInterpSpeed);
			OwnerActor->SetActorRotation(FRotator(0.0f, Smoothed.Yaw, 0.0f));
		}
		else
		{
			OwnerActor->SetActorRotation(TargetRot);
		}
	}

	// ---- Debug ---------------------------------------------------------
#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), CachedPointA, 25.0f, 8, FColor::Green,  false, -1.0f, 0, 2.0f);
		DrawDebugSphere(GetWorld(), CachedPointB, 25.0f, 8, FColor::Red,    false, -1.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), CachedPointA, CachedPointB, FColor::Yellow, false, -1.0f, 0, 1.5f);
		DrawDebugDirectionalArrow(GetWorld(), NewLoc, TargetPos, 20.0f, FColor::Cyan, false, -1.0f, 0, 2.0f);
	}
#endif
}

// ---------------------------------------------------------------------------
// Public API – Patrol
// ---------------------------------------------------------------------------

void UTwoPointSplinePatrolComponent::StartPatrol()
{
	if (!IsSplineValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TwoPointSplinePatrolComponent [%s]: StartPatrol() – spline is not valid."),
			*GetOwner()->GetName());
		return;
	}

	CacheSplinePoints();

	bWantsToPatrol = true;
	bIsWaiting     = false;

	SetComponentTickEnabled(true);
}

void UTwoPointSplinePatrolComponent::StopPatrol()
{
	bWantsToPatrol = false;
	bIsWaiting     = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitTimerHandle);
		World->GetTimerManager().ClearTimer(ResumeAfterCombatTimer);
		World->GetTimerManager().ClearTimer(StartDelayTimerHandle);
	}

	bCombatActive    = false;
	bInCombatCooldown = false;

	SetComponentTickEnabled(false);
	UpdateFlipbookForIdle();
}

void UTwoPointSplinePatrolComponent::SetPatrolSpline(USplineComponent* NewSpline)
{
	PatrolSpline = NewSpline;

	if (!PatrolSpline)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TwoPointSplinePatrolComponent [%s]: SetPatrolSpline() – null spline."),
			*GetOwner()->GetName());
		return;
	}

	if (PatrolSpline->GetNumberOfSplinePoints() < 2)
	{
		UE_LOG(LogTemp, Error,
			TEXT("TwoPointSplinePatrolComponent [%s]: New spline '%s' has fewer than 2 points."),
			*GetOwner()->GetName(), *PatrolSpline->GetName());
		return;
	}

	CacheSplinePoints();
}

void UTwoPointSplinePatrolComponent::ForceGoToPoint(int32 PointIndex)
{
	if (PointIndex != 0 && PointIndex != 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TwoPointSplinePatrolComponent::ForceGoToPoint: index %d is invalid. Use 0 or 1."),
			PointIndex);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitTimerHandle);
	}

	CurrentTargetIndex = PointIndex;
	bIsWaiting         = false;
	bWantsToPatrol     = true;

	SetComponentTickEnabled(true);
}

bool UTwoPointSplinePatrolComponent::IsPatrolling() const
{
	return bWantsToPatrol && !IsCombatActive() && !bIsWaiting;
}

FVector UTwoPointSplinePatrolComponent::GetPatrolPointWorldLocation(int32 PointIndex) const
{
	if (PointIndex == 0) return CachedPointA;
	if (PointIndex == 1) return CachedPointB;

	UE_LOG(LogTemp, Warning,
		TEXT("TwoPointSplinePatrolComponent::GetPatrolPointWorldLocation: index %d is invalid "
			"(use 0 or 1). Returning ZeroVector."),
		PointIndex);
	return FVector::ZeroVector;
}

void UTwoPointSplinePatrolComponent::ResumePatrolFromStartPoint()
{
	if (!IsSplineValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TwoPointSplinePatrolComponent [%s]: ResumePatrolFromStartPoint() – spline is not valid."),
			*GetOwner()->GetName());
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResumeAfterCombatTimer);
		World->GetTimerManager().ClearTimer(WaitTimerHandle);
	}

	bCombatActive     = false;
	bInCombatCooldown = false;
	bIsWaiting        = false;

	// Head from A toward B
	CurrentTargetIndex = 1;

	// Clear dedup so patrol can immediately apply WalkFlipbook on the first tick
	LastSetFlipbook = nullptr;

	bWantsToPatrol = true;
	SetComponentTickEnabled(true);
}

// ---------------------------------------------------------------------------
// Public API – Combat Override
// ---------------------------------------------------------------------------

void UTwoPointSplinePatrolComponent::NotifyCombatStarted()
{
	if (!bCombatOverrideEnabled)
	{
		return;
	}

	// Cancel any in-flight resume countdown – player is back in range
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResumeAfterCombatTimer);
		// Also cancel any endpoint wait; movement is paused for a different reason
		World->GetTimerManager().ClearTimer(WaitTimerHandle);
	}

	bCombatActive     = true;
	bInCombatCooldown = false;
	bIsWaiting        = false;

	// Switch to idle only when the patrol component owns the animation state
	if (bDriveFlipbooks && !bCombatOwnsAnimations)
	{
		UpdateFlipbookForIdle();
	}
}

void UTwoPointSplinePatrolComponent::NotifyCombatEnded()
{
	if (!bCombatOverrideEnabled)
	{
		return;
	}

	bCombatActive = false;

	// Reset any leftover cooldown and start the fresh countdown
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResumeAfterCombatTimer);
	}

	if (ResumeDelayAfterOutOfRange > 0.0f)
	{
		bInCombatCooldown = true;

		GetWorld()->GetTimerManager().SetTimer(
			ResumeAfterCombatTimer,
			this,
			&UTwoPointSplinePatrolComponent::OnResumeAfterCombat,
			ResumeDelayAfterOutOfRange,
			/*bLooping=*/false);
	}
	else
	{
		// Zero delay – resume immediately
		OnResumeAfterCombat();
	}
}

bool UTwoPointSplinePatrolComponent::IsCombatActive() const
{
	// API flags take priority
	if (bCombatActive || bInCombatCooldown)
	{
		return true;
	}

	// Method B tag fallback: read an actor tag each tick
	if (OwnerActor && !PlayerInRangeTag.IsNone())
	{
		return OwnerActor->ActorHasTag(PlayerInRangeTag);
	}

	return false;
}

// ---------------------------------------------------------------------------
// Private – Spline helpers
// ---------------------------------------------------------------------------

bool UTwoPointSplinePatrolComponent::IsSplineValid() const
{
	return PatrolSpline != nullptr && PatrolSpline->GetNumberOfSplinePoints() >= 2;
}

void UTwoPointSplinePatrolComponent::CacheSplinePoints()
{
	if (!IsSplineValid())
	{
		return;
	}

	if (bUseSplineLocalSpace)
	{
		const FTransform SplineWorld = PatrolSpline->GetComponentTransform();

		CachedPointA = SplineWorld.TransformPosition(
			PatrolSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local));

		CachedPointB = SplineWorld.TransformPosition(
			PatrolSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local));
	}
	else
	{
		CachedPointA = PatrolSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
		CachedPointB = PatrolSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
	}
}

FVector UTwoPointSplinePatrolComponent::GetCurrentTargetPosition() const
{
	return (CurrentTargetIndex == 0) ? CachedPointA : CachedPointB;
}

void UTwoPointSplinePatrolComponent::BeginWaitAtEnd()
{
	bIsWaiting = true;
	UpdateFlipbookForIdle();

	if (WaitTimeAtEnds <= 0.0f)
	{
		EndWaitAndSwapTarget();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		WaitTimerHandle,
		this,
		&UTwoPointSplinePatrolComponent::EndWaitAndSwapTarget,
		WaitTimeAtEnds,
		/*bLooping=*/false);
}

void UTwoPointSplinePatrolComponent::EndWaitAndSwapTarget()
{
	bIsWaiting         = false;
	CurrentTargetIndex = (CurrentTargetIndex == 0) ? 1 : 0;
	// Walk flipbook applied on the first moving tick via UpdateFlipbookForMoving()
}

// ---------------------------------------------------------------------------
// Private – Flipbook helpers
// ---------------------------------------------------------------------------

void UTwoPointSplinePatrolComponent::ResolveFlipbookComponent()
{
	if (!bDriveFlipbooks)
	{
		return;
	}

	// Use the explicit override first
	CachedFlipbookComp = FlipbookCompOverride;

	// Otherwise search the owner
	if (!CachedFlipbookComp && OwnerActor)
	{
		CachedFlipbookComp = OwnerActor->FindComponentByClass<UPaperFlipbookComponent>();

		if (CachedFlipbookComp)
		{
			UE_LOG(LogTemp, Log,
				TEXT("TwoPointSplinePatrolComponent [%s]: Auto-detected flipbook component '%s'."),
				*OwnerActor->GetName(), *CachedFlipbookComp->GetName());
		}
		else
		{
			// Log once only; do not repeat in Tick
			UE_LOG(LogTemp, Warning,
				TEXT("TwoPointSplinePatrolComponent [%s]: bDriveFlipbooks is true but no "
					"UPaperFlipbookComponent found on owner. Assign FlipbookCompOverride or "
					"add a UPaperFlipbookComponent to the owner."),
				*GetOwner()->GetName());
		}
	}
}

void UTwoPointSplinePatrolComponent::TrySetFlipbook(UPaperFlipbook* NewFlipbook)
{
	// Bail early for every reason to avoid spam
	if (!bDriveFlipbooks || !CachedFlipbookComp || !NewFlipbook)
	{
		return;
	}

	// Skip the call if this flipbook is already active
	if (NewFlipbook == LastSetFlipbook)
	{
		return;
	}

	CachedFlipbookComp->SetFlipbook(NewFlipbook);
	CachedFlipbookComp->SetLooping(true);
	CachedFlipbookComp->Play();

	LastSetFlipbook = NewFlipbook;
}

void UTwoPointSplinePatrolComponent::UpdateFlipbookForMoving()
{
	TrySetFlipbook(WalkFlipbook);
}

void UTwoPointSplinePatrolComponent::UpdateFlipbookForIdle()
{
	TrySetFlipbook(IdleFlipbook);
}

// ---------------------------------------------------------------------------
// Private – Combat resume timer callback
// ---------------------------------------------------------------------------

void UTwoPointSplinePatrolComponent::OnResumeAfterCombat()
{
	// Clear the cooldown flag; IsCombatActive() can now return false
	bInCombatCooldown = false;

	// Guard: player might have re-entered range exactly as the timer fired
	if (IsCombatActive())
	{
		return;
	}

	// Resume movement if patrol is still desired
	if (bWantsToPatrol)
	{
		// Determine correct initial flipbook based on current state
		if (bIsWaiting)
		{
			UpdateFlipbookForIdle();
		}
		else
		{
			UpdateFlipbookForMoving();
		}
		// Tick is already enabled; movement continues on the next frame
	}
}
