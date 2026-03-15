// EnemyProjectile.cpp

#include "EnemyProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create collision component
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(15.0f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn,        ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	// Do NOT block WorldDynamic: enemy actors (FlipbookComp) are WorldDynamic and
	// would stop the projectile dead before it reaches the player.
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	CollisionComp->SetGenerateOverlapEvents(true);
	RootComponent = CollisionComp;

	// Create mesh component (optional visual)
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));

	// Create projectile movement component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	// Set lifespan
	InitialLifeSpan = ProjectileLifespan;
}

void AEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap event (player hit)
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AEnemyProjectile::OnProjectileBeginOverlap);

	// Bind stop event: when the projectile hits world geometry and stops, destroy it
	// so it doesn't float at the wall for the full InitialLifeSpan duration.
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AEnemyProjectile::OnProjectileStopped);

	// Apply speed from settings
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = GetActorForwardVector() * Speed;
	}
}

void AEnemyProjectile::OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Don't hit owner
	if (OtherActor == GetOwner() || OtherActor == this)
	{
		return;
	}

	// Check if hit a player pawn
	APawn* HitPawn = Cast<APawn>(OtherActor);
	if (HitPawn && HitPawn->IsPlayerControlled())
	{
		// Apply damage
		UGameplayStatics::ApplyDamage(HitPawn, Damage, GetInstigatorController(), this, UDamageType::StaticClass());

		// Destroy projectile
		Destroy();
		return;
	}

	// The only overlap channel configured is ECC_Pawn, so this branch is only
	// reachable if a non-player-controlled pawn (e.g. an NPC) is in the scene.
	// Destroy the projectile so it doesn't pass through NPCs silently.
	// Enemy actors extend AActor (not APawn) so they can never reach this line.
	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (OtherPawn)
	{
		Destroy();
	}
}

void AEnemyProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	// Projectile hit world geometry and can no longer move – destroy immediately
	// rather than waiting for InitialLifeSpan to expire.
	Destroy();
}

void AEnemyProjectile::InitializeProjectile(FVector Direction, float InSpeed, float InDamage)
{
	Speed = InSpeed;
	Damage = InDamage;

	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Direction * Speed;
	}
}
