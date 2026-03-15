// EnemyProjectile.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;

/**
 * Simple projectile fired by billboard enemies
 * Applies damage to player on hit
 */
UCLASS()
class UEM3A2BLOCKOUT_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyProjectile();

protected:
	virtual void BeginPlay() override;

public:
	// ========== COMPONENTS ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// ========== SETTINGS ==========

	/** Damage dealt to player on hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 10.0f;

	/** Projectile speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Speed = 1000.0f;

	/** Lifespan in seconds before auto-destroy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float ProjectileLifespan = 5.0f;

protected:
	UFUNCTION()
	void OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Called by ProjectileMovementComponent when the projectile stops (hits world geometry). */
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

public:
	/** Initialize projectile velocity and damage */
	void InitializeProjectile(FVector Direction, float InSpeed, float InDamage);
};
