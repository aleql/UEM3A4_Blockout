// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DialogWidget.generated.h"

class UTextBlock;

UCLASS()
class UEM3A2BLOCKOUT_API UDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Dialog")
	void SetDialogText(const FText& NewText);

	UFUNCTION(BlueprintCallable, Category = "Dialog")
	void ShowDialog(bool bShow);

protected:
	virtual void NativeConstruct() override;

	// Must be named exactly "DialogText" in the Widget Blueprint for BindWidget to work.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DialogText;
};
