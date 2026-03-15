// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DialogTypes.h"
#include "DialogTriggerActor.generated.h"

class UBoxComponent;
class UDialogWidget;

UCLASS()
class UEM3A2BLOCKOUT_API ADialogTriggerActor : public AActor
{
	GENERATED_BODY()

public:
	ADialogTriggerActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	// Fallback duration (seconds) used for any line whose Duration is left at 0.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog", meta = (ClampMin = "0.1"))
	float DefaultLineDuration = 2.5f;

	// Lines to display in sequence. Set Duration on each line to override DefaultLineDuration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog")
	TArray<FDialogLine> DialogLines;

	// Widget Blueprint derived from UDialogWidget. Assign this in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog")
	TSubclassOf<UDialogWidget> DialogWidgetClass;

	// If true, the trigger fires only once per level load.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog")
	bool bTriggerOnlyOnce = true;

	// If true, this actor is destroyed after the dialog finishes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog")
	bool bDestroyAfterDialog = false;

private:
	UFUNCTION()
	void OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	void StartDialog();
	void ShowNextLine();
	void EndDialog();

	UPROPERTY()
	UDialogWidget* ActiveWidget;

	FTimerHandle LineTimerHandle;

	int32 CurrentLineIndex;
	bool bHasTriggered;
	bool bDialogPlaying;
};
