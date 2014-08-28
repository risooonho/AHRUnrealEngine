// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "BlueprintBoundEventNodeSpawner.h"
#include "ObjectEditorUtils.h"		// for GetCategory()
#include "KismetEditorUtilities.h"	// for FindBoundEventForComponent()/FindBoundEventForActor()
#include "EditorCategoryUtils.h"	// for GetCommonCategory()

#define LOCTEXT_NAMESPACE "BlueprintBoundEventNodeSpawner"

/*******************************************************************************
 * UBlueprintBoundEventNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintBoundEventNodeSpawner* UBlueprintBoundEventNodeSpawner::Create(TSubclassOf<UK2Node_Event> NodeClass, UMulticastDelegateProperty* EventDelegate, UObject* Outer/* = nullptr*/)
{
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	UBlueprintBoundEventNodeSpawner* NodeSpawner = NewObject<UBlueprintBoundEventNodeSpawner>(Outer);
	NodeSpawner->NodeClass     = NodeClass;
	NodeSpawner->EventDelegate = EventDelegate;

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintBoundEventNodeSpawner::UBlueprintBoundEventNodeSpawner(class FPostConstructInitializeProperties const& PCIP)
	: Super(PCIP)
	, EventDelegate(nullptr)
{
}

//------------------------------------------------------------------------------
UEdGraphNode* UBlueprintBoundEventNodeSpawner::Invoke(UEdGraph* ParentGraph, FVector2D const Location) const
{
	UK2Node_Event* EventNode = nullptr;
	if (IsBindingSet())
	{
		EventNode = CastChecked<UK2Node_Event>(Super::Invoke(ParentGraph, Location));
		Bind(EventNode);
	}
	return EventNode;
}

//------------------------------------------------------------------------------
FText UBlueprintBoundEventNodeSpawner::GetDefaultMenuName() const
{
	FText const DelegateName = FText::FromName(EventDelegate->GetFName());
	return FText::Format(LOCTEXT("ComponentEventName", "Add {0}"), DelegateName);
}

//------------------------------------------------------------------------------
FText UBlueprintBoundEventNodeSpawner::GetDefaultMenuCategory() const
{
	FText DelegateCategory = FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Delegates);
	if (UMulticastDelegateProperty const* Delegate = GetEventDelegate())
	{
		DelegateCategory = FText::FromString(FObjectEditorUtils::GetCategory(Delegate));
	}
	return DelegateCategory;
}

//------------------------------------------------------------------------------
UK2Node_Event const* UBlueprintBoundEventNodeSpawner::FindPreExistingEvent(UBlueprint* Blueprint) const
{
	UK2Node_Event const* PreExistingEvent = nullptr;

	UObject const* BoundObject = nullptr;
	if (IsBindingSet())
	{
		BoundObject = BoundObjects.CreateConstIterator()->Get();
	}

	if (BoundObject != nullptr)
	{
		if (NodeClass->IsChildOf<UK2Node_ComponentBoundEvent>())
		{
			PreExistingEvent = FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, EventDelegate->GetFName(), BoundObject->GetFName());
		}
		else if (NodeClass->IsChildOf<UK2Node_ActorBoundEvent>())
		{
			PreExistingEvent = FKismetEditorUtilities::FindBoundEventForActor(CastChecked<AActor>(BoundObject), EventDelegate->GetFName());
		}
	}
	return PreExistingEvent;
}

//------------------------------------------------------------------------------
bool UBlueprintBoundEventNodeSpawner::CanBind(UObject const* BindingCandidate) const
{
	bool bCanBind = false;

	UClass* DelegateOwner = nullptr;
	if (UMulticastDelegateProperty const* Delegate = GetEventDelegate())
	{
		DelegateOwner = Delegate->GetOwnerClass();
	}

	if (NodeClass->IsChildOf<UK2Node_ComponentBoundEvent>())
	{
		UObjectProperty const* BindingProperty = Cast<UObjectProperty>(BindingCandidate);

		bCanBind = (BindingProperty != nullptr) && (DelegateOwner != nullptr) &&
			BindingProperty->PropertyClass->IsChildOf(DelegateOwner);
	}
	else if (NodeClass->IsChildOf<UK2Node_ActorBoundEvent>())
	{
		bCanBind = BindingCandidate->IsA<AActor>() && (DelegateOwner != nullptr) &&
			BindingCandidate->GetClass()->IsChildOf(DelegateOwner);
	}
	return bCanBind;
}

//------------------------------------------------------------------------------
bool UBlueprintBoundEventNodeSpawner::BindToNode(UEdGraphNode* Node, UObject* Binding) const
{
	bool bWasBound = false;
	if (UK2Node_ComponentBoundEvent* EventNode = Cast<UK2Node_ComponentBoundEvent>(Node))
	{
		if (UObjectProperty const* BoundProperty = Cast<UObjectProperty>(Binding))
		{
			EventNode->InitializeComponentBoundEventParams(BoundProperty, EventDelegate.Get());
			bWasBound = true;
			Node->ReconstructNode();
		}
	}
	else if (UK2Node_ActorBoundEvent* EventNode = CastChecked<UK2Node_ActorBoundEvent>(Node))
	{
		if (AActor* BoundActor = Cast<AActor>(Binding))
		{
			EventNode->InitializeActorBoundEventParams(BoundActor, EventDelegate.Get());
			bWasBound = true;
			Node->ReconstructNode();
		}
	}
	return bWasBound;
}

//------------------------------------------------------------------------------
UMulticastDelegateProperty const* UBlueprintBoundEventNodeSpawner::GetEventDelegate() const
{
	return EventDelegate.Get();
}

#undef LOCTEXT_NAMESPACE