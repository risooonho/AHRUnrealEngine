// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWrapperTypes.h"

#include "SWidgetSwitcher.h"
#include "WidgetSwitcherSlot.generated.h"

/** The Slot for the UWidgetSwitcher, contains the widget that is flowed vertically */
UCLASS()
class UMG_API UWidgetSwitcherSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:
	
	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout (Widget Switcher Slot)")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout (Widget Switcher Slot)")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout (Widget Switcher Slot)")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	UFUNCTION(BlueprintCallable, Category="Layout (Widget Switcher Slot)")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout (Widget Switcher Slot)")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Layout (Widget Switcher Slot)")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SWidgetSwitcher> InWidgetSwitcher);

private:
	//TODO UMG Slots should hold weak or shared refs to slots.

	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SWidgetSwitcher::FSlot* Slot;
};
