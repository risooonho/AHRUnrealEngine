// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Engine.cpp: Unreal engine package.
=============================================================================*/

#include "EnginePrivate.h"
#include "Materials/MaterialInstanceConstant.h"
#include "EngineModule.h"
#include "EditorSupportDelegates.h"
#include "ComponentReregisterContext.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 EngineLinkerHelper;


#if WITH_EDITOR
/*-----------------------------------------------------------------------------
 FEditorSupportDelegates
 Delegates that are needed for proper editor functionality, but are accessed 
 or triggered in engine code.
 -----------------------------------------------------------------------------*/
/** Called when all viewports need to be redrawn */
FSimpleMulticastDelegate FEditorSupportDelegates::RedrawAllViewports;
/** Called when the editor is cleansing of transient references before a map change event */
FSimpleMulticastDelegate FEditorSupportDelegates::CleanseEditor;
/** Called when the world is modified */
FSimpleMulticastDelegate FEditorSupportDelegates::WorldChange;
/** Sent to force a property window rebuild */
FEditorSupportDelegates::FOnForcePropertyWindowRebuild FEditorSupportDelegates::ForcePropertyWindowRebuild;
/** Sent when events happen that affect how the editors UI looks (mode changes, grid size changes, etc) */
FSimpleMulticastDelegate FEditorSupportDelegates::UpdateUI;
/** Called for a material after the user has change a texture's compression settings.
	Needed to notify the material editors that the need to reattach their preview objects */
FEditorSupportDelegates::FOnMaterialTextureSettingsChanged FEditorSupportDelegates::MaterialTextureSettingsChanged;
/** Refresh property windows w/o creating/destroying controls */
FSimpleMulticastDelegate FEditorSupportDelegates::RefreshPropertyWindows;
/** Sent before the given windows message is handled in the given viewport */
FEditorSupportDelegates::FOnWindowsMessage FEditorSupportDelegates::PreWindowsMessage;
/** Sent after the given windows message is handled in the given viewport */
FEditorSupportDelegates::FOnWindowsMessage FEditorSupportDelegates::PostWindowsMessage;
/** Sent after the usages flags on a material have changed*/
FEditorSupportDelegates::FOnMaterialUsageFlagsChanged FEditorSupportDelegates::MaterialUsageFlagsChanged;
FEditorSupportDelegates::FOnVectorParameterDefaultChanged FEditorSupportDelegates::VectorParameterDefaultChanged;
FEditorSupportDelegates::FOnScalarParameterDefaultChanged FEditorSupportDelegates::ScalarParameterDefaultChanged;

#endif // WITH_EDITOR

IRendererModule* CachedRendererModule = NULL;

IRendererModule& GetRendererModule()
{
	if (!CachedRendererModule)
	{
		CachedRendererModule = &FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
	}

	return *CachedRendererModule;
}

void ResetCachedRendererModule()
{
	CachedRendererModule = NULL;
}



#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void ReattachMaterialInstances(const TArray<FString>& Args)
{
	FMaterialUpdateContext MaterialUpdateContext;

	UE_LOG(LogConsoleResponse, Display, TEXT("Reattach.MaterialInstances:"));

	// Clear the parents out of combination material instances
	for( TObjectIterator<UMaterialInstanceConstant> MaterialIt; MaterialIt; ++MaterialIt )
	{
		if(Args.Num() == 1)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("   %s"), *MaterialIt->GetName());

			if(MaterialIt->GetName() == Args[0])
			{
				MaterialUpdateContext.AddMaterialInstance(*MaterialIt);
			}
		}
	}
	UE_LOG(LogConsoleResponse, Display, TEXT(""));
}

FAutoConsoleCommand ReattachMaterialInstancesCmd(
	TEXT("Reattach.MaterialInstances"),
	TEXT("Useful for debugging, reattaches all materials. Optional parameter can be a materialinstance name (e.g. DecoStatue_Subsurface0)."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&ReattachMaterialInstances)
	);

void ReattachMaterials(const TArray<FString>& Args)
{
	FMaterialUpdateContext MaterialUpdateContext;

	UE_LOG(LogConsoleResponse, Display, TEXT("Reattach.Materials:"));

	// Clear the parents out of combination material
	for( TObjectIterator<UMaterial> MaterialIt; MaterialIt; ++MaterialIt )
	{
		if(Args.Num() == 1)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("   %s"), *MaterialIt->GetName());

			if(MaterialIt->GetName() == Args[0])
			{
				MaterialUpdateContext.AddMaterial(*MaterialIt);
			}
		}
	}
	UE_LOG(LogConsoleResponse, Display, TEXT(""));
}

FAutoConsoleCommand ReattachMaterialsCmd(
	TEXT("Reattach.Materials"),
	TEXT("Useful for debugging, reattaches all materials. Optional parameter can be a material name (e.g. DecoStatue_Subsurface0_Inst)."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&ReattachMaterials)
	);


void ReattachComponents(const TArray<FString>& Args)
{
	if(Args.Num() != 1)
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Reattach.Components: missing class name parameter"));
		return;
	}

	UE_LOG(LogConsoleResponse, Display, TEXT("Reattach.Components:"));

	UClass* Class=NULL;
	if( ParseObject<UClass>( *Args[0], TEXT("CLASS="), Class, ANY_PACKAGE ) &&
		Class->IsChildOf(UActorComponent::StaticClass()) )
	{
		for( FObjectIterator It(Class); It; ++It )
		{
			UActorComponent* ActorComponent = Cast<UActorComponent>(*It);
			if( ActorComponent )
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("   Component: %s"), 
					*ActorComponent->GetName());

				FComponentReregisterContext Reregister(ActorComponent);
			}
		}
		UE_LOG(LogConsoleResponse, Display, TEXT(""));
	}
	else
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Reattach.Components: No objects with the class name '%s' found"), *Args[0]);
	}

}

FAutoConsoleCommand ReattachComponentsCmd(
	TEXT("Reattach.Components"),
	TEXT("Useful for debugging, reattaches all components. Parameter needs to be the class name."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&ReattachComponents)
	);


#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
