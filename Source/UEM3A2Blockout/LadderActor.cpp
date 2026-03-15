// LadderActor.cpp

#include "LadderActor.h"
#include "LadderClimbComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "UEM3A2Blockout.h"

ALadderActor::ALadderActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // ── Scene root ───────────────────────────────────────────────────────────
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // ── Ladder mesh ──────────────────────────────────────────────────────────
    LadderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMesh"));
    LadderMesh->SetupAttachment(Root);
    LadderMesh->SetCollisionProfileName(TEXT("BlockAll"));

    // ── Interact trigger volume ──────────────────────────────────────────────
    // Default extent covers a standard 3 m ladder; resize per mesh in the editor.
    InteractVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractVolume"));
    InteractVolume->SetupAttachment(Root);
    InteractVolume->SetBoxExtent(FVector(60.f, 60.f, 150.f));
    InteractVolume->SetCollisionProfileName(TEXT("Trigger"));
    InteractVolume->SetRelativeLocation(FVector(30.f, 0.f, 150.f));

    // ── Mount points ─────────────────────────────────────────────────────────
    BottomPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BottomPoint"));
    BottomPoint->SetupAttachment(Root);
    BottomPoint->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

    TopPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TopPoint"));
    TopPoint->SetupAttachment(Root);
    TopPoint->SetRelativeLocation(FVector(0.f, 0.f, 300.f)); // default 3 m; adjust per mesh

    // ── Facing arrow ─────────────────────────────────────────────────────────
    FacingDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingDirection"));
    FacingDirection->SetupAttachment(Root);
    FacingDirection->ArrowColor = FColor::Blue;
    FacingDirection->ArrowSize  = 2.f;
}

void ALadderActor::BeginPlay()
{
    Super::BeginPlay();

    InteractVolume->OnComponentBeginOverlap.AddDynamic(this, &ALadderActor::OnInteractVolumeBegin);
    InteractVolume->OnComponentEndOverlap.AddDynamic(this, &ALadderActor::OnInteractVolumeEnd);
}

// ── Overlap handlers ──────────────────────────────────────────────────────────

void ALadderActor::OnInteractVolumeBegin(
    UPrimitiveComponent* OverlappedComp,
    AActor*              OtherActor,
    UPrimitiveComponent* OtherComp,
    int32                OtherBodyIndex,
    bool                 bFromSweep,
    const FHitResult&    SweepResult)
{
    if (ACharacter* Char = Cast<ACharacter>(OtherActor))
    {
        if (ULadderClimbComponent* Comp = Char->FindComponentByClass<ULadderClimbComponent>())
        {
            Comp->SetCurrentLadder(this);
        }
    }
}

void ALadderActor::OnInteractVolumeEnd(
    UPrimitiveComponent* OverlappedComp,
    AActor*              OtherActor,
    UPrimitiveComponent* OtherComp,
    int32                OtherBodyIndex)
{
    if (ACharacter* Char = Cast<ACharacter>(OtherActor))
    {
        if (ULadderClimbComponent* Comp = Char->FindComponentByClass<ULadderClimbComponent>())
        {
            // Only clear while not climbing; EndClimb() handles the mid-climb case.
            if (!Comp->bIsClimbing)
            {
                Comp->SetCurrentLadder(nullptr);
            }
        }
    }
}

// ── Getters ───────────────────────────────────────────────────────────────────

FVector ALadderActor::GetBottomWorld() const
{
    return BottomPoint ? BottomPoint->GetComponentLocation() : GetActorLocation();
}

FVector ALadderActor::GetTopWorld() const
{
    return TopPoint ? TopPoint->GetComponentLocation() : GetActorLocation();
}

float ALadderActor::GetHeight() const
{
    return FVector::Dist(GetBottomWorld(), GetTopWorld());
}

FVector ALadderActor::GetForwardDirection() const
{
    return FacingDirection ? FacingDirection->GetForwardVector() : GetActorForwardVector();
}
