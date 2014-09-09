// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SeparableSSS.h"
#include "RendererInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubsurfaceProfile, Log, All);

// lives on the render thread
ENGINE_API TGlobalResource<FSubsurfaceProfileTexture> GSubsufaceProfileTextureObject;

// Texture with one or more SubSurfaceProfiles or 0 if there is no user
static TRefCountPtr<IPooledRenderTarget> GSSProfiles;


FSubsurfaceProfileTexture::FSubsurfaceProfileTexture()
	: RendererModule(0)
{
	FSubsurfaceProfileStruct DefaultSkin;

	// add element 0, it is used as default profile
	SubsurfaceProfileEntries.Add(FSubsurfaceProfileEntry(DefaultSkin, 0));
}

FSubsurfaceProfileTexture::~FSubsurfaceProfileTexture()
{
	// we assume all ~USubsurfaceProfile() have bene called already
	for (int32 i = 0; i < SubsurfaceProfileEntries.Num(); ++i)
	{
		check(SubsurfaceProfileEntries[i].GameThreadObject == 0);
	}
}

int32 FSubsurfaceProfileTexture::AddProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfilePointer InProfile)
{
	check(InProfile);
	check(FindAllocationId(InProfile) == -1);

	int32 RetAllocationId = -1;
	{
		for (int32 i = 1; i < SubsurfaceProfileEntries.Num(); ++i)
		{
			if (SubsurfaceProfileEntries[i].GameThreadObject == 0)
			{
				RetAllocationId = i; break;
			}
		}

		if(RetAllocationId == -1)
		{
			RetAllocationId = SubsurfaceProfileEntries.Num();
			SubsurfaceProfileEntries.Add(FSubsurfaceProfileEntry(Settings, InProfile));
		}
	}

	UpdateProfile(RetAllocationId, Settings);

	return RetAllocationId;
}


void FSubsurfaceProfileTexture::RemoveProfile(const USubsurfaceProfilePointer InProfile)
{
	int32 AllocationId = FindAllocationId(InProfile);

	if(AllocationId == -1)
	{
		// -1: no allocation, no work needed
		return;
	}

	// >0 as 0 is used as default profile which should never be removed
	check(AllocationId > 0);

	check(SubsurfaceProfileEntries[AllocationId].GameThreadObject == InProfile);

	// make it available for reuse
	SubsurfaceProfileEntries[AllocationId].GameThreadObject = 0;
	SubsurfaceProfileEntries[AllocationId].Settings.Invalidate();
}

void FSubsurfaceProfileTexture::UpdateProfile(int32 AllocationId, const FSubsurfaceProfileStruct Settings)
{
	check(IsInRenderingThread());

	// call SetRendererModule() is missing
	check(RendererModule);

	if (AllocationId == -1)
	{
		// if we modify a profile that is not assigned/used yet, no work is needed
		return;
	}

	check(AllocationId < SubsurfaceProfileEntries.Num());

	SubsurfaceProfileEntries[AllocationId].Settings = Settings;

	GSSProfiles.SafeRelease();
}

const IPooledRenderTarget* FSubsurfaceProfileTexture::GetTexture(FRHICommandListImmediate& RHICmdList)
{
	// call SetRendererModule() is missing
	check(RendererModule);

	if (!GSSProfiles)
	{
		CreateTexture(RHICmdList);
	}

	return GSSProfiles;
}

void FSubsurfaceProfileTexture::ReleaseDynamicRHI()
{
	GSSProfiles.SafeRelease();
}

void FSubsurfaceProfileTexture::CreateTexture(FRHICommandListImmediate& RHICmdList)
{
	// call SetRendererModule() is missing
	check(RendererModule);

	uint32 Height = SubsurfaceProfileEntries.Num();

	check(Height);

	// true:16bit (currently required to have very small and very large kernel sizes), false: 8bit
	const bool b16Bit = true;

	const uint32 Width = 32;

	// at minimum 64 lines (less reallocations)
	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(Width, FMath::Max(Height, (uint32)64)), PF_B8G8R8A8, TexCreate_FastVRAM, TexCreate_None, false));

	if (b16Bit)
	{
		Desc.Format = PF_A16B16G16R16;
	}

	RendererModule->RenderTargetPoolFindFreeElement(Desc, GSSProfiles, TEXT("SSProfiles"));

	// Write the contents of the texture.
	uint32 DestStride;
	uint8* DestBuffer = (uint8*)RHICmdList.LockTexture2D((FTexture2DRHIRef&)GSSProfiles->GetRenderTargetItem().ShaderResourceTexture, 0, RLM_WriteOnly, DestStride, false);

	// we precompute 3 kernels of different size and store in one line
	const uint32 KernelSize0 = 13;
	const uint32 KernelSize1 = 9; 
	const uint32 KernelSize2 = 6;

	const uint32 KernelTotalSize = KernelSize0 + KernelSize1 + KernelSize2;
	check(KernelTotalSize < Width);

	FLinearColor kernel[Width];

	for (uint32 y = 0; y < Height; ++y)
	{
		FSubsurfaceProfileStruct Data = SubsurfaceProfileEntries[y].Settings;

		// bias to avoid div by 0 and a jump to a different value
		// this basically means we don't want subsurface scattering
		// 0.0001f turned out to be too small to fix the issue
		const float Bias = 0.001f;

		Data.SubsurfaceColor = Data.SubsurfaceColor.GetClamped(Bias);
		Data.FalloffColor = Data.FalloffColor.GetClamped(Bias);

		ComputeMirroredSSSKernel(&kernel[0], KernelSize0, Data.SubsurfaceColor, Data.FalloffColor);
		ComputeMirroredSSSKernel(&kernel[KernelSize0], KernelSize1, Data.SubsurfaceColor, Data.FalloffColor);
		ComputeMirroredSSSKernel(&kernel[KernelSize0 + KernelSize1], KernelSize2, Data.SubsurfaceColor, Data.FalloffColor);

		const float TableMaxRGB = 1.0f;
		const float TableMaxA = 3.0f;

		for (int32 Pos = 0; Pos < KernelTotalSize; ++Pos)
		{
			FVector4 C = kernel[Pos] * FLinearColor(1.0f / TableMaxRGB, 1.0f / TableMaxRGB, 1.0f / TableMaxRGB, 1.0f / TableMaxA);

			// requires 16bit (could be made with 8 bit e.g. using sample0.w as 8bit scale applied to all samples (more multiplications in the shader))
			C.W *= Data.ScatterRadius / 1000.0f;

			if (b16Bit)
			{
				uint16* Dest = (uint16*)(DestBuffer + DestStride * y);

				Dest[Pos * 4 + 0] = (uint16)(C.X * (256 * 256 - 0.0001f));
				Dest[Pos * 4 + 1] = (uint16)(C.Y * (256 * 256 - 0.0001f));
				Dest[Pos * 4 + 2] = (uint16)(C.Z * (256 * 256 - 0.0001f));
				Dest[Pos * 4 + 3] = (uint16)(C.W * (256 * 256 - 0.0001f));
			}
			else
			{
				FColor* Dest = (FColor*)(DestBuffer + DestStride * y);

				Dest[Pos] = FColor(FMath::Quantize8UnsignedByte(C.X), FMath::Quantize8UnsignedByte(C.Y), FMath::Quantize8UnsignedByte(C.Z), FMath::Quantize8UnsignedByte(C.W));
			}
		}
	}

	RHICmdList.UnlockTexture2D((FTexture2DRHIRef&)GSSProfiles->GetRenderTargetItem().ShaderResourceTexture, 0, false);
}

TCHAR MiniFontCharFromIndex(uint32 Index)
{
	if (Index <= 9)
	{
		return (TCHAR)('0' + Index);
	}

	Index -= 10;

	if (Index <= 'Z' - 'A')
	{
		return (TCHAR)('A' + Index);
	}

	return (TCHAR)'?';
}

bool FSubsurfaceProfileTexture::GetEntryString(uint32 Index, FString& Out) const
{
	if (Index >= (uint32)SubsurfaceProfileEntries.Num())
	{
		return false;
	}

	const FSubsurfaceProfileStruct& ref = SubsurfaceProfileEntries[Index].Settings;


	Out = FString::Printf(TEXT(" %c. %p ScatterRadius=%.1f, SubsurfaceColor=%.1f %.1f %.1f, FalloffColor=%.1f %.1f %.1f"), 
		MiniFontCharFromIndex(Index), 
		SubsurfaceProfileEntries[Index].GameThreadObject,
		ref.ScatterRadius,
		ref.SubsurfaceColor.R, ref.SubsurfaceColor.G, ref.SubsurfaceColor.B,
		ref.FalloffColor.R, ref.FalloffColor.G, ref.FalloffColor.B);

	return true;
}

int32 FSubsurfaceProfileTexture::FindAllocationId(const USubsurfaceProfilePointer InProfile) const
{
	// we start at 1 because [0] is the default profile and always [0].GameThreadObject = 0 so we don't need to iterate that one
	for (int32 i = 1; i < SubsurfaceProfileEntries.Num(); ++i)
	{
		if (SubsurfaceProfileEntries[i].GameThreadObject == InProfile)
		{
			return i;
		}
	}

	return -1;
}

// for debugging
void FSubsurfaceProfileTexture::Dump()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogSubsurfaceProfile, Log, TEXT("USubsurfaceProfileManager::Dump"));
	for (int32 i = 0; i < SubsurfaceProfileEntries.Num(); ++i)
	{
		// + 1 as the Id is one higher than the array index, 0 is used for the default profile (not assigned)
		UE_LOG(LogSubsurfaceProfile, Log, TEXT("  %d. AllocationId=%d, Pointer=%p"), i, i + 1, SubsurfaceProfileEntries[i].GameThreadObject);

		{
			UE_LOG(LogSubsurfaceProfile, Log, TEXT("     ScatterRadius = %f"),
				SubsurfaceProfileEntries[i].Settings.ScatterRadius);
			UE_LOG(LogSubsurfaceProfile, Log, TEXT("     SubsurfaceColor=%f %f %f"),
				SubsurfaceProfileEntries[i].Settings.SubsurfaceColor.R, SubsurfaceProfileEntries[i].Settings.SubsurfaceColor.G, SubsurfaceProfileEntries[i].Settings.SubsurfaceColor.B);
			UE_LOG(LogSubsurfaceProfile, Log, TEXT("     FalloffColor=%f %f %f"),
				SubsurfaceProfileEntries[i].Settings.FalloffColor.R, SubsurfaceProfileEntries[i].Settings.FalloffColor.G, SubsurfaceProfileEntries[i].Settings.FalloffColor.B);
		}
	}

	UE_LOG(LogSubsurfaceProfile, Log, TEXT(""));
#endif
}



ENGINE_API const IPooledRenderTarget* GetSubsufaceProfileTexture_RT(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	return GSubsufaceProfileTextureObject.GetTexture(RHICmdList);
}

// ------------------------------------------------------

USubsurfaceProfile::USubsurfaceProfile(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void USubsurfaceProfile::BeginDestroy()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		RemoveSubsurfaceProfile,
		USubsurfaceProfilePointer, Ref, (USubsurfaceProfilePointer)this,
		{
			GSubsufaceProfileTextureObject.RemoveProfile(Ref);
		});

	Super::BeginDestroy();
}

void USubsurfaceProfile::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		UpdateSubsurfaceProfile,
		const FSubsurfaceProfileStruct, Settings, this->Settings,
		USubsurfaceProfilePointer, Profile, (USubsurfaceProfilePointer)this,
	{
		// any changes to the setting require an update of the texture
		GSubsufaceProfileTextureObject.UpdateProfile(Settings, Profile);
	});
}