// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogTriggerActor.h"
#include "DialogWidget.h"
#include "Components/BoxComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UEM3A2Blockout.h"

ADialogTriggerActor::ADialogTriggerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(SceneRoot);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	ActiveWidget = nullptr;
	CurrentLineIndex = 0;
	bHasTriggered = false;
	bDialogPlaying = false;
}

void ADialogTriggerActor::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADialogTriggerActor::OnTriggerOverlapBegin);
}

void ADialogTriggerActor::OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bDialogPlaying) return;
	if (bTriggerOnlyOnce && bHasTriggered) return;

	const ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!PlayerCharacter || OtherActor != PlayerCharacter) return;

	StartDialog();
}

void ADialogTriggerActor::StartDialog()
{
	if (DialogLines.Num() == 0)
	{
		UE_LOG(LogUEM3A2Blockout, Warning,
			TEXT("ADialogTriggerActor '%s': DialogLines is empty. Add lines in the editor."), *GetName());
		return;
	}

	if (!DialogWidgetClass)
	{
		UE_LOG(LogUEM3A2Blockout, Warning,
			TEXT("ADialogTriggerActor '%s': DialogWidgetClass is not set. Assign the Widget Blueprint in the editor."), *GetName());
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	ActiveWidget = CreateWidget<UDialogWidget>(PC, DialogWidgetClass);
	if (!ActiveWidget) return;

	ActiveWidget->AddToViewport();

	bDialogPlaying = true;
	bHasTriggered = true;
	CurrentLineIndex = 0;

	ShowNextLine();
}

void ADialogTriggerActor::ShowNextLine()
{
	if (CurrentLineIndex >= DialogLines.Num())
	{
		EndDialog();
		return;
	}

	const FDialogLine& Line = DialogLines[CurrentLineIndex];
	ActiveWidget->SetDialogText(Line.Text);
	ActiveWidget->ShowDialog(true);

	CurrentLineIndex++;

	const float LineDuration = Line.Duration > 0.f ? Line.Duration : DefaultLineDuration;
	GetWorldTimerManager().SetTimer(
		LineTimerHandle,
		this,
		&ADialogTriggerActor::ShowNextLine,
		LineDuration,
		false
	);
}

void ADialogTriggerActor::EndDialog()
{
	GetWorldTimerManager().ClearTimer(LineTimerHandle);

	if (ActiveWidget)
	{
		ActiveWidget->ShowDialog(false);
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}

	bDialogPlaying = false;

	if (bDestroyAfterDialog)
	{
		Destroy();
	}
}
