// EnemyArcProjectile.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyArcProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;

/**
 * Gravity-affected arc projectile fired by AArcBillboardEnemy.
 *
 * Usage:
 *   1. After SpawnActor, immediately call InitializeArcProjectile(Velocity, GravityScale, Damage).
 *   2. InitialSpeed in the constructor is 0 so BeginPlay does not override the velocity.
 *   3. GravityScale on the ProjectileMovementComponent MUST match the value used in
 *      SuggestProjectileVelocity (EffectiveGravityZ = GetWorld()->GetGravityZ() * GravityScale).
 *
 * Collision behaviour:
 *   - Overlaps ECC_Pawn:  applies damage to the player and self-destructs.
 *   - Blocks ECC_WorldStatic / ECC_WorldDynamic:
 *     UProjectileMovementComponent stops simulation; OnProjectileStopped destroys the actor.
 *   - Auto-destroys after InitialLifeSpan seconds (default 6 s).
 */
UCLASS()
class UEM3A2BLOCKOUT_API AEnemyArcProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyArcProjectile();

protected:
	virtual void BeginPlay() override;

public:
	// =========================================================
	// Components
	// =========================================================

	/** Sphere used for all collision (overlap pawn, block world). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComp;

	/**
	 * Optional visual mesh. Assign a sphere or custom asset in the Blueprint.
	 * Scale is set to 0.25 by default; override in Blueprint as needed.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	/** Drives arc movement with gravity. InitialSpeed = 0; velocity set via InitializeArcProjectile. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// =========================================================
	// Settings
	// =========================================================

	/** Damage applied to the player on overlap. Set by AArcBillboardEnemy::SpawnArcProjectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 10.0f;

	/**
	 * Gravity multiplier applied to this projectile's movement.
	 * MUST match the GravityScale used in the ballistic solver on the enemy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.1"))
	float GravityScale = 1.0f;

	/** Radius of the collision sphere in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "5.0"))
	float CollisionRadius = 15.0f;

protected:
	// =========================================================
	// Collision Events
	// =========================================================

	/** Player pawn overlapped: apply damage, destroy. */
	UFUNCTION()
	void OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/** World geometry blocked movement: destroy so the projectile doesn't float. */
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

public:
	// =========================================================
	// Initialization
	// =========================================================

	/**
	 * Sets the arc velocity, gravity scale, and damage on this projectile.
	 * Call this immediately after SpawnActor before the next physics tick.
	 *
	 * @param LaunchVelocity  World-space velocity from SuggestProjectileVelocity.
	 * @param InGravityScale  Must equal AArcBillboardEnemy::GravityScale.
	 * @param InDamage        Damage applied to the player on hit.
	 */
	void InitializeArcProjectile(FVector LaunchVelocity, float InGravityScale, float InDamage);
};
