// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PickupNotificationWidget.generated.h"

class UTextBlock;

UCLASS()
class UEM3A2BLOCKOUT_API UPickupNotificationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void SetPickupText(const FText& NewText);

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void ShowNotification(bool bShow);

protected:
	virtual void NativeConstruct() override;

	// Must be named exactly "PickupText" in the Widget Blueprint for BindWidget to work.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PickupText;
};
