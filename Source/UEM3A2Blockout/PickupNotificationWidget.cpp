// Copyright Epic Games, Inc. All Rights Reserved.

#include "PickupNotificationWidget.h"
#include "Components/TextBlock.h"

void UPickupNotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Hidden);
}

void UPickupNotificationWidget::SetPickupText(const FText& NewText)
{
	if (PickupText)
	{
		PickupText->SetText(NewText);
	}
}

void UPickupNotificationWidget::ShowNotification(bool bShow)
{
	SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
