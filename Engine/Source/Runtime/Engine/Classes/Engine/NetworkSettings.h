// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkSettings.generated.h"


/**
 * Implements project settings for the Network sub-system.
 */
UCLASS(config=Engine, defaultconfig)
class ENGINE_API UNetworkSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category=libcurl, meta=(
		ConsoleVariable="n.VerifyPeer",DisplayName="Verify Peer",
		ToolTip="If true, libcurl authenticates the peer's certificate. Disable to allow self-signed certificates."))
	uint32 bVerifyPeer:1;

public:

	// Begin UObject interface

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// End UObject interface

};
