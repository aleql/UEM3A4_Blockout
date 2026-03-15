// EnemyArcProjectile.cpp
#include "EnemyArcProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AEnemyArcProjectile::AEnemyArcProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// ------------------------------------------------------------------
	// Collision sphere
	// ------------------------------------------------------------------
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(CollisionRadius);
	RootComponent = CollisionComp;

	// Object type: WorldDynamic so other projectiles don't interact with it
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Start with all channels ignored, then enable the ones we need
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Overlap pawn → damage + self-destruct in OnProjectileBeginOverlap
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Block world geometry so ProjectileMovementComponent fires OnProjectileStop.
	// Do NOT block WorldDynamic: enemy actors (FlipbookComp) are WorldDynamic and
	// would stop the projectile dead before it reaches the player.
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic,  ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);

	CollisionComp->SetGenerateOverlapEvents(true);

	// ------------------------------------------------------------------
	// Optional visual mesh
	// Assign a sphere mesh in the Blueprint Details panel.
	// Default scale is small (0.25) – scale up if needed.
	// ------------------------------------------------------------------
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetRelativeScale3D(FVector(0.25f));

	// ------------------------------------------------------------------
	// Projectile movement
	//
	// InitialSpeed = 0 so that BeginPlay() does NOT override the velocity
	// we set in InitializeArcProjectile().
	// MaxSpeed     = 0 means no speed clamping; the projectile can
	//               accelerate naturally as gravity pulls it down.
	// ------------------------------------------------------------------
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent        = CollisionComp;
	ProjectileMovement->InitialSpeed            = 0.0f;
	ProjectileMovement->MaxSpeed                = 0.0f;   // 0 = no cap
	ProjectileMovement->bRotationFollowsVelocity = true;  // rotates with arc
	ProjectileMovement->bShouldBounce           = false;
	ProjectileMovement->ProjectileGravityScale  = GravityScale; // overridden in InitializeArcProjectile

	// Auto-destroy after 6 seconds (exposed via AActor::InitialLifeSpan in Blueprint)
	InitialLifeSpan = 6.0f;
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void AEnemyArcProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap for player hit
	CollisionComp->OnComponentBeginOverlap.AddDynamic(
		this, &AEnemyArcProjectile::OnProjectileBeginOverlap);

	// Bind stop event so we self-destruct when the arc hits world geometry
	ProjectileMovement->OnProjectileStop.AddDynamic(
		this, &AEnemyArcProjectile::OnProjectileStopped);
}

// ---------------------------------------------------------------------------
// InitializeArcProjectile
// Must be called immediately after SpawnActor, before the next physics tick.
// ---------------------------------------------------------------------------

void AEnemyArcProjectile::InitializeArcProjectile(
	FVector LaunchVelocity, float InGravityScale, float InDamage)
{
	Damage       = InDamage;
	GravityScale = InGravityScale;

	if (ProjectileMovement)
	{
		// Apply gravity scale that matches the ballistic solver
		ProjectileMovement->ProjectileGravityScale = InGravityScale;

		// Set arc velocity directly.
		// Because InitialSpeed == 0, UProjectileMovementComponent::BeginPlay()
		// left Velocity at zero, so this assignment is the first real velocity.
		ProjectileMovement->Velocity = LaunchVelocity;
	}
}

// ---------------------------------------------------------------------------
// Collision Events
// ---------------------------------------------------------------------------

void AEnemyArcProjectile::OnProjectileBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	// Ignore self or the enemy that fired this projectile
	if (OtherActor == this || OtherActor == GetOwner())
	{
		return;
	}

	APawn* HitPawn = Cast<APawn>(OtherActor);
	if (HitPawn && HitPawn->IsPlayerControlled())
	{
		UGameplayStatics::ApplyDamage(
			HitPawn,
			Damage,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass());

		Destroy();
	}
}

void AEnemyArcProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	// Projectile hit world geometry and can no longer move – destroy it
	Destroy();
}
