// ArcBillboardEnemy.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcBillboardEnemy.generated.h"

// Forward declarations
class UPaperFlipbookComponent;
class UPaperFlipbook;
class USphereComponent;
class UCapsuleComponent;
class AEnemyArcProjectile;
class APawn;

/**
 * 2.5D Billboard Turret Enemy that fires parabolic arc projectiles.
 *
 * Setup:
 *   1. Assign IdleFlipbook, ShootFlipbook, DamageFlipbook, DieFlipbook in the Details panel.
 *   2. Assign ProjectileClass (a Blueprint child of AEnemyArcProjectile).
 *   3. Tune LaunchSpeed and GravityScale so arcs reach the intended range.
 *   4. Adjust AggroTrigger radius to match the arena.
 *
 * Firing uses UGameplayStatics::SuggestProjectileVelocity (Approach A).
 * GravityScale is applied consistently to both the ballistic solver and
 * the projectile movement component.
 */
UCLASS()
class UEM3A2BLOCKOUT_API AArcBillboardEnemy : public AActor
{
	GENERATED_BODY()

public:
	AArcBillboardEnemy();

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

	/** Sphere trigger: player entering starts facing + firing, player leaving stops both. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* AggroTrigger;

	// =========================================================
	// Flipbook Assets  (assign in Blueprint Details panel)
	// =========================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* IdleFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* ShootFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* DamageFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* DieFlipbook;

	// =========================================================
	// Combat Settings
	// =========================================================

	/** When true the enemy ignores the player entirely and stays idle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bDisableCombatAI = false;

	/**
	 * Mark true when this enemy starts disabled and is activated by an
	 * EnableEnemyCombatTrigger. When global AI is ON, triggered enemies stay
	 * disabled until their trigger fires — they are never auto-enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bIsTriggeredEnemy = false;

	/** Total hit-points. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Seconds between successive shots (fire interval, not rate). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float FireInterval = 2.0f;

	/** Damage applied to player per projectile hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float DamageAmount = 10.0f;

	/** Blueprint child of AEnemyArcProjectile to spawn when shooting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<AEnemyArcProjectile> ProjectileClass;

	// =========================================================
	// Arc Settings
	// =========================================================

	/**
	 * Speed (cm/s) passed to SuggestProjectileVelocity as TossSpeed.
	 * Higher values reach farther targets. If the solver fails, increase this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (ClampMin = "100.0"))
	float LaunchSpeed = 1500.0f;

	/**
	 * When true, SuggestProjectileVelocity prefers the high lob arc.
	 * When false, it prefers the lower, faster arc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc")
	bool bUseHighArc = false;

	/**
	 * When true, all projectiles are always fired straight toward the player
	 * (no arc solver, no gravity). Overrides both the close-range threshold
	 * and the high/low arc selection. Default false (arc behavior).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc")
	bool bForceStraightShot = false;

	/**
	 * Multiplier applied to world gravity for both the ballistic solver
	 * and the spawned projectile. Keep them in sync or arcs will miss.
	 * Default 1.0 = normal gravity (~980 cm/s^2).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float GravityScale = 1.0f;

	/** Local-space offset from actor origin where the projectile spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc")
	FVector MuzzleOffset = FVector(50.0f, 0.0f, 20.0f);

	// =========================================================
	// Behavior Settings
	// =========================================================

	/**
	 * Delay (seconds) from PlayShoot() until the projectile actually spawns.
	 * Sync this to the "fire" frame of your Shoot flipbook.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.0"))
	float ShootAnimLeadTime = 0.2f;

	/** Fallback damage-react duration if DamageFlipbook has no frames. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.05"))
	float DamageAnimDuration = 0.3f;

	/** If true, the actor is destroyed DeathDestroyDelay seconds after the Die animation starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bDestroyOnDeath = false;

	/** Seconds after die animation before Destroy() is called (requires bDestroyOnDeath). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.0"))
	float DeathDestroyDelay = 2.0f;

	/** Only rotate to face the player while they are inside AggroTrigger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bFacePlayerOnlyInTrigger = true;

protected:
	// =========================================================
	// Internal State
	// =========================================================

	float CurrentHealth;

	/** Weak ref so we never hold the player alive. Cleared on exit / death. */
	TWeakObjectPtr<APawn> TargetPlayer;

	bool bIsDead          = false;
	bool bIsPlayerInRange = false;
	bool bIsInDamageReact = false;

	/** Repeating timer that triggers BeginShootSequence(). */
	FTimerHandle FireTimerHandle;

	/** One-shot timer that spawns the projectile after ShootAnimLeadTime. */
	FTimerHandle SpawnProjectileTimerHandle;

	/** One-shot timer that returns to Idle after Damage animation finishes. */
	FTimerHandle DamageReactTimerHandle;

	/** One-shot timer that calls HandleDeathComplete() for optional Destroy(). */
	FTimerHandle DeathTimerHandle;

	// =========================================================
	// Trigger Events
	// =========================================================

	UFUNCTION()
	void OnAggroBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnAggroEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// =========================================================
	// Facing
	// =========================================================

	/** Yaw-only rotation toward the target player. Called every Tick while in range. */
	void UpdateFacingDirection();

	/**
	 * Tick-driven aggro check that mirrors the sphere overlap events.
	 * Chaos physics broadphase becomes unreliable for sphere radii above ~4000 cm,
	 * causing OnComponentBeginOverlap / EndOverlap to fire silently.
	 * This distance check runs every frame and catches what the physics misses.
	 * bIsPlayerInRange prevents double-triggering when overlaps DO fire normally.
	 */
	void CheckPlayerAggroDistance();

	// =========================================================
	// Shooting
	// =========================================================

	void StartFiring();
	void StopFiring();

	/** Called by FireTimerHandle: plays Shoot animation and schedules SpawnArcProjectile. */
	void BeginShootSequence();

	/** Spawns the projectile with ballistic velocity toward the player. */
	void SpawnArcProjectile();

	/**
	 * Solves ballistic launch velocity via UGameplayStatics::SuggestProjectileVelocity.
	 * @param MuzzleLocation  World-space start of the arc.
	 * @param TargetLocation  World-space target (player location).
	 * @param OutVelocity     Filled with the launch velocity on success.
	 * @return true if a valid arc was found.
	 */
	bool ComputeLaunchVelocity(const FVector& MuzzleLocation,
		const FVector& TargetLocation, FVector& OutVelocity) const;

	// =========================================================
	// Animation Helpers
	// =========================================================

	/** Priority: Die > Damage > Shoot > Idle */
	void PlayIdle();
	void PlayShoot();
	void PlayDamage();
	void PlayDie();

	/** Returns the total play-time in seconds for a flipbook. Returns 0 on failure. */
	float GetFlipbookDuration(UPaperFlipbook* Flipbook) const;

	// =========================================================
	// Damage & Death
	// =========================================================

	/** Initiates the death sequence. Idempotent (guarded by bIsDead). */
	void Die();

	/** Called by DeathTimerHandle to optionally destroy the actor. */
	void HandleDeathComplete();
};
