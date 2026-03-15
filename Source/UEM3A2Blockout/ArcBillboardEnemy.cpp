// ArcBillboardEnemy.cpp
#include "ArcBillboardEnemy.h"

#include "BlockoutGameInstance.h"
#include "EnemyArcProjectile.h"
#include "TwoPointSplinePatrolComponent.h"
#include "PaperFlipbookComponent.h"
#include "PaperFlipbook.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AArcBillboardEnemy::AArcBillboardEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
	CapsuleComp->InitCapsuleSize(40.f, 88.f);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComp->SetCollisionObjectType(ECC_Pawn);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Block);
	CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SetRootComponent(CapsuleComp);

	// Flipbook (Paper2D)
	// The +90° yaw makes the sprite face the actor's forward (+X) direction.
	FlipbookComp = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("FlipbookComp"));
	FlipbookComp->SetupAttachment(CapsuleComp);
	FlipbookComp->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	FlipbookComp->SetRelativeLocation(FVector(0.f, 0.f, -88.f));

	// Aggro sphere trigger
	AggroTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("AggroTrigger"));
	AggroTrigger->SetupAttachment(CapsuleComp);
	AggroTrigger->SetSphereRadius(600.0f);
	AggroTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AggroTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	AggroTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	AggroTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AggroTrigger->SetGenerateOverlapEvents(true);
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	AggroTrigger->OnComponentBeginOverlap.AddDynamic(this, &AArcBillboardEnemy::OnAggroBeginOverlap);
	AggroTrigger->OnComponentEndOverlap.AddDynamic(this, &AArcBillboardEnemy::OnAggroEndOverlap);

	PlayIdle();

	// Apply global AI flag: triggered enemies always start disabled regardless.
	if (UBlockoutGameInstance* GI = GetGameInstance<UBlockoutGameInstance>())
	{
		if (!GI->bGlobalAIEnabled || bIsTriggeredEnemy)
		{
			bDisableCombatAI = true;
		}
	}
}

// ---------------------------------------------------------------------------
// Tick – yaw-only billboard facing
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsDead)
	{
		UpdateFacingDirection();
		CheckPlayerAggroDistance();
	}
}

// ---------------------------------------------------------------------------
// Damage system
// ---------------------------------------------------------------------------

float AArcBillboardEnemy::TakeDamage(float Damage, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.0f;
	}

	const float Applied = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	CurrentHealth -= Applied;

	if (CurrentHealth <= 0.0f)
	{
		CurrentHealth = 0.0f;
		Die();
	}
	else
	{
		PlayDamage();
	}

	return Applied;
}

// ---------------------------------------------------------------------------
// Trigger Overlaps
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::OnAggroBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsDead || bDisableCombatAI)
	{
		return;
	}

	// Diagnostic: confirms this C++ callback is what triggers aggro (not a Blueprint event).
APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		TargetPlayer      = Pawn;
		bIsPlayerInRange  = true;
		StartFiring();

		// Suspend patrol immediately (Method A)
		if (UTwoPointSplinePatrolComponent* Patrol =
			FindComponentByClass<UTwoPointSplinePatrolComponent>())
		{
			Patrol->NotifyCombatStarted();
		}
	}
}

void AArcBillboardEnemy::OnAggroEndOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn == TargetPlayer.Get())
	{
		TargetPlayer     = nullptr;
		bIsPlayerInRange = false;

		StopFiring();

		// Start the 5-second patrol resume countdown (Method A)
		if (UTwoPointSplinePatrolComponent* Patrol =
			FindComponentByClass<UTwoPointSplinePatrolComponent>())
		{
			Patrol->NotifyCombatEnded();
		}

		if (!bIsDead && !bIsInDamageReact)
		{
			PlayIdle();
		}
	}
}

// ---------------------------------------------------------------------------
// Facing – yaw only so the billboard stays upright
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::UpdateFacingDirection()
{
	APawn* Player = TargetPlayer.IsValid()
		? TargetPlayer.Get()
		: UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

	if (!Player)
	{
		return;
	}

	FVector Direction = Player->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f;   // horizontal plane only

	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
	}
}

// ---------------------------------------------------------------------------
// Aggro Distance Check
// Supplements overlap events, which Chaos physics may drop for large radii.
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::CheckPlayerAggroDistance()
{
	if (bDisableCombatAI)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

	const float RadiusSq = FMath::Square(AggroTrigger->GetScaledSphereRadius());

	if (Player)
	{
		const float DistSq = FVector::DistSquared(GetActorLocation(), Player->GetActorLocation());

		if (DistSq <= RadiusSq && !bIsPlayerInRange)
		{
			// Player entered aggro range — mirrors OnAggroBeginOverlap
			TargetPlayer     = Player;
			bIsPlayerInRange = true;
			StartFiring();

			if (UTwoPointSplinePatrolComponent* Patrol =
				FindComponentByClass<UTwoPointSplinePatrolComponent>())
			{
				Patrol->NotifyCombatStarted();
			}
		}
		else if (DistSq > RadiusSq && bIsPlayerInRange)
		{
			// Player exited aggro range — mirrors OnAggroEndOverlap
			TargetPlayer     = nullptr;
			bIsPlayerInRange = false;
			StopFiring();

			if (UTwoPointSplinePatrolComponent* Patrol =
				FindComponentByClass<UTwoPointSplinePatrolComponent>())
			{
				Patrol->NotifyCombatEnded();
			}

			if (!bIsInDamageReact)
			{
				PlayIdle();
			}
		}
	}
	else if (bIsPlayerInRange)
	{
		// Player no longer exists (e.g. destroyed in editor) — clean up
		TargetPlayer     = nullptr;
		bIsPlayerInRange = false;
		StopFiring();

		if (!bIsInDamageReact)
		{
			PlayIdle();
		}
	}
}

// ---------------------------------------------------------------------------
// Firing – timer management
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::StartFiring()
{
	if (bIsDead || !TargetPlayer.IsValid())
	{
		return;
	}

	// Fire immediately, then repeat every FireInterval seconds
	GetWorldTimerManager().SetTimer(
		FireTimerHandle,
		this, &AArcBillboardEnemy::BeginShootSequence,
		FireInterval,
		/*bLoop=*/true,
		/*FirstDelay=*/0.0f);
}

void AArcBillboardEnemy::StopFiring()
{
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnProjectileTimerHandle);
}

// ---------------------------------------------------------------------------
// Shoot Sequence
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::BeginShootSequence()
{
	if (bIsDead || !TargetPlayer.IsValid() || bIsInDamageReact)
	{
		return;
	}

	PlayShoot();

	// Delay projectile spawn by ShootAnimLeadTime to sync with the animation
	GetWorldTimerManager().SetTimer(
		SpawnProjectileTimerHandle,
		this, &AArcBillboardEnemy::SpawnArcProjectile,
		FMath::Max(ShootAnimLeadTime, 0.01f),
		/*bLoop=*/false);
}

void AArcBillboardEnemy::SpawnArcProjectile()
{
	if (bIsDead || !TargetPlayer.IsValid() || !ProjectileClass)
	{
		return;
	}

	// World-space muzzle location (MuzzleOffset is in local space)
	const FVector MuzzleLocation =
		GetActorLocation() + GetActorRotation().RotateVector(MuzzleOffset);

	const FVector TargetLocation = TargetPlayer->GetActorLocation();

	// Within 1/3 of the aggro radius: fire straight (gravity 0, direct aim).
	// Beyond that threshold: solve a parabolic arc via bUseHighArc.
	const float CloseRangeThreshold = AggroTrigger->GetScaledSphereRadius() / 3.0f;
	const float DistToTarget        = FVector::Dist(MuzzleLocation, TargetLocation);

	FVector LaunchVelocity    = FVector::ZeroVector;
	float   EffectiveGravity  = GravityScale;

	if (bForceStraightShot || DistToTarget < CloseRangeThreshold)
	{
		// Close range: shoot directly toward the player, no gravity drop
		LaunchVelocity   = (TargetLocation - MuzzleLocation).GetSafeNormal() * LaunchSpeed;
		EffectiveGravity = 0.0f;
	}
	else
	{
		// Far range: solve arc trajectory.
		// If the solver fails (LaunchSpeed too low to reach the target at this distance),
		// fall back to a straight shot so the enemy always fires rather than silently skipping.
		if (!ComputeLaunchVelocity(MuzzleLocation, TargetLocation, LaunchVelocity))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("AArcBillboardEnemy [%s]: SuggestProjectileVelocity found no solution "
					"(LaunchSpeed %.0f may be too low for distance %.0f). Falling back to straight shot."),
				*GetName(), LaunchSpeed, DistToTarget);

			LaunchVelocity   = (TargetLocation - MuzzleLocation).GetSafeNormal() * LaunchSpeed;
			EffectiveGravity = 0.0f;
		}
	}

	// Spawn parameters
	FActorSpawnParameters Params;
	Params.Owner      = this;
	Params.Instigator = GetInstigator();
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnemyArcProjectile* Projectile = GetWorld()->SpawnActor<AEnemyArcProjectile>(
		ProjectileClass, MuzzleLocation, LaunchVelocity.Rotation(), Params);

	if (Projectile)
	{
		Projectile->InitializeArcProjectile(LaunchVelocity, EffectiveGravity, DamageAmount);
	}

	// Return to idle once the shoot animation finishes
	if (!bIsDead && !bIsInDamageReact)
	{
		const float ShootDuration = GetFlipbookDuration(ShootFlipbook);
		const float IdleDelay = FMath::Max(ShootDuration - ShootAnimLeadTime, 0.01f);

		FTimerHandle IdleTimerHandle;
		TWeakObjectPtr<AArcBillboardEnemy> WeakThis(this);
		GetWorldTimerManager().SetTimer(IdleTimerHandle,
			FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (WeakThis.IsValid() && !WeakThis->bIsDead && !WeakThis->bIsInDamageReact)
				{
					WeakThis->PlayIdle();
				}
			}),
			IdleDelay, /*bLoop=*/false);
	}
}

// ---------------------------------------------------------------------------
// Ballistic Arc Solver (Approach A)
// ---------------------------------------------------------------------------

bool AArcBillboardEnemy::ComputeLaunchVelocity(const FVector& MuzzleLocation,
	const FVector& TargetLocation, FVector& OutVelocity) const
{
	// Scale world gravity to match what the projectile movement component will use.
	// GetGravityZ() returns a negative value (e.g. -980 cm/s^2).
	// Passing 0 to SuggestProjectileVelocity uses the world default;
	// passing the explicit scaled value keeps the solver and physics in sync.
	const float EffectiveGravityZ = GetWorld()->GetGravityZ() * GravityScale;

	return UGameplayStatics::SuggestProjectileVelocity(
		this,
		OutVelocity,
		MuzzleLocation,
		TargetLocation,
		LaunchSpeed,
		bUseHighArc,
		/*CollisionRadius=*/0.0f,
		EffectiveGravityZ,
		ESuggestProjVelocityTraceOption::DoNotTrace);
}

// ---------------------------------------------------------------------------
// Animation Helpers – enforce Die > Damage > Shoot > Idle priority
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::PlayIdle()
{
	if (!IdleFlipbook || !FlipbookComp)
	{
		return;
	}
	FlipbookComp->SetFlipbook(IdleFlipbook);
	FlipbookComp->SetLooping(true);
	FlipbookComp->Play();
}

void AArcBillboardEnemy::PlayShoot()
{
	if (!ShootFlipbook || !FlipbookComp)
	{
		return;
	}
	FlipbookComp->SetFlipbook(ShootFlipbook);
	FlipbookComp->SetLooping(false);
	FlipbookComp->Play();
}

void AArcBillboardEnemy::PlayDamage()
{
	if (!DamageFlipbook || !FlipbookComp)
	{
		return;
	}

	// Cancel any in-flight damage react timer so overlapping hits don't stack
	GetWorldTimerManager().ClearTimer(DamageReactTimerHandle);

	bIsInDamageReact = true;
	FlipbookComp->SetFlipbook(DamageFlipbook);
	FlipbookComp->SetLooping(false);
	FlipbookComp->Play();

	float Duration = GetFlipbookDuration(DamageFlipbook);
	if (Duration <= 0.0f)
	{
		Duration = DamageAnimDuration;
	}

	TWeakObjectPtr<AArcBillboardEnemy> WeakThis(this);
	GetWorldTimerManager().SetTimer(DamageReactTimerHandle,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			WeakThis->bIsInDamageReact = false;
			if (!WeakThis->bIsDead)
			{
				WeakThis->PlayIdle();
			}
		}),
		Duration, /*bLoop=*/false);
}

void AArcBillboardEnemy::PlayDie()
{
	if (!DieFlipbook || !FlipbookComp)
	{
		return;
	}
	FlipbookComp->SetFlipbook(DieFlipbook);
	FlipbookComp->SetLooping(false);
	FlipbookComp->Play();
}

float AArcBillboardEnemy::GetFlipbookDuration(UPaperFlipbook* Flipbook) const
{
	if (!Flipbook)
	{
		return 0.0f;
	}

	const float FPS = Flipbook->GetFramesPerSecond();
	return (FPS > 0.0f)
		? static_cast<float>(Flipbook->GetNumFrames()) / FPS
		: 0.0f;
}

// ---------------------------------------------------------------------------
// Death
// ---------------------------------------------------------------------------

void AArcBillboardEnemy::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// Stop all active timers
	StopFiring();
	GetWorldTimerManager().ClearTimer(DamageReactTimerHandle);

	// Permanently halt patrol – enemy is dead, no resume should ever happen
	if (UTwoPointSplinePatrolComponent* Patrol =
		FindComponentByClass<UTwoPointSplinePatrolComponent>())
	{
		Patrol->StopPatrol();
	}

	// Disable collision so the corpse doesn't block the player
	AggroTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Clear player ref
	TargetPlayer     = nullptr;
	bIsPlayerInRange = false;
	bIsInDamageReact = false;

	PlayDie();

	if (bDestroyOnDeath)
	{
		GetWorldTimerManager().SetTimer(
			DeathTimerHandle,
			this, &AArcBillboardEnemy::HandleDeathComplete,
			FMath::Max(DeathDestroyDelay, 0.01f),
			/*bLoop=*/false);
	}
}

void AArcBillboardEnemy::HandleDeathComplete()
{
	Destroy();
}
