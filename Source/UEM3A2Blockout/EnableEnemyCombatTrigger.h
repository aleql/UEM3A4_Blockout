// EnableEnemyCombatTrigger.h
// One-shot trigger that enables combat AI on a linked billboard enemy.
// While the enemy has bDisableCombatAI = true it stands idle; this trigger
// flips it to false so it begins reacting to the player.
// Fires once, then permanently disables itself (same pattern as RaiseCylinderTrigger).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "EnableEnemyCombatTrigger.generated.h"

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom),
	meta = (BlueprintSpawnableComponent))
class UEM3A2BLOCKOUT_API AEnableEnemyCombatTrigger : public AActor
{
	GENERATED_BODY()

public:
	AEnableEnemyCombatTrigger();

protected:
	virtual void BeginPlay() override;

	// ── Components ────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger|Components",
		meta = (AllowPrivateAccess = "true"))
	USceneComponent* TriggerRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger|Components",
		meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerZone;

	// ── Designer settings ─────────────────────────────────────────────────────

	/**
	 * The billboard enemy to activate. Assign a level instance in the Details panel.
	 * Accepts ABillboardEnemy, AArcBillboardEnemy, or ABillboardMeleeEnemy.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Trigger|Settings")
	AActor* TargetEnemy = nullptr;

	/** Print a log line to the Output Log when the trigger fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger|Debug")
	bool bDebugLog = false;

	// ── Runtime state ─────────────────────────────────────────────────────────

	/** Latched on first fire – prevents any re-entry. */
	bool bHasTriggered = false;

	// ── Callbacks ─────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnTriggerOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor*              OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                OtherBodyIndex,
		bool                 bFromSweep,
		const FHitResult&    SweepResult);
};
