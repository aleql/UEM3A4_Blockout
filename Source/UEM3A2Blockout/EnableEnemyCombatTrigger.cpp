// EnableEnemyCombatTrigger.cpp
// One-shot trigger that enables combat AI on a linked billboard enemy.
//
// Design notes
// ─────────────
// • No Tick, no Timeline – activating combat is a one-line bool flip.
// • TargetEnemy is AActor* so any of the three billboard enemy types can be
//   assigned in the Details panel. The overlap casts to each in turn.
// • bHasTriggered is latched immediately and the overlap delegate is unbound,
//   matching the zero-cost-after-fire pattern in RaiseCylinderTrigger.

#include "EnableEnemyCombatTrigger.h"

#include "BlockoutGameInstance.h"
#include "ArcBillboardEnemy.h"
#include "BillboardEnemy.h"
#include "BillboardMeleeEnemy.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnableEnemyCombatTrigger, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AEnableEnemyCombatTrigger::AEnableEnemyCombatTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	// ── Root ─────────────────────────────────────────────────────────────────
	TriggerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TriggerRoot"));
	SetRootComponent(TriggerRoot);

	// ── Trigger box ──────────────────────────────────────────────────────────
	TriggerZone = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerZone"));
	TriggerZone->SetupAttachment(TriggerRoot);
	TriggerZone->SetBoxExtent(FVector(100.f, 100.f, 50.f));

	TriggerZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerZone->SetGenerateOverlapEvents(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AEnableEnemyCombatTrigger::BeginPlay()
{
	Super::BeginPlay();

	TriggerZone->OnComponentBeginOverlap.AddDynamic(
		this, &AEnableEnemyCombatTrigger::OnTriggerOverlapBegin);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTriggerOverlapBegin
// ─────────────────────────────────────────────────────────────────────────────

void AEnableEnemyCombatTrigger::OnTriggerOverlapBegin(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor*              OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32                /*OtherBodyIndex*/,
	bool                 /*bFromSweep*/,
	const FHitResult&    /*SweepResult*/)
{
	// ── One-shot latch ────────────────────────────────────────────────────────
	if (bHasTriggered)
		return;

	// ── Must be the local player character ────────────────────────────────────
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character)
		return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || PC->GetPawn() != Character)
		return;

	// ── Silenced when global AI is off (don't latch — trigger can fire later) ─
	if (UBlockoutGameInstance* GI = GetGameInstance<UBlockoutGameInstance>())
	{
		if (!GI->bGlobalAIEnabled)
			return;
	}

	// ── Defensive: TargetEnemy must be valid ──────────────────────────────────
	if (!IsValid(TargetEnemy))
	{
		UE_LOG(LogEnableEnemyCombatTrigger, Warning,
			TEXT("[%s] TargetEnemy is null – assign it in the Details panel. Aborting."),
			*GetName());
		return;
	}

	// ── Latch and unbind – zero cost from here on ─────────────────────────────
	bHasTriggered = true;
	TriggerZone->OnComponentBeginOverlap.RemoveDynamic(
		this, &AEnableEnemyCombatTrigger::OnTriggerOverlapBegin);

	// ── Enable combat on whichever billboard enemy type is assigned ───────────
	bool bActivated = false;

	if (ABillboardEnemy* Billboard = Cast<ABillboardEnemy>(TargetEnemy))
	{
		Billboard->bDisableCombatAI = false;
		bActivated = true;
	}
	else if (AArcBillboardEnemy* Arc = Cast<AArcBillboardEnemy>(TargetEnemy))
	{
		Arc->bDisableCombatAI = false;
		bActivated = true;
	}
	else if (ABillboardMeleeEnemy* Melee = Cast<ABillboardMeleeEnemy>(TargetEnemy))
	{
		Melee->bDisableCombatAI = false;
		bActivated = true;
	}

	if (bActivated)
	{
		if (bDebugLog)
		{
			UE_LOG(LogEnableEnemyCombatTrigger, Log,
				TEXT("[%s] Triggered by '%s' – combat enabled on '%s'."),
				*GetName(), *Character->GetName(), *TargetEnemy->GetName());
		}
	}
	else
	{
		UE_LOG(LogEnableEnemyCombatTrigger, Warning,
			TEXT("[%s] TargetEnemy '%s' is not a recognized billboard enemy type. "
				"Assign an ABillboardEnemy, AArcBillboardEnemy, or ABillboardMeleeEnemy."),
			*GetName(), *TargetEnemy->GetName());
	}
}
