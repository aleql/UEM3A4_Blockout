// BillboardEnemy.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BillboardEnemy.generated.h"

// Forward declarations
class UPaperFlipbookComponent;
class UPaperFlipbook;
class USphereComponent;
class UCapsuleComponent;
class APawn;

/**
 * 2.5D Billboard Enemy that uses Paper2D Flipbooks
 * Tracks and shoots at player when inside trigger volume
 */
UCLASS()
class UEM3A2BLOCKOUT_API ABillboardEnemy : public AActor
{
	GENERATED_BODY()

public:
	ABillboardEnemy();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

public:
	// ========== COMPONENTS ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* CapsuleComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPaperFlipbookComponent* FlipbookComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* AggroTrigger;

	// ========== FLIPBOOK ASSETS ==========

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* IdleFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* ShootFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* DamageFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UPaperFlipbook* DieFlipbook;

	// ========== COMBAT SETTINGS ==========

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

	/** Maximum health of the enemy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Seconds between each shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float FireRate = 2.0f;

	/** Speed of spawned projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 1000.0f;

	/** Damage dealt by projectiles to player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float DamageAmount = 10.0f;

	/** Projectile class to spawn when shooting */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<AActor> ProjectileClass;

	// ========== BEHAVIOR SETTINGS ==========

	/** Only face player when inside trigger (recommended: true) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bFacePlayerOnlyInTrigger = true;

	/** Duration of damage animation in seconds (used if flipbook duration can't be determined) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.1"))
	float DamageAnimDuration = 0.3f;

	/** Time from shoot animation start to spawning projectile (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.0"))
	float ShootAnimLeadTime = 0.2f;

	/** Distance in front of actor to spawn projectile */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	float MuzzleOffset = 50.0f;

	/** Height offset for projectile spawn location */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	float MuzzleHeightOffset = 0.0f;

	/** Destroy actor after death animation completes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bDestroyOnDeath = false;

	/** Duration to wait before destroying after death (if bDestroyOnDeath is true) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.0"))
	float DeathDestroyDelay = 2.0f;

protected:
	// ========== INTERNAL STATE ==========

	float CurrentHealth;

	TWeakObjectPtr<APawn> TargetPlayer;

	bool bIsDead = false;
	bool bIsPlayerInRange = false;
	bool bIsPlayingDamageAnim = false;

	FTimerHandle ShootingTimerHandle;
	FTimerHandle DamageAnimTimerHandle;
	FTimerHandle ShootProjectileTimerHandle;
	FTimerHandle ShootIdleReturnTimerHandle;  // cancellable return-to-idle after shoot anim
	FTimerHandle DeathTimerHandle;

	// ========== TRIGGER EVENTS ==========

	UFUNCTION()
	void OnAggroBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnAggroEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ========== FACING LOGIC ==========

	void UpdateFacingDirection();

	// ========== SHOOTING LOGIC ==========

	void StartShooting();
	void StopShooting();
	void PerformShootSequence();
	void SpawnProjectile();

	// ========== ANIMATION STATE ==========

	void SetFlipbookIdle();
	void SetFlipbookShoot();
	void SetFlipbookDamage();
	void SetFlipbookDie();

	float GetFlipbookDuration(UPaperFlipbook* Flipbook) const;

	// ========== DAMAGE & DEATH ==========

	void Die();
	void HandleDeath();
};
