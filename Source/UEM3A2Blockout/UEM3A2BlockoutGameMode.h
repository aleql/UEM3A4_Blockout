// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UEM3A2BlockoutGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AUEM3A2BlockoutGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AUEM3A2BlockoutGameMode();
};



