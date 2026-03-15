// ElevatorRemoteTrigger.cpp
// Remote lever trigger for AElevatorActor.
// Contains zero movement logic – it only calls TargetElevator->RequestToggle().

#include "ElevatorRemoteTrigger.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRemoteTrigger, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AElevatorRemoteTrigger::AElevatorRemoteTrigger()
{
    PrimaryActorTick.bCanEverTick = false; // nothing to tick

    // ── Root ─────────────────────────────────────────────────────────────────
    TriggerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TriggerRoot"));
    SetRootComponent(TriggerRoot);

    // ── Trigger box ──────────────────────────────────────────────────────────
    TriggerZone = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerZone"));
    TriggerZone->SetupAttachment(TriggerRoot);
    TriggerZone->SetBoxExtent(FVector(80.f, 80.f, 60.f));

    TriggerZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerZone->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    TriggerZone->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
    TriggerZone->SetGenerateOverlapEvents(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorRemoteTrigger::BeginPlay()
{
    Super::BeginPlay();

    // Warn early if the designer forgot to wire the elevator reference.
    if (!IsValid(TargetElevator))
    {
        UE_LOG(LogRemoteTrigger, Warning,
            TEXT("[%s] TargetElevator is not assigned – this trigger will do nothing. "
                 "Assign it in the Details panel."),
            *GetName());
    }

    TriggerZone->OnComponentBeginOverlap.AddDynamic(
        this, &AElevatorRemoteTrigger::OnTriggerOverlapBegin);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnTriggerOverlapBegin
// ─────────────────────────────────────────────────────────────────────────────

void AElevatorRemoteTrigger::OnTriggerOverlapBegin(
    UPrimitiveComponent* /*OverlappedComponent*/,
    AActor*              OtherActor,
    UPrimitiveComponent* /*OtherComp*/,
    int32                /*OtherBodyIndex*/,
    bool                 /*bFromSweep*/,
    const FHitResult&    /*SweepResult*/)
{
    if (bDebugLog)
        UE_LOG(LogRemoteTrigger, Log, TEXT("[%s] Overlap received from %s"),
            *GetName(), *GetNameSafe(OtherActor));

    // ── One-shot latch ────────────────────────────────────────────────────────
    if (bOneShot && bFired)
    {
        if (bDebugLog)
            UE_LOG(LogRemoteTrigger, Log, TEXT("[%s] One-shot already fired – ignoring."), *GetName());
        return;
    }

    // ── Must be the local player character ────────────────────────────────────
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character)
    {
        if (bDebugLog)
            UE_LOG(LogRemoteTrigger, Log, TEXT("[%s] Overlap ignored – not a Character."), *GetName());
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || PC->GetPawn() != Character)
    {
        if (bDebugLog)
            UE_LOG(LogRemoteTrigger, Log, TEXT("[%s] Overlap ignored – not the local player."), *GetName());
        return;
    }

    // ── Null check on target ──────────────────────────────────────────────────
    if (!IsValid(TargetElevator))
    {
        UE_LOG(LogRemoteTrigger, Warning,
            TEXT("[%s] TargetElevator is null – assign it in the Details panel."), *GetName());
        return;
    }

    // ── Delegate to the elevator ──────────────────────────────────────────────
    if (bDebugLog)
        UE_LOG(LogRemoteTrigger, Log,
            TEXT("[%s] Calling RequestToggle on '%s'."),
            *GetName(), *TargetElevator->GetName());

    TargetElevator->RequestToggle(Character);

    // ── One-shot: unbind after first successful call ──────────────────────────
    if (bOneShot)
    {
        bFired = true;
        TriggerZone->OnComponentBeginOverlap.RemoveDynamic(
            this, &AElevatorRemoteTrigger::OnTriggerOverlapBegin);

        if (bDebugLog)
            UE_LOG(LogRemoteTrigger, Log, TEXT("[%s] One-shot fired – overlap unbound."), *GetName());
    }
}
