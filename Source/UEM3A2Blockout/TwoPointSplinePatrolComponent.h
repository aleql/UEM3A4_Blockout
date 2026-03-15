// TwoPointSplinePatrolComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TwoPointSplinePatrolComponent.generated.h"

class USplineComponent;
class UPaperFlipbookComponent;
class UPaperFlipbook;

/**
 * Reusable patrol component: moves any actor between spline points 0 and 1,
 * pauses at each end, drives Walk/Idle flipbooks, and supports a combat
 * override that suspends patrol while the player is in range.
 *
 * ── Combat integration (two methods, both implemented) ───────────────────
 *
 *  Method A – API calls (preferred):
 *    Call NotifyCombatStarted() when the player enters the aggro trigger.
 *    Call NotifyCombatEnded()   when the player leaves.
 *    The component handles the 5-second resume cooldown automatically.
 *
 *  Method B – Actor tags (fallback, no code changes needed):
 *    Add the tag named by PlayerInRangeTag to the owner actor while the
 *    player is in range. IsCombatActive() reads this tag each tick.
 *    Useful when you cannot easily call the API directly.
 *
 *  Both methods can coexist; the API flags take priority.
 *
 * ── Animation integration ────────────────────────────────────────────────
 *
 *  Set bDriveFlipbooks = true, assign WalkFlipbook and IdleFlipbook.
 *  The component auto-detects the first UPaperFlipbookComponent on the
 *  owner unless FlipbookCompOverride is assigned.
 *
 *  If bCombatOwnsAnimations = true the patrol component does NOT switch
 *  to Idle when combat starts; the combat system is expected to own that.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API UTwoPointSplinePatrolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTwoPointSplinePatrolComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// =========================================================
	// Spline
	// =========================================================

	/**
	 * Spline whose points 0 and 1 define the patrol route.
	 * Leave empty to auto-detect the first USplineComponent on the owner.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Spline")
	TObjectPtr<USplineComponent> PatrolSpline;

	/**
	 * When true, endpoint world positions are re-derived from the spline's
	 * local-space points each tick. Needed when the spline is on a moving platform.
	 * Leave false (default) for static splines – positions are cached once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Spline")
	bool bUseSplineLocalSpace = false;

	// =========================================================
	// Movement
	// =========================================================

	/** Constant movement speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Movement", meta = (ClampMin = "1.0"))
	float MoveSpeed = 200.0f;

	/**
	 * Arrival tolerance in cm. When the actor is within this distance of an
	 * endpoint it is considered arrived. Values below 5 can cause jitter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Movement", meta = (ClampMin = "1.0"))
	float ArriveDistance = 10.0f;

	/** If true the actor yaws to face the current movement direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Movement")
	bool bRotateToMovementYaw = true;

	/**
	 * Rotation interpolation speed via RInterpTo. Set to 0 for instant snap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Movement", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 8.0f;

	// =========================================================
	// Endpoint Pausing
	// =========================================================

	/** How long (seconds) to pause at each endpoint before reversing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Pausing", meta = (ClampMin = "0.0"))
	float WaitTimeAtEnds = 1.0f;

	/** If true the patrol does not start automatically in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Pausing")
	bool bStartPaused = false;

	/** If true, patrol is completely disabled: no auto-start, no spline validation,
	 *  no tick. Use this for enemies that should stand still without a patrol route. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Pausing")
	bool bDisablePatrol = false;

	/** Optional delay before auto-start (only when bStartPaused is false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Pausing", meta = (ClampMin = "0.0"))
	float StartDelay = 0.0f;

	// =========================================================
	// Animation (Paper2D)
	// =========================================================

	/**
	 * When true the component sets WalkFlipbook while moving and IdleFlipbook
	 * while waiting at endpoints. Only calls SetFlipbook when the flipbook changes
	 * to avoid redundant calls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Animation")
	bool bDriveFlipbooks = true;

	/**
	 * Override which UPaperFlipbookComponent receives the Walk/Idle flipbooks.
	 * Leave empty to auto-detect the first UPaperFlipbookComponent on the owner.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Animation")
	TObjectPtr<UPaperFlipbookComponent> FlipbookCompOverride;

	/** Flipbook to play while the actor is moving between endpoints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Animation")
	TObjectPtr<UPaperFlipbook> WalkFlipbook;

	/** Flipbook to play while the actor is waiting at an endpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Animation")
	TObjectPtr<UPaperFlipbook> IdleFlipbook;

	// =========================================================
	// Combat Override
	// =========================================================

	/** When true, NotifyCombatStarted/Ended and tag detection suspend patrol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Combat")
	bool bCombatOverrideEnabled = true;

	/**
	 * Seconds the actor must be out of range before patrol resumes.
	 * The player re-entering during this window resets the cooldown.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Combat", meta = (ClampMin = "0.0"))
	float ResumeDelayAfterOutOfRange = 5.0f;

	/**
	 * Actor tag (Method B fallback). When bCombatOverrideEnabled is true and
	 * this tag is present on the owner, IsCombatActive() returns true even if
	 * NotifyCombatStarted() was never called.
	 * Set to NAME_None to disable tag detection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Combat")
	FName PlayerInRangeTag = FName("PlayerInRange");

	/**
	 * When true the patrol component does NOT force-switch to Idle when combat
	 * starts. Use this when the combat/shooting system manages its own animations.
	 * When false, Idle is applied as soon as NotifyCombatStarted() fires.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Combat")
	bool bCombatOwnsAnimations = true;

	// =========================================================
	// Debug
	// =========================================================

	/** Draws patrol path and endpoint spheres each tick. Stripped in shipping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol|Debug")
	bool bDrawDebug = false;

	// =========================================================
	// Public API
	// =========================================================

	/**
	 * Begins (or resumes) the patrol loop.
	 * Sets bWantsToPatrol = true. Combat override can still block movement.
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol")
	void StartPatrol();

	/**
	 * Permanently halts patrol and clears all timers.
	 * Unlike combat override, this sets bWantsToPatrol = false.
	 * Call StartPatrol() to restart.
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol")
	void StopPatrol();

	/** Replaces the patrol spline and re-caches endpoints. Safe to call at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Patrol")
	void SetPatrolSpline(USplineComponent* NewSpline);

	/**
	 * Forces the target to PointIndex (0 or 1), clears the end-wait timer, and
	 * resumes movement. Does not bypass the combat override.
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol")
	void ForceGoToPoint(int32 PointIndex);

	/** Returns true only while actively moving (not waiting, not in combat). */
	UFUNCTION(BlueprintCallable, Category = "Patrol")
	bool IsPatrolling() const;

	/**
	 * Returns the world-space location of patrol point 0 (A) or 1 (B).
	 * Uses cached positions. Returns FVector::ZeroVector for invalid indices.
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol")
	FVector GetPatrolPointWorldLocation(int32 PointIndex) const;

	/**
	 * Immediately resumes patrol from point A heading toward point B.
	 * Clears all combat flags and the resume cooldown timer.
	 * Use this when an enemy has manually returned to its patrol start
	 * and wants to hand movement control back to the component.
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol")
	void ResumePatrolFromStartPoint();

	// ── Combat API ───────────────────────────────────────────

	/**
	 * Method A: call this when the player enters the aggro trigger.
	 * Suspends patrol immediately. If another combat call is already active,
	 * this resets state cleanly (idempotent for active combat).
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol|Combat")
	void NotifyCombatStarted();

	/**
	 * Method A: call this when the player exits the aggro trigger.
	 * Does NOT resume patrol immediately. Starts a ResumeDelayAfterOutOfRange
	 * countdown. If NotifyCombatStarted fires again before the timer expires,
	 * the countdown is cancelled automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol|Combat")
	void NotifyCombatEnded();

	/**
	 * Returns true when combat is active (direct API flag OR cooldown timer
	 * is running OR the owner has the PlayerInRangeTag).
	 */
	UFUNCTION(BlueprintCallable, Category = "Patrol|Combat")
	bool IsCombatActive() const;

protected:
	// =========================================================
	// Internal State
	// =========================================================

	UPROPERTY()
	TObjectPtr<AActor> OwnerActor;

	/** Resolved at BeginPlay from FlipbookCompOverride or owner auto-detect. */
	UPROPERTY()
	TObjectPtr<UPaperFlipbookComponent> CachedFlipbookComp;

	/** Last flipbook we called SetFlipbook with. Prevents redundant calls. */
	UPROPERTY()
	TObjectPtr<UPaperFlipbook> LastSetFlipbook;

	FVector CachedPointA = FVector::ZeroVector;
	FVector CachedPointB = FVector::ZeroVector;

	/** 0 = heading toward A, 1 = heading toward B. Flipped after each end-wait. */
	int32 CurrentTargetIndex = 0;

	/** True while the endpoint pause timer is ticking. Blocks movement tick. */
	bool bIsWaiting = false;

	/**
	 * True between StartPatrol() and StopPatrol().
	 * Combat override does NOT clear this flag; it only blocks movement while active.
	 */
	bool bWantsToPatrol = false;

	/** Set true by NotifyCombatStarted, cleared by NotifyCombatEnded. */
	bool bCombatActive = false;

	/**
	 * Set true by NotifyCombatEnded, cleared when ResumeAfterCombatTimer fires.
	 * Keeps IsCombatActive() returning true during the resume cooldown.
	 */
	bool bInCombatCooldown = false;

	/** Pause at endpoint. Cleared by NotifyCombatStarted. */
	FTimerHandle WaitTimerHandle;

	/** Resume patrol after the out-of-range cooldown. */
	FTimerHandle ResumeAfterCombatTimer;

	/** Optional auto-start delay timer. */
	FTimerHandle StartDelayTimerHandle;

	// =========================================================
	// Private Helpers
	// =========================================================

	bool IsSplineValid() const;
	void CacheSplinePoints();
	FVector GetCurrentTargetPosition() const;
	void BeginWaitAtEnd();
	void EndWaitAndSwapTarget();

	/** Finds and caches the UPaperFlipbookComponent. Logs once on failure. */
	void ResolveFlipbookComponent();

	/**
	 * Sets a new flipbook on CachedFlipbookComp.
	 * No-op when: bDriveFlipbooks is false, comp is missing, flipbook is null,
	 * or the flipbook is already the active one (prevents spam).
	 */
	void TrySetFlipbook(UPaperFlipbook* NewFlipbook);

	/** Switches to WalkFlipbook if appropriate. */
	void UpdateFlipbookForMoving();

	/** Switches to IdleFlipbook if appropriate. */
	void UpdateFlipbookForIdle();

	/** Timer callback fired after ResumeDelayAfterOutOfRange expires. */
	void OnResumeAfterCombat();
};
