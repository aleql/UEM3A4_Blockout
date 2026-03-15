// CameraShakeTrigger.cpp
// One-shot trigger: delay → start camera shake → stop after ShakeDuration.
//
// Design notes
// ─────────────
// • No Tick or Timeline needed – two FTimerHandles handle the delay and duration.
// • ShakeClass must be a Blueprint child of UCameraShakeBase with its own
//   oscillation/anim settings. Assign it in the Details panel.
// • bHasTriggered latched on first valid overlap; delegate unbound afterward.
// • If TriggerDelay is 0 the delay timer is skipped and StartShake fires immediately.

#include "CameraShakeTrigger.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCameraShakeTrigger, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ACameraShakeTrigger::ACameraShakeTrigger()
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

void ACameraShakeTrigger::BeginPlay()
{
	Super::BeginPlay();

	TriggerZone->OnComponentBeginOverlap.AddDynamic(
		this, &ACameraShakeTrigger::OnTriggerOverlapBegin);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTriggerOverlapBegin
// ─────────────────────────────────────────────────────────────────────────────

void ACameraShakeTrigger::OnTriggerOverlapBegin(
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

	// ── ShakeClass must be assigned ───────────────────────────────────────────
	if (!ShakeClass)
	{
		UE_LOG(LogCameraShakeTrigger, Warning,
			TEXT("[%s] ShakeClass is null – assign a UCameraShakeBase Blueprint in the Details panel. Aborting."),
			*GetName());
		return;
	}

	// ── Latch and unbind ──────────────────────────────────────────────────────
	bHasTriggered = true;
	TriggerZone->OnComponentBeginOverlap.RemoveDynamic(
		this, &ACameraShakeTrigger::OnTriggerOverlapBegin);

	if (bDebugLog)
	{
		UE_LOG(LogCameraShakeTrigger, Log,
			TEXT("[%s] Triggered by '%s' – shake starts in %.2fs, lasts %.2fs."),
			*GetName(), *Character->GetName(), TriggerDelay, ShakeDuration);
	}

	// ── Schedule shake start ──────────────────────────────────────────────────
	if (TriggerDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			DelayTimerHandle,
			this,
			&ACameraShakeTrigger::StartShake,
			TriggerDelay,
			/*bLooping=*/false);
	}
	else
	{
		StartShake();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// StartShake
// ─────────────────────────────────────────────────────────────────────────────

void ACameraShakeTrigger::StartShake()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager)
		return;

	PC->PlayerCameraManager->StartCameraShake(ShakeClass, ShakeScale);

	if (bDebugLog)
	{
		UE_LOG(LogCameraShakeTrigger, Log,
			TEXT("[%s] Camera shake started (scale %.2f) – will stop in %.2fs."),
			*GetName(), ShakeScale, ShakeDuration);
	}

	// ── Schedule shake stop ───────────────────────────────────────────────────
	GetWorld()->GetTimerManager().SetTimer(
		StopTimerHandle,
		this,
		&ACameraShakeTrigger::StopShake,
		ShakeDuration,
		/*bLooping=*/false);
}

// ─────────────────────────────────────────────────────────────────────────────
// StopShake
// ─────────────────────────────────────────────────────────────────────────────

void ACameraShakeTrigger::StopShake()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager)
		return;

	PC->PlayerCameraManager->StopAllInstancesOfCameraShake(ShakeClass, /*bImmediately=*/false);

	if (bDebugLog)
	{
		UE_LOG(LogCameraShakeTrigger, Log,
			TEXT("[%s] Camera shake stopped."), *GetName());
	}
}
