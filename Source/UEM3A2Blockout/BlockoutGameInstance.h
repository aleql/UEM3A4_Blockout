// BlockoutGameInstance.h
// Persists bGlobalAIEnabled across level reloads.
// Set this class as the Game Instance in Project Settings → Maps & Modes.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BlockoutGameInstance.generated.h"

UCLASS()
class UEM3A2BLOCKOUT_API UBlockoutGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/**
	 * When true  (default) enemy combat AI is active normally.
	 * When false all enemy combat AI is suppressed; EnableEnemyCombatTriggers
	 * are also silenced until AI is toggled back on and the level restarts.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AI")
	bool bGlobalAIEnabled = true;
};
