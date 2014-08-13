// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AutomationEditorCommon.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutomationEditorCommon, Log, All);

namespace AutomationEditorCommonUtils
{
	/**
	* Converts a package path to an asset path
	*
	* @param PackagePath - The package path to convert
	*/
	static FString ConvertPackagePathToAssetPath(const FString& PackagePath)
	{
		const FString Filename = FPaths::ConvertRelativePathToFull(PackagePath);
		FString EngineFileName = Filename;
		FString GameFileName = Filename;
		if (FPaths::MakePathRelativeTo(EngineFileName, *FPaths::EngineContentDir()) && !FPaths::IsRelative(EngineFileName))
		{
			const FString ShortName = FPaths::GetBaseFilename(EngineFileName);
			const FString PathName = FPaths::GetPath(EngineFileName);
			const FString AssetName = FString::Printf(TEXT("/Engine/%s/%s.%s"), *PathName, *ShortName, *ShortName);
			return AssetName;
		}
		else if (FPaths::MakePathRelativeTo(GameFileName, *FPaths::GameContentDir()) && !FPaths::IsRelative(GameFileName))
		{
			const FString ShortName = FPaths::GetBaseFilename(GameFileName);
			const FString PathName = FPaths::GetPath(GameFileName);
			const FString AssetName = FString::Printf(TEXT("/Game/%s/%s.%s"), *PathName, *ShortName, *ShortName);
			return AssetName;
		}
		else
		{
			UE_LOG(LogAutomationEditorCommon, Error, TEXT("PackagePath (%s) is invalid for the current project"), *PackagePath);
			return TEXT("");
		}
	}

	/**
	* Imports an object using a given factory
	*
	* @param ImportFactory - The factory to use to import the object
	* @param ObjectName - The name of the object to create
	* @param PackagePath - The full path of the package file to create
	* @param ImportPath - The path to the object to import
	*/
	static UObject* ImportAssetUsingFactory(UFactory* ImportFactory, const FString& ObjectName, const FString& PackagePath, const FString& ImportPath)
	{
		UObject* ImportedAsset = NULL;

		UPackage* Pkg = CreatePackage(NULL, *PackagePath);
		if (Pkg)
		{
			// Make sure the destination package is loaded
			Pkg->FullyLoad();

			UClass* ImportAssetType = ImportFactory->ResolveSupportedClass();
			bool bDummy = false;

			//If we are a texture factory suppress some warning dialog that we don't want
			if (ImportFactory->IsA(UTextureFactory::StaticClass()))
			{
				UTextureFactory::SuppressImportResolutionWarningDialog();
				UTextureFactory::SuppressImportOverwriteDialog();
			}

			ImportedAsset = UFactory::StaticImportObject(ImportAssetType, Pkg, FName(*ObjectName), RF_Public | RF_Standalone, bDummy, *ImportPath, NULL, ImportFactory, NULL, GWarn, 0);

			if (ImportedAsset)
			{
				UE_LOG(LogAutomationEditorCommon, Display, TEXT("Imported %s"), *ImportPath);
			}
			else
			{
				UE_LOG(LogAutomationEditorCommon, Error, TEXT("Failed to import asset using factory %s!"), *ImportFactory->GetName());
			}
		}
		else
		{
			UE_LOG(LogAutomationEditorCommon, Error, TEXT("Failed to create a package!"));
		}

		return ImportedAsset;
	}

	/**
	* Nulls out references to a given object
	*
	* @param InObject - Object to null references to
	*/
	static void NullReferencesToObject(UObject* InObject)
	{
		TArray<UObject*> ReplaceableObjects;
		TMap<UObject*, UObject*> ReplacementMap;
		ReplacementMap.Add(InObject, NULL);
		ReplacementMap.GenerateKeyArray(ReplaceableObjects);

		// Find all the properties (and their corresponding objects) that refer to any of the objects to be replaced
		TMap< UObject*, TArray<UProperty*> > ReferencingPropertiesMap;
		for (FObjectIterator ObjIter; ObjIter; ++ObjIter)
		{
			UObject* CurObject = *ObjIter;

			// Find the referencers of the objects to be replaced
			FFindReferencersArchive FindRefsArchive(CurObject, ReplaceableObjects);

			// Inform the object referencing any of the objects to be replaced about the properties that are being forcefully
			// changed, and store both the object doing the referencing as well as the properties that were changed in a map (so that
			// we can correctly call PostEditChange later)
			TMap<UObject*, int32> CurNumReferencesMap;
			TMultiMap<UObject*, UProperty*> CurReferencingPropertiesMMap;
			if (FindRefsArchive.GetReferenceCounts(CurNumReferencesMap, CurReferencingPropertiesMMap) > 0)
			{
				TArray<UProperty*> CurReferencedProperties;
				CurReferencingPropertiesMMap.GenerateValueArray(CurReferencedProperties);
				ReferencingPropertiesMap.Add(CurObject, CurReferencedProperties);
				for (TArray<UProperty*>::TConstIterator RefPropIter(CurReferencedProperties); RefPropIter; ++RefPropIter)
				{
					CurObject->PreEditChange(*RefPropIter);
				}
			}

		}

		// Iterate over the map of referencing objects/changed properties, forcefully replacing the references and then
		// alerting the referencing objects the change has completed via PostEditChange
		int32 NumObjsReplaced = 0;
		for (TMap< UObject*, TArray<UProperty*> >::TConstIterator MapIter(ReferencingPropertiesMap); MapIter; ++MapIter)
		{
			++NumObjsReplaced;

			UObject* CurReplaceObj = MapIter.Key();
			const TArray<UProperty*>& RefPropArray = MapIter.Value();

			FArchiveReplaceObjectRef<UObject> ReplaceAr(CurReplaceObj, ReplacementMap, false, true, false);

			for (TArray<UProperty*>::TConstIterator RefPropIter(RefPropArray); RefPropIter; ++RefPropIter)
			{
				FPropertyChangedEvent PropertyEvent(*RefPropIter);
				CurReplaceObj->PostEditChangeProperty(PropertyEvent);
			}

			if (!CurReplaceObj->HasAnyFlags(RF_Transient) && CurReplaceObj->GetOutermost() != GetTransientPackage())
			{
				if (!CurReplaceObj->RootPackageHasAnyFlags(PKG_CompiledIn))
				{
					CurReplaceObj->MarkPackageDirty();
				}
			}
		}
	}

	/**
	* gets a factory class based off an asset file extension
	*
	* @param AssetExtension - The file extension to use to find a supporting UFactory
	*/
	static UClass* GetFactoryClassForType(const FString& AssetExtension)
	{
		// First instantiate one factory for each file extension encountered that supports the extension
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			if ((*ClassIt)->IsChildOf(UFactory::StaticClass()) && !((*ClassIt)->HasAnyClassFlags(CLASS_Abstract)))
			{
				UFactory* Factory = Cast<UFactory>((*ClassIt)->GetDefaultObject());
				if (Factory->bEditorImport && Factory->ValidForCurrentGame())
				{
					TArray<FString> FactoryExtensions;
					Factory->GetSupportedFileExtensions(FactoryExtensions);

					// Case insensitive string compare with supported formats of this factory
					if (FactoryExtensions.Contains(AssetExtension))
					{
						return *ClassIt;
					}
				}
			}
		}

		return NULL;
	}

	/**
	* Applies settings to an object by finding UProperties by name and calling ImportText
	*
	* @param InObject - The object to search for matching properties
	* @param PropertyChain - The list UProperty names recursively to search through
	* @param Value - The value to import on the found property
	*/
	static void ApplyCustomFactorySetting(UObject* InObject, TArray<FString>& PropertyChain, const FString& Value)
	{
		const FString PropertyName = PropertyChain[0];
		PropertyChain.RemoveAt(0);

		UProperty* TargetProperty = FindField<UProperty>(InObject->GetClass(), *PropertyName);
		if (TargetProperty)
		{
			if (PropertyChain.Num() == 0)
			{
				TargetProperty->ImportText(*Value, TargetProperty->ContainerPtrToValuePtr<uint8>(InObject), 0, InObject);
			}
			else
			{
				UStructProperty* StructProperty = Cast<UStructProperty>(TargetProperty);
				UObjectProperty* ObjectProperty = Cast<UObjectProperty>(TargetProperty);

				UObject* SubObject = NULL;
				bool bValidPropertyType = true;

				if (StructProperty)
				{
					SubObject = StructProperty->Struct;
				}
				else if (ObjectProperty)
				{
					SubObject = ObjectProperty->GetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<UObject>(InObject));
				}
				else
				{
					//Unknown nested object type
					bValidPropertyType = false;
					UE_LOG(LogAutomationEditorCommon, Error, TEXT("ERROR: Unknown nested object type for property: %s"), *PropertyName);
				}

				if (SubObject)
				{
					ApplyCustomFactorySetting(SubObject, PropertyChain, Value);
				}
				else if (bValidPropertyType)
				{
					UE_LOG(LogAutomationEditorCommon, Error, TEXT("Error accessing null property: %s"), *PropertyName);
				}
			}
		}
		else
		{
			UE_LOG(LogAutomationEditorCommon, Error, TEXT("ERROR: Could not find factory property: %s"), *PropertyName);
		}
	}

	/**
	* Applies the custom factory settings
	*
	* @param InFactory - The factory to apply custom settings to
	* @param FactorySettings - An array of custom settings to apply to the factory
	*/
	static void ApplyCustomFactorySettings(UFactory* InFactory, const TArray<FImportFactorySettingValues>& FactorySettings)
	{
		bool bCallConfigureProperties = true;

		for (int32 i = 0; i < FactorySettings.Num(); ++i)
		{
			if (FactorySettings[i].SettingName.Len() > 0 && FactorySettings[i].Value.Len() > 0)
			{
				//Check if we are setting an FBX import type override.  If we are, we don't want to call ConfigureProperties because that enables bDetectImportTypeOnImport
				if (FactorySettings[i].SettingName.Contains(TEXT("MeshTypeToImport")))
				{
					bCallConfigureProperties = false;
				}

				TArray<FString> PropertyChain;
				FactorySettings[i].SettingName.ParseIntoArray(&PropertyChain, TEXT("."), false);
				ApplyCustomFactorySetting(InFactory, PropertyChain, FactorySettings[i].Value);
			}
		}

		if (bCallConfigureProperties)
		{
			InFactory->ConfigureProperties();
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Common Latent commands

//Latent Undo and Redo command
//If bUndo is true then the undo action will occur otherwise a redo will happen.
bool FUndoRedoCommand::Update()
{
	if (bUndo == true)
	{
		//Undo
		GEditor->UndoTransaction();
	}
	else
	{
		//Redo
		GEditor->RedoTransaction();
	}

	return true;
}