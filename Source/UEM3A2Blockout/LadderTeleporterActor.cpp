// LadderTeleporterActor.cpp

#include "LadderTeleporterActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UEM3A2Blockout.h"

ALadderTeleporterActor::ALadderTeleporterActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // ── Root ─────────────────────────────────────────────────────────────────
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // ── Ladder mesh ──────────────────────────────────────────────────────────
    LadderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMesh"));
    LadderMesh->SetupAttachment(Root);
    LadderMesh->SetCollisionProfileName(TEXT("BlockAll"));

    // ── Helper: applies the same trigger settings to both boxes ──────────────
    auto SetupTrigger = [](UBoxComponent* Box, const FVector& Extent, const FVector& LocalOffset)
    {
        Box->SetBoxExtent(Extent);
        Box->SetRelativeLocation(LocalOffset);
        Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetGenerateOverlapEvents(true);
    };

    // ── Triggers ─────────────────────────────────────────────────────────────
    // Default extents fit a standard capsule (42 r, 96 half-height).
    // Reposition and resize in the editor to match your mesh.

    BottomTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("BottomTrigger"));
    BottomTrigger->SetupAttachment(Root);
    SetupTrigger(BottomTrigger, FVector(50.f, 50.f, 50.f), FVector(0.f, 0.f, 50.f));

    TopTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("TopTrigger"));
    TopTrigger->SetupAttachment(Root);
    SetupTrigger(TopTrigger, FVector(50.f, 50.f, 50.f), FVector(0.f, 0.f, 250.f));

    // ── Targets ───────────────────────────────────────────────────────────────
    // Move these in the editor; they define exact teleport destinations.

    BottomTarget = CreateDefaultSubobject<USceneComponent>(TEXT("BottomTarget"));
    BottomTarget->SetupAttachment(Root);
    BottomTarget->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

    TopTarget = CreateDefaultSubobject<USceneComponent>(TEXT("TopTarget"));
    TopTarget->SetupAttachment(Root);
    TopTarget->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
}

void ALadderTeleporterActor::BeginPlay()
{
    Super::BeginPlay();

    BottomTrigger->OnComponentBeginOverlap.AddDynamic(this, &ALadderTeleporterActor::OnBottomOverlap);
    TopTrigger->OnComponentBeginOverlap.AddDynamic(this, &ALadderTeleporterActor::OnTopOverlap);
}

// ── Internal helpers ──────────────────────────────────────────────────────────

void ALadderTeleporterActor::CleanupStaleEntries()
{
    for (auto It = LastTeleportTimes.CreateIterator(); It; ++It)
    {
        if (!It->Key.IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

void ALadderTeleporterActor::TryTeleport(AActor* OtherActor, USceneComponent* Target)
{
    // ── Basic validation ──────────────────────────────────────────────────────
    if (!IsValid(OtherActor)) return;
    if (!Target)              return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    // ── Cooldown check ────────────────────────────────────────────────────────
    const float                 Now      = GetWorld()->GetTimeSeconds();
    const TWeakObjectPtr<AActor> WeakKey(OtherActor);

    if (const float* LastTime = LastTeleportTimes.Find(WeakKey))
    {
        if (Now - *LastTime < TeleportCooldown)
        {
            return;
        }
    }

    // ── Teleport ──────────────────────────────────────────────────────────────
    // Preserve the character's current yaw so they do not snap to a foreign rotation.
    const FVector  Destination = Target->GetComponentLocation();
    const FRotator Rotation    = Character->GetActorRotation();

    const bool bSuccess = Character->TeleportTo(Destination, Rotation, false, true);

    if (!bSuccess)
    {
        UE_LOG(LogUEM3A2Blockout, Warning,
            TEXT("ALadderTeleporterActor: TeleportTo failed for '%s'"),
            *Character->GetName());
        return;
    }

    // ── Stop residual velocity ────────────────────────────────────────────────
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        MoveComp->StopMovementImmediately();
    }

    // ── Record timestamp and purge stale map entries ──────────────────────────
    LastTeleportTimes.Add(WeakKey, Now);
    CleanupStaleEntries();
}

// ── Overlap callbacks ─────────────────────────────────────────────────────────

void ALadderTeleporterActor::OnBottomOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor*              OtherActor,
    UPrimitiveComponent* OtherComp,
    int32                OtherBodyIndex,
    bool                 bFromSweep,
    const FHitResult&    SweepResult)
{
    TryTeleport(OtherActor, TopTarget);
}

void ALadderTeleporterActor::OnTopOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor*              OtherActor,
    UPrimitiveComponent* OtherComp,
    int32                OtherBodyIndex,
    bool                 bFromSweep,
    const FHitResult&    SweepResult)
{
    TryTeleport(OtherActor, BottomTarget);
}
