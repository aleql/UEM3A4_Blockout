// BillboardMeleeEnemy.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BillboardMeleeEnemy.generated.h"

class UPaperFlipbookComponent;
class UPaperFlipbook;
class USphereComponent;
class UCapsuleComponent;
class UTwoPointSplinePatrolComponent;
class APawn;

/**
 * Enemy state machine states.
 * Patrolling        – patrol component drives movement and Walk/Idle flipbooks.
 * ReturningToStart  – enemy moves back to spline point A under its own control.
 * Chasing           – enemy sprints toward player.
 * Attacking         – locked in an attack, windup then damage applied.
 * Damaged           – brief stun after taking a hit.
 * Dying             – death animation, optional destroy.
 */
UENUM(BlueprintType)
enum class EMeleeEnemyState : uint8
{
	Patrolling              UMETA(DisplayName = "Patrolling"),
	ReturningToPatrolStart  UMETA(DisplayName = "Returning To Patrol Start"),
	Chasing                 UMETA(DisplayName = "Chasing"),
	Attacking               UMETA(DisplayName = "Attacking"),
	Damaged                 UMETA(DisplayName = "Damaged"),
	Dying                   UMETA(DisplayName = "Dying"),
};

/**
 * 2.5D melee enemy with a full state machine.
 *
 * Setup:
 *  1. Assign all six flipbooks in the Details panel.
 *  2. Attach a USplineComponent (with at least 2 points) to the actor or a
 *     child actor and assign it to PatrolComp->PatrolSpline.
 *  3. Tune AggroTrigger radius, speeds, ranges, and damage values.
 *
 * Patrol flow:
 *  – BeginPlay: patrol starts automatically (A→B→A…).
 *  – Player enters AggroTrigger → Chasing.
 *  – Player leaves → ResumePatrolDelay timer → ReturningToPatrolStart → Patrolling.
 *
 * Attack flow:
 *  – Within MeleeRange and cooldown clear → Attacking.
 *  – AttackWindupTime elapses → damage applied.
 *  – AttackDuration elapses → cooldown starts, state returns to Chasing.
 */
UCLASS()
class UEM3A2BLOCKOUT_API ABillboardMeleeEnemy : public AActor
{
	GENERATED_BODY()

public:
	ABillboardMeleeEnemy();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

public:
	// =========================================================
	// Components
	// =========================================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* CapsuleComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPaperFlipbookComponent* FlipbookComp;

	/** Sphere trigger: player entering starts chase, leaving starts return-to-patrol. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* AggroTrigger;

	/** Handles A↔B patrol, flipbook switching during patrol, and combat suspension. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTwoPointSplinePatrolComponent* PatrolComp;

	// =========================================================
	// Flipbook Assets  (assign in Blueprint Details panel)
	// =========================================================

	/** Played by the patrol component while waiting at patrol endpoints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UPaperFlipbook> IdleFlipbook;

	/** Played by the patrol component while moving between patrol endpoints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UPaperFlipbook> WalkFlipbook;

	/** Played while chasing the player or returning to patrol start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UPaperFlipbook> RunFlipbook;

	/** Played during the attack state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UPaperFlipbook> AttackFlipbook;

	/** Played briefly after taking damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UPaperFlipbook> DamageFlipbook;

	/** Played on death. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UPaperFlipbook> DieFlipbook;

	// =========================================================
	// Combat Settings
	// =========================================================

	/** When true the enemy ignores the player entirely and stays in patrol/idle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bDisableCombatAI = false;

	/**
	 * Mark true when this enemy starts disabled and is activated by an
	 * EnableEnemyCombatTrigger. When global AI is ON, triggered enemies stay
	 * disabled until their trigger fires — they are never auto-enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bIsTriggeredEnemy = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Damage dealt to the player per successful attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 20.0f;

	/** Distance (cm) at which chasing stops and attacking begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "10.0"))
	float MeleeRange = 80.0f;

	/** Time (s) from attack start until damage is applied (animation windup). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackWindupTime = 0.3f;

	/**
	 * Total duration (s) of the attack state.
	 * Set this to match your Attack flipbook's play time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float AttackDuration = 0.8f;

	/** Minimum time (s) between successive attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackCooldown = 1.5f;

	// =========================================================
	// Behavior Settings
	// =========================================================

	/** Movement speed (cm/s) while chasing the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "1.0"))
	float ChaseSpeed = 300.0f;

	/** Movement speed (cm/s) while returning to patrol start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "1.0"))
	float PatrolReturnSpeed = 250.0f;

	/** Fallback duration (s) for the damage reaction if DamageFlipbook has no frames. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.05"))
	float DamageReactDuration = 0.3f;

	/**
	 * Seconds after the player leaves aggro before the enemy starts returning to
	 * patrol start. Mirrors the turret enemies' out-of-range delay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.0"))
	float ResumePatrolDelay = 5.0f;

	/** Arrival tolerance (cm) when returning to patrol start point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "1.0"))
	float ReturnArriveDistance = 20.0f;

	/** If true the actor is destroyed DeathDestroyDelay seconds after dying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bDestroyOnDeath = false;

	/** Extra seconds after the Die animation starts before Destroy() is called. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.0"))
	float DeathDestroyDelay = 2.0f;

protected:
	// =========================================================
	// Internal State
	// =========================================================

	UPROPERTY(BlueprintReadOnly, Category = "State")
	EMeleeEnemyState CurrentState = EMeleeEnemyState::Patrolling;

	float CurrentHealth = 0.0f;

	/** Weak ref so we never hold the player alive past their death. */
	TWeakObjectPtr<APawn> TargetPlayer;

	bool bIsDead           = false;
	bool bAttackOnCooldown = false;

	/** Cached patrol point A world location; set when return-to-patrol begins. */
	FVector ReturnTarget = FVector::ZeroVector;

	FTimerHandle AttackWindupTimer;
	FTimerHandle AttackEndTimer;
	FTimerHandle AttackCooldownTimer;
	FTimerHandle DamageReactTimer;
	FTimerHandle ResumePatrolTimer;
	FTimerHandle DeathTimer;

	/** Last flipbook set by this enemy. Prevents redundant SetFlipbook calls. */
	TObjectPtr<UPaperFlipbook> LastSetFlipbook;

	// =========================================================
	// Overlap Events
	// =========================================================

	UFUNCTION()
	void OnAggroBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnAggroEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// =========================================================
	// State Machine
	// =========================================================

	/** Transitions to a new state and plays the appropriate flipbook. */
	void SetState(EMeleeEnemyState NewState);

	/** Tick routine while Chasing: move toward player, start attack in range. */
	void TickChasing(float DeltaTime);

	/** Tick routine while ReturningToPatrolStart: move to point A, then resume patrol. */
	void TickReturningToPatrolStart(float DeltaTime);

	// =========================================================
	// Attack
	// =========================================================

	void StartAttack();
	void ApplyAttackDamage();
	void OnAttackEnd();
	void OnAttackCooldownEnd();

	// =========================================================
	// Patrol Resume
	// =========================================================

	/** Fired by ResumePatrolTimer: begins the return-to-start movement. */
	void OnResumePatrolTimerExpired();

	// =========================================================
	// Damage & Death
	// =========================================================

	void OnDamageReactEnd();
	void Die();
	void HandleDeathComplete();

	// =========================================================
	// Animation
	// =========================================================

	/**
	 * Sets a flipbook on FlipbookComp with dedup to avoid redundant calls.
	 * @param bLooping  true for walk/run/idle; false for attack/damage/death.
	 */
	void SetEnemyFlipbook(UPaperFlipbook* Flipbook, bool bLooping = true);

	float GetFlipbookDuration(UPaperFlipbook* Flipbook) const;

	// =========================================================
	// Utility
	// =========================================================

	/** Snaps yaw to face TargetLocation (2D, no pitch/roll). */
	void UpdateFacingToward(const FVector& TargetLocation);
};
