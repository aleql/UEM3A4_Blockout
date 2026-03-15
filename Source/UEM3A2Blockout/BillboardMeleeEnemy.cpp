// BillboardMeleeEnemy.cpp
#include "BillboardMeleeEnemy.h"

#include "BlockoutGameInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "TwoPointSplinePatrolComponent.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ABillboardMeleeEnemy::ABillboardMeleeEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

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
	FlipbookComp->SetRelativeLocation(FVector(0.f, 0.f, -88.f));

	AggroTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("AggroTrigger"));
	AggroTrigger->SetupAttachment(CapsuleComp);
	AggroTrigger->SetSphereRadius(400.0f);
	AggroTrigger->SetCollisionProfileName(TEXT("Trigger"));

	PatrolComp = CreateDefaultSubobject<UTwoPointSplinePatrolComponent>(TEXT("PatrolComp"));
	// Melee enemy owns combat animations; patrol component only drives Walk/Idle
	PatrolComp->bDriveFlipbooks       = true;
	PatrolComp->bCombatOwnsAnimations = true;
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::BeginPlay()
{
	// Wire patrol comp flipbook assets BEFORE Super triggers component BeginPlay,
	// so ResolveFlipbookComponent() inside the patrol comp's BeginPlay sees them.
	PatrolComp->WalkFlipbook       = WalkFlipbook;
	PatrolComp->IdleFlipbook       = IdleFlipbook;
	PatrolComp->FlipbookCompOverride = FlipbookComp;

	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	AggroTrigger->OnComponentBeginOverlap.AddDynamic(
		this, &ABillboardMeleeEnemy::OnAggroBeginOverlap);
	AggroTrigger->OnComponentEndOverlap.AddDynamic(
		this, &ABillboardMeleeEnemy::OnAggroEndOverlap);

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
// Tick
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDead)
	{
		return;
	}

	// Always face the player regardless of state
	APawn* Player = TargetPlayer.IsValid()
		? TargetPlayer.Get()
		: UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Player)
	{
		UpdateFacingToward(Player->GetActorLocation());
	}

	switch (CurrentState)
	{
	case EMeleeEnemyState::Chasing:
		TickChasing(DeltaTime);
		break;

	case EMeleeEnemyState::ReturningToPatrolStart:
		TickReturningToPatrolStart(DeltaTime);
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// TakeDamage
// ---------------------------------------------------------------------------

float ABillboardMeleeEnemy::TakeDamage(float DamageAmount,
	struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.0f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage <= 0.0f)
	{
		return 0.0f;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - ActualDamage);

	if (CurrentHealth <= 0.0f)
	{
		Die();
		return ActualDamage;
	}

	// Do not interrupt a death or an already-playing damage reaction
	if (CurrentState == EMeleeEnemyState::Dying ||
		CurrentState == EMeleeEnemyState::Damaged)
	{
		return ActualDamage;
	}

	// Interrupt attack windup so we don't deal damage after being stunned
	if (CurrentState == EMeleeEnemyState::Attacking)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AttackWindupTimer);
			World->GetTimerManager().ClearTimer(AttackEndTimer);
		}
	}

	SetState(EMeleeEnemyState::Damaged);

	const float ReactDuration = GetFlipbookDuration(DamageFlipbook) > 0.0f
		? GetFlipbookDuration(DamageFlipbook)
		: DamageReactDuration;

	GetWorld()->GetTimerManager().SetTimer(
		DamageReactTimer,
		this,
		&ABillboardMeleeEnemy::OnDamageReactEnd,
		ReactDuration,
		/*bLooping=*/false);

	return ActualDamage;
}

// ---------------------------------------------------------------------------
// Overlap Events
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::OnAggroBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsDead || bDisableCombatAI)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	// Cancel any pending return-to-patrol delay
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResumePatrolTimer);
	}

	TargetPlayer = Pawn;

	// Suspend patrol (idempotent if already active)
	PatrolComp->NotifyCombatStarted();

	// Only switch to Chasing from non-combat states; don't interrupt an attack
	if (CurrentState == EMeleeEnemyState::Patrolling ||
		CurrentState == EMeleeEnemyState::ReturningToPatrolStart)
	{
		SetState(EMeleeEnemyState::Chasing);
	}
}

void ABillboardMeleeEnemy::OnAggroEndOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (bIsDead)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || Pawn != TargetPlayer.Get())
	{
		return;
	}

	TargetPlayer = nullptr;

	// Cancel the damage windup so the player isn't hit after leaving,
	// but let AttackEndTimer run to completion so OnAttackEnd() can
	// cleanly transition the state out of Attacking.
	if (CurrentState == EMeleeEnemyState::Attacking)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AttackWindupTimer);
		}
	}

	// Never interrupt death
	if (CurrentState == EMeleeEnemyState::Dying)
	{
		return;
	}

	// Start the return-to-patrol delay.
	// Works even while Damaged: OnDamageReactEnd will check the timer state.
	if (ResumePatrolDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ResumePatrolTimer,
			this,
			&ABillboardMeleeEnemy::OnResumePatrolTimerExpired,
			ResumePatrolDelay,
			/*bLooping=*/false);
	}
	else
	{
		OnResumePatrolTimerExpired();
	}
}

// ---------------------------------------------------------------------------
// State Machine
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::SetState(EMeleeEnemyState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	CurrentState = NewState;

	switch (NewState)
	{
	case EMeleeEnemyState::Patrolling:
		// Patrol component takes over flipbook control from here.
		// Clear our dedup so the patrol comp can immediately apply WalkFlipbook.
		LastSetFlipbook = nullptr;
		break;

	case EMeleeEnemyState::ReturningToPatrolStart:
		SetEnemyFlipbook(RunFlipbook, /*bLooping=*/true);
		break;

	case EMeleeEnemyState::Chasing:
		SetEnemyFlipbook(RunFlipbook, /*bLooping=*/true);
		break;

	case EMeleeEnemyState::Attacking:
		SetEnemyFlipbook(AttackFlipbook, /*bLooping=*/false);
		break;

	case EMeleeEnemyState::Damaged:
		SetEnemyFlipbook(DamageFlipbook, /*bLooping=*/false);
		break;

	case EMeleeEnemyState::Dying:
		SetEnemyFlipbook(DieFlipbook, /*bLooping=*/false);
		break;
	}
}

// ---------------------------------------------------------------------------
// Tick Routines
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::TickChasing(float DeltaTime)
{
	if (!TargetPlayer.IsValid())
	{
		// Lost the weak pointer but EndOverlap hasn't fired; do nothing this tick
		return;
	}

	const FVector MyLoc     = GetActorLocation();
	const FVector PlayerLoc = TargetPlayer->GetActorLocation();
	const FVector ToPlayer  = PlayerLoc - MyLoc;
	const float   Dist      = ToPlayer.Size();

	if (Dist <= MeleeRange)
	{
		if (!bAttackOnCooldown)
		{
			StartAttack();
		}
		// If on cooldown, stand still until cooldown clears
		return;
	}

	// Move toward player, stopping exactly at MeleeRange so we don't overshoot
	const float   Step    = FMath::Min(ChaseSpeed * DeltaTime, Dist - MeleeRange);
	const FVector MoveDir = ToPlayer.GetSafeNormal();
	SetActorLocation(MyLoc + MoveDir * Step);
}

void ABillboardMeleeEnemy::TickReturningToPatrolStart(float DeltaTime)
{
	const FVector MyLoc    = GetActorLocation();
	const FVector ToTarget = ReturnTarget - MyLoc;
	const float   Dist     = ToTarget.Size();

	if (Dist <= ReturnArriveDistance)
	{
		SetActorLocation(ReturnTarget);
		// Hand movement back to the patrol component: A→B
		PatrolComp->ResumePatrolFromStartPoint();
		SetState(EMeleeEnemyState::Patrolling);
		return;
	}

	const float   Step    = FMath::Min(PatrolReturnSpeed * DeltaTime, Dist);
	const FVector MoveDir = ToTarget.GetSafeNormal();
	SetActorLocation(MyLoc + MoveDir * Step);
}

// ---------------------------------------------------------------------------
// Attack
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::StartAttack()
{
	SetState(EMeleeEnemyState::Attacking);

	// Apply damage after windup
	GetWorld()->GetTimerManager().SetTimer(
		AttackWindupTimer,
		this,
		&ABillboardMeleeEnemy::ApplyAttackDamage,
		AttackWindupTime,
		/*bLooping=*/false);

	// End attack state after the full duration
	GetWorld()->GetTimerManager().SetTimer(
		AttackEndTimer,
		this,
		&ABillboardMeleeEnemy::OnAttackEnd,
		AttackDuration,
		/*bLooping=*/false);
}

void ABillboardMeleeEnemy::ApplyAttackDamage()
{
	if (!TargetPlayer.IsValid())
	{
		return;
	}

	// Leniency check: if the player dodged well out of range don't deal damage
	const float Dist = FVector::Dist(GetActorLocation(), TargetPlayer->GetActorLocation());
	if (Dist > MeleeRange * 1.5f)
	{
		return;
	}

	UGameplayStatics::ApplyDamage(TargetPlayer.Get(), AttackDamage, nullptr, this, nullptr);
}

void ABillboardMeleeEnemy::OnAttackEnd()
{
	if (bIsDead)
	{
		return;
	}

	bAttackOnCooldown = true;

	if (AttackCooldown > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			AttackCooldownTimer,
			this,
			&ABillboardMeleeEnemy::OnAttackCooldownEnd,
			AttackCooldown,
			/*bLooping=*/false);
	}
	else
	{
		bAttackOnCooldown = false;
	}

	// Resume chasing if the player is still in range
	if (TargetPlayer.IsValid())
	{
		SetState(EMeleeEnemyState::Chasing);
	}
	// Otherwise the ResumePatrolTimer is already running from OnAggroEndOverlap
}

void ABillboardMeleeEnemy::OnAttackCooldownEnd()
{
	bAttackOnCooldown = false;
}

// ---------------------------------------------------------------------------
// Patrol Resume
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::OnResumePatrolTimerExpired()
{
	if (bIsDead)
	{
		return;
	}

	// Re-entered aggro while waiting; abort return
	if (TargetPlayer.IsValid())
	{
		return;
	}

	// Don't interrupt damage or death; OnDamageReactEnd will call us when ready
	if (CurrentState == EMeleeEnemyState::Dying ||
		CurrentState == EMeleeEnemyState::Damaged)
	{
		return;
	}

	// Cache patrol start location, then begin moving back
	ReturnTarget = PatrolComp->GetPatrolPointWorldLocation(0);
	SetState(EMeleeEnemyState::ReturningToPatrolStart);
}

// ---------------------------------------------------------------------------
// Damage & Death
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::OnDamageReactEnd()
{
	if (bIsDead)
	{
		return;
	}

	if (TargetPlayer.IsValid())
	{
		SetState(EMeleeEnemyState::Chasing);
	}
	else
	{
		// If ResumePatrolTimer is still pending, it will call OnResumePatrolTimerExpired.
		// If it already fired (or ResumePatrolDelay == 0) we call it directly now.
		if (!GetWorld()->GetTimerManager().IsTimerActive(ResumePatrolTimer))
		{
			OnResumePatrolTimerExpired();
		}
	}
}

void ABillboardMeleeEnemy::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackWindupTimer);
		World->GetTimerManager().ClearTimer(AttackEndTimer);
		World->GetTimerManager().ClearTimer(AttackCooldownTimer);
		World->GetTimerManager().ClearTimer(DamageReactTimer);
		World->GetTimerManager().ClearTimer(ResumePatrolTimer);
	}

	PatrolComp->StopPatrol();

	AggroTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SetState(EMeleeEnemyState::Dying);

	if (bDestroyOnDeath)
	{
		const float DieDuration = GetFlipbookDuration(DieFlipbook);
		const float Delay       = (DieDuration > 0.0f ? DieDuration : 0.0f) + DeathDestroyDelay;

		GetWorld()->GetTimerManager().SetTimer(
			DeathTimer,
			this,
			&ABillboardMeleeEnemy::HandleDeathComplete,
			FMath::Max(Delay, 0.01f),
			/*bLooping=*/false);
	}
}

void ABillboardMeleeEnemy::HandleDeathComplete()
{
	Destroy();
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::SetEnemyFlipbook(UPaperFlipbook* Flipbook, bool bLooping)
{
	if (!Flipbook || !FlipbookComp)
	{
		return;
	}

	if (Flipbook == LastSetFlipbook)
	{
		return;
	}

	FlipbookComp->SetFlipbook(Flipbook);
	FlipbookComp->SetLooping(bLooping);
	FlipbookComp->Play();

	LastSetFlipbook = Flipbook;
}

float ABillboardMeleeEnemy::GetFlipbookDuration(UPaperFlipbook* Flipbook) const
{
	return Flipbook ? Flipbook->GetTotalDuration() : 0.0f;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

void ABillboardMeleeEnemy::UpdateFacingToward(const FVector& TargetLocation)
{
	const FVector ToTarget = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}
}
