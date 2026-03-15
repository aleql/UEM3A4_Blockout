// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DialogTypes.generated.h"

USTRUCT(BlueprintType)
struct FDialogLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog")
	FText Text;

	// Duration this line stays visible in seconds.
	// Leave at 0 to use the DefaultLineDuration set on the trigger actor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog", meta = (ClampMin = "0.0"))
	float Duration = 0.f;
};
