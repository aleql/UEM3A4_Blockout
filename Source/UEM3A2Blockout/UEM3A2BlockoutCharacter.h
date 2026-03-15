// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "LadderClimbComponent.h"
#include "UEM3A2BlockoutCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AUEM3A2BlockoutCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Interact Input Action — bind to E key in the Input Mapping Context */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	/** Ladder climb component — handles all climbing state and movement */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components",
		meta = (AllowPrivateAccess = "true"))
	ULadderClimbComponent* LadderClimbComponent;

	/**
	 * Minimum view pitch in degrees. Negative = looking up.
	 * Default -89.9 (full range). Example: -20 caps upward look to 20°.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ViewPitchMin = -89.9f;

	/**
	 * Maximum view pitch in degrees. Positive = looking down.
	 * Default 89.9 (full range). Example: 60 caps downward look to 60°.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ViewPitchMax = 89.9f;

public:

	/** Constructor */
	AUEM3A2BlockoutCharacter();

	/**
	 * Flips bGlobalAIEnabled in the GameInstance, prints a status message,
	 * then reloads the current level so the new flag is applied from BeginPlay.
	 * Bound to the U key in SetupPlayerInputComponent.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ToggleAIMode();

protected:

	/** Applies ViewPitchMin/Max to the possessing PlayerController's camera manager. */
	virtual void PossessedBy(AController* NewController) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called when the player presses the interact key (E) */
	void Interact();

public:

	/**
	 * Handles move inputs from either controls or UI interfaces.
	 * When climbing, Forward drives ClimbInput instead of normal movement.
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

