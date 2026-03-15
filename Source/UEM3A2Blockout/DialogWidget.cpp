// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogWidget.h"
#include "Components/TextBlock.h"

void UDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Hidden);
}

void UDialogWidget::SetDialogText(const FText& NewText)
{
	if (DialogText)
	{
		DialogText->SetText(NewText);
	}
}

void UDialogWidget::ShowDialog(bool bShow)
{
	SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
