// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineDelegateMacros.h"
#include "OnlineStoreInterface.generated.h"

/**
 * Possible result states of an in-app purchase transaction
 */
UENUM(BlueprintType)
namespace EInAppPurchaseState
{
	enum Type
	{
		Success = 0 UMETA(DisplayName = "Success"),
		Failed UMETA(DisplayName = "Failed"),
		Cancelled UMETA(DisplayName = "Cancelled"),
		Invalid UMETA(DisplayName = "Invalid"),
		NotAllowed UMETA(DisplayName = "NotAllowed"),
		Unknown  UMETA(DisplayName = "Unknown")
	};
}

/**
 * Delegate fired when a session create request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQueryForAvailablePurchasesComplete, bool);
typedef FOnQueryForAvailablePurchasesComplete::FDelegate FOnQueryForAvailablePurchasesCompleteDelegate;

/**
 * Delegate fired when the online session has transitioned to the started state
 *
 * @param SessionName the name of the session the that has transitioned to started
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInAppPurchaseComplete, EInAppPurchaseState::Type);
typedef FOnInAppPurchaseComplete::FDelegate FOnInAppPurchaseCompleteDelegate;


/**
* Micro-transaction purchase information
*/
USTRUCT(BlueprintType)
struct FInAppPurchaseProductInfo
{
	GENERATED_USTRUCT_BODY()

	// The unique product identifier
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString Identifier;

	// The localized display name
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString DisplayName;

	// The localized display description name
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString DisplayDescription;

	// The localized display price name
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString DisplayPrice;
};


/**
*	Interface for reading data from an In App Purchase service
*/
class ONLINESUBSYSTEM_API FOnlineProductInformationRead
{
public:
	/** Indicates an error reading data occurred while processing */
	EOnlineAsyncTaskState::Type ReadState;

	FOnlineProductInformationRead() :
		ReadState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	TArray<FInAppPurchaseProductInfo> ProvidedProductInformation;
};

typedef TSharedRef<FOnlineProductInformationRead, ESPMode::ThreadSafe> FOnlineProductInformationReadRef;
typedef TSharedPtr<FOnlineProductInformationRead, ESPMode::ThreadSafe> FOnlineProductInformationReadPtr;


/**
 *	Interface for reading data from an In App Purchase service
 */
class ONLINESUBSYSTEM_API FOnlineInAppPurchaseTransaction
{
public:
	/** Indicates an error reading data occurred while processing */
	EOnlineAsyncTaskState::Type ReadState;

	FOnlineInAppPurchaseTransaction() :
		ReadState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	FInAppPurchaseProductInfo ProvidedProductInformation;
};

typedef TSharedRef<FOnlineInAppPurchaseTransaction, ESPMode::ThreadSafe> FOnlineInAppPurchaseTransactionRef;
typedef TSharedPtr<FOnlineInAppPurchaseTransaction, ESPMode::ThreadSafe> FOnlineInAppPurchaseTransactionPtr;

/**
 *	IOnlineStore - Interface class for microtransactions
 */
class IOnlineStore
{
public:

	/**
	 * Search for what purchases are available
	 *
	 * @return - Whether a request was sent to check for purchases
	 */
	virtual bool QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject) = 0;

	/**
	 * Delegate which is executed when QueryForAvailablePurchases completes
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnQueryForAvailablePurchasesComplete, bool);

	/**
	 * Check whether microtransactions can be purchased
	 *
	 * @return - Whether the device can make purchases
	 */
	virtual bool IsAllowedToMakePurchases() = 0;

	/**
	 * Begin a purchase transaction for the product which relates to the given index
	 *
	 * @param Index The id of the object we would like to purchase.
	 *
	 * @return - whether a purchase request was sent
	 */
	virtual bool BeginPurchase(const FString& ProductId, FOnlineInAppPurchaseTransactionRef& InReadObject) = 0;
	
	/**
	 * Delegate which is executed when a Purchase completes
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnInAppPurchaseComplete, EInAppPurchaseState::Type);
};