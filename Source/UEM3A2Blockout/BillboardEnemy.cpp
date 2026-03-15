// BillboardEnemy.cpp

#include "BillboardEnemy.h"
#include "BlockoutGameInstance.h"
#include "EnemyProjectile.h"
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

ABillboardEnemy::ABillboardEnemy()
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

	FlipbookComp = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("FlipbookComp"));
	FlipbookComp->SetupAttachment(CapsuleComp);
	FlipbookComp->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	FlipbookComp->SetRelativeLocation(FVector(0.f, 0.f, -88.f));

	AggroTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("AggroTrigger"));
	AggroTrigger->SetupAttachment(CapsuleComp);
	AggroTrigger->SetSphereRadius(500.0f);
	AggroTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AggroTrigger->SetGenerateOverlapEvents(true);
}

void ABillboardEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Initialize health
	CurrentHealth = MaxHealth;

	// Bind overlap events
	AggroTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABillboardEnemy::OnAggroBeginOverlap);
	AggroTrigger->OnComponentEndOverlap.AddDynamic(this, &ABillboardEnemy::OnAggroEndOverlap);

	// Set initial flipbook
	SetFlipbookIdle();

	// Apply global AI flag: triggered enemies always start disabled regardless.
	if (UBlockoutGameInstance* GI = GetGameInstance<UBlockoutGameInstance>())
	{
		if (!GI->bGlobalAIEnabled || bIsTriggeredEnemy)
		{
			bDisableCombatAI = true;
		}
	}
}

void ABillboardEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsDead)
	{
		UpdateFacingDirection();
	}
}

float ABillboardEnemy::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.0f;
	}

	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

	CurrentHealth -= ActualDamage;

	if (CurrentHealth <= 0.0f)
	{
		CurrentHealth = 0.0f;
		Die();
	}
	else
	{
		SetFlipbookDamage();
	}

	return ActualDamage;
}

// ========== TRIGGER EVENTS ==========

void ABillboardEnemy::OnAggroBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsDead || bDisableCombatAI)
	{
		return;
	}

	// Check if the overlapping actor is a player pawn
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		TargetPlayer = Pawn;
		bIsPlayerInRange = true;

		// Start shooting
		StartShooting();

		// Suspend patrol immediately (Method A)
		if (UTwoPointSplinePatrolComponent* Patrol =
			FindComponentByClass<UTwoPointSplinePatrolComponent>())
		{
			Patrol->NotifyCombatStarted();
		}
	}
}

void ABillboardEnemy::OnAggroEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn == TargetPlayer.Get())
	{
		TargetPlayer = nullptr;
		bIsPlayerInRange = false;

		// Stop shooting
		StopShooting();

		// Start the 5-second patrol resume countdown (Method A)
		if (UTwoPointSplinePatrolComponent* Patrol =
			FindComponentByClass<UTwoPointSplinePatrolComponent>())
		{
			Patrol->NotifyCombatEnded();
		}

		// Return to idle if not dead
		if (!bIsDead && !bIsPlayingDamageAnim)
		{
			SetFlipbookIdle();
		}
	}
}

// ========== FACING LOGIC ==========

void ABillboardEnemy::UpdateFacingDirection()
{
	APawn* Player = TargetPlayer.IsValid()
		? TargetPlayer.Get()
		: UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

	if (!Player)
	{
		return;
	}

	FVector PlayerLocation = Player->GetActorLocation();
	FVector MyLocation = GetActorLocation();
	FVector DirectionToPlayer = PlayerLocation - MyLocation;
	DirectionToPlayer.Z = 0.0f; // Keep rotation on horizontal plane only

	if (!DirectionToPlayer.IsNearlyZero())
	{
		FRotator NewRotation = DirectionToPlayer.Rotation();
		SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
	}
}

// ========== SHOOTING LOGIC ==========

void ABillboardEnemy::StartShooting()
{
	if (bIsDead || !TargetPlayer.IsValid())
	{
		return;
	}

	// Start repeating shoot timer
	GetWorldTimerManager().SetTimer(ShootingTimerHandle, this, &ABillboardEnemy::PerformShootSequence,
		FireRate, true, 0.0f); // Start immediately
}

void ABillboardEnemy::StopShooting()
{
	GetWorldTimerManager().ClearTimer(ShootingTimerHandle);
	GetWorldTimerManager().ClearTimer(ShootProjectileTimerHandle);
	GetWorldTimerManager().ClearTimer(ShootIdleReturnTimerHandle);
}

void ABillboardEnemy::PerformShootSequence()
{
	if (bIsDead || !TargetPlayer.IsValid() || bIsPlayingDamageAnim)
	{
		return;
	}

	// Play shoot animation
	SetFlipbookShoot();

	// Schedule projectile spawn after lead time
	GetWorldTimerManager().SetTimer(ShootProjectileTimerHandle, this, &ABillboardEnemy::SpawnProjectile,
		ShootAnimLeadTime, false);
}

void ABillboardEnemy::SpawnProjectile()
{
	if (bIsDead || !TargetPlayer.IsValid() || !ProjectileClass)
	{
		return;
	}

	// Calculate spawn location
	FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * MuzzleOffset;
	SpawnLocation.Z += MuzzleHeightOffset;

	// Calculate direction to player
	FVector PlayerLocation = TargetPlayer->GetActorLocation();
	FVector Direction = (PlayerLocation - SpawnLocation).GetSafeNormal();

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Projectile = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLocation, Direction.Rotation(), SpawnParams);

	// Apply this turret's configured speed and damage to the spawned projectile.
	// Without this call the projectile runs on its C++ default values (1000 cm/s, 10 dmg).
	if (AEnemyProjectile* EP = Cast<AEnemyProjectile>(Projectile))
	{
		EP->InitializeProjectile(Direction, ProjectileSpeed, DamageAmount);
	}
}

// ========== ANIMATION STATE ==========

void ABillboardEnemy::SetFlipbookIdle()
{
	if (IdleFlipbook && FlipbookComp)
	{
		FlipbookComp->SetFlipbook(IdleFlipbook);
		FlipbookComp->SetLooping(true);
		FlipbookComp->Play();
	}
}

void ABillboardEnemy::SetFlipbookShoot()
{
	if (ShootFlipbook && FlipbookComp)
	{
		FlipbookComp->SetFlipbook(ShootFlipbook);
		FlipbookComp->SetLooping(false);
		FlipbookComp->Play();

		// Return to idle after animation completes.
		// Use a persistent member handle so StopShooting() can cancel this timer;
		// a local FTimerHandle would go out of scope and accumulate ghost callbacks.
		float Duration = GetFlipbookDuration(ShootFlipbook);
		GetWorldTimerManager().SetTimer(ShootIdleReturnTimerHandle, this, &ABillboardEnemy::SetFlipbookIdle, Duration, false);
	}
}

void ABillboardEnemy::SetFlipbookDamage()
{
	if (DamageFlipbook && FlipbookComp)
	{
		bIsPlayingDamageAnim = true;

		FlipbookComp->SetFlipbook(DamageFlipbook);
		FlipbookComp->SetLooping(false);
		FlipbookComp->Play();

		// Return to idle after damage animation
		float Duration = GetFlipbookDuration(DamageFlipbook);
		if (Duration <= 0.0f)
		{
			Duration = DamageAnimDuration;
		}

		GetWorldTimerManager().SetTimer(DamageAnimTimerHandle, [this]()
		{
			bIsPlayingDamageAnim = false;
			if (!bIsDead)
			{
				SetFlipbookIdle();
			}
		}, Duration, false);
	}
}

void ABillboardEnemy::SetFlipbookDie()
{
	if (DieFlipbook && FlipbookComp)
	{
		FlipbookComp->SetFlipbook(DieFlipbook);
		FlipbookComp->SetLooping(false);
		FlipbookComp->Play();
	}
}

float ABillboardEnemy::GetFlipbookDuration(UPaperFlipbook* Flipbook) const
{
	if (!Flipbook)
	{
		return 0.0f;
	}

	float FrameCount = static_cast<float>(Flipbook->GetNumFrames());
	float FramesPerSecond = Flipbook->GetFramesPerSecond();

	if (FramesPerSecond > 0.0f)
	{
		return FrameCount / FramesPerSecond;
	}

	return 0.0f;
}

// ========== DAMAGE & DEATH ==========

void ABillboardEnemy::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// Stop all timers
	StopShooting();
	GetWorldTimerManager().ClearTimer(DamageAnimTimerHandle);

	// Permanently halt patrol – enemy is dead, no resume should ever happen
	if (UTwoPointSplinePatrolComponent* Patrol =
		FindComponentByClass<UTwoPointSplinePatrolComponent>())
	{
		Patrol->StopPatrol();
	}

	// Disable collision
	AggroTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Clear target
	TargetPlayer = nullptr;
	bIsPlayerInRange = false;

	// Play death animation
	SetFlipbookDie();

	// Handle post-death behavior
	if (bDestroyOnDeath)
	{
		GetWorldTimerManager().SetTimer(DeathTimerHandle, this, &ABillboardEnemy::HandleDeath,
			DeathDestroyDelay, false);
	}
}

void ABillboardEnemy::HandleDeath()
{
	Destroy();
}
