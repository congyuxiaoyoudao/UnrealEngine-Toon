// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeUtils.h"

#include "Engine/Level.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeEditTypes.h"
#include "Algo/Transform.h"

#if WITH_EDITOR
#include "EditorDirectories.h"
#include "Engine/Texture2D.h"
#include "LandscapeComponent.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif

// Channel remapping
extern const size_t ChannelOffsets[4];

namespace UE::Landscape
{

bool DoesPlatformSupportEditLayers(EShaderPlatform InShaderPlatform)
{
	// Edit layers work on the GPU and are only available on SM5+ and in the editor : 
	return IsFeatureLevelSupported(InShaderPlatform, ERHIFeatureLevel::SM5)
		&& !IsConsolePlatform(InShaderPlatform)
		&& !IsMobilePlatform(InShaderPlatform);
}

ELandscapeToolTargetTypeFlags GetLandscapeToolTargetTypeAsFlags(ELandscapeToolTargetType InTargetType)
{
	uint8 TargetTypeValue = static_cast<uint8>(InTargetType);
	check(TargetTypeValue < static_cast<uint8>(ELandscapeToolTargetType::Count));
	return static_cast<ELandscapeToolTargetTypeFlags>(1 << TargetTypeValue);
}

ELandscapeToolTargetType GetLandscapeToolTargetTypeSingleFlagAsType(ELandscapeToolTargetTypeFlags InSingleFlag)
{
	check(FMath::CountBits(static_cast<uint64>(InSingleFlag)) == 1);
	uint32 Index = FMath::FloorLog2(static_cast<uint8>(InSingleFlag));
	check(Index < static_cast<uint32>(ELandscapeToolTargetType::Count));
	return static_cast<ELandscapeToolTargetType>(Index);
}

FString GetLandscapeToolTargetTypeFlagsAsString(ELandscapeToolTargetTypeFlags InTargetTypeFlags)
{
	TArray<FString> TargetTypeStrings;
	Algo::Transform(MakeFlagsRange(InTargetTypeFlags), TargetTypeStrings, [](ELandscapeToolTargetTypeFlags InTargetTypeFlag) 
		{ return UEnum::GetValueAsString(GetLandscapeToolTargetTypeSingleFlagAsType(InTargetTypeFlag)); });
	return *FString::Join(TargetTypeStrings, TEXT(","));
}

#if WITH_EDITOR

FString GetSharedAssetsPath(const FString& InPath)
{
	FString Path = InPath + TEXT("_sharedassets/");

	if (Path.StartsWith("/Temp/"))
	{
		Path = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::LEVEL) / Path.RightChop(FString("/Temp/").Len());
	}

	return Path;
}

FString GetSharedAssetsPath(const ULevel* InLevel)
{
	return GetSharedAssetsPath(InLevel->GetOutermost()->GetName());
}

FString GetLayerInfoObjectPackageName(const ULevel* InLevel, const FName& InLayerName, FName& OutLayerObjectName)
{
	FString PackageName;
	FString PackageFilename;
	FString SharedAssetsPath = GetSharedAssetsPath(InLevel);
	int32 Suffix = 1;

	OutLayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo"), *ObjectTools::SanitizeInvalidChars(*InLayerName.ToString(), INVALID_LONGPACKAGE_CHARACTERS)));
	FPackageName::TryConvertFilenameToLongPackageName(SharedAssetsPath / OutLayerObjectName.ToString(), PackageName);

	while (FPackageName::DoesPackageExist(PackageName, &PackageFilename))
	{
		OutLayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo_%d"), *ObjectTools::SanitizeInvalidChars(*InLayerName.ToString(), INVALID_LONGPACKAGE_CHARACTERS), Suffix));
		if (!FPackageName::TryConvertFilenameToLongPackageName(SharedAssetsPath / OutLayerObjectName.ToString(), PackageName))
		{
			break;
		}

		Suffix++;
	}

	return PackageName;
}

bool IsVisibilityLayer(const ULandscapeLayerInfoObject* InLayerInfoObject)
{
	return (ALandscapeProxy::VisibilityLayer != nullptr) && (ALandscapeProxy::VisibilityLayer == InLayerInfoObject);
}

uint32 GetTypeHash(const FTextureCopyRequest& InKey)
{
	uint32 Hash = ::GetTypeHash(InKey.Source);
	uint32 HashSlice = ::GetTypeHash(InKey.DestinationSlice);
	return HashCombine(Hash, ::GetTypeHash(InKey.Destination));
}

bool operator==(const FTextureCopyRequest& InEntryA, const FTextureCopyRequest& InEntryB)
{
	return (InEntryA.Source == InEntryB.Source) && (InEntryA.Destination == InEntryB.Destination) && (InEntryA.DestinationSlice == InEntryB.DestinationSlice);
}

bool FBatchTextureCopy::AddWeightmapCopy(UTexture* InDestination, int8 InDestinationSlice, int8 InDestinationChannel, const ULandscapeComponent* InComponent, ULandscapeLayerInfoObject* InLayerInfo)
{
	FTextureCopyRequest CopyRequest;
	const TArray<UTexture2D*>& ComponentWeightmapTextures = InComponent->GetWeightmapTextures();
	const TArray<FWeightmapLayerAllocationInfo>& ComponentWeightmapLayerAllocations = InComponent->GetWeightmapLayerAllocations();
	int8 SourceChannel = INDEX_NONE;

	CopyRequest.Destination = InDestination;
	CopyRequest.DestinationSlice = InDestinationSlice;

	// Find the proper Source Texture and channel from Layer Allocations
	for (const FWeightmapLayerAllocationInfo& ComponentWeightmapLayerAllocation : ComponentWeightmapLayerAllocations)
	{
		if ((ComponentWeightmapLayerAllocation.LayerInfo == InLayerInfo) &&
			ComponentWeightmapLayerAllocation.IsAllocated() &&
			ComponentWeightmapTextures.IsValidIndex(ComponentWeightmapLayerAllocation.WeightmapTextureIndex))
		{
			CopyRequest.Source = ComponentWeightmapTextures[ComponentWeightmapLayerAllocation.WeightmapTextureIndex];
			SourceChannel = ComponentWeightmapLayerAllocation.WeightmapTextureChannel;
			break;
		}
	}

	// Check if we found a proper allocation for this LayerInfo
	if (SourceChannel != INDEX_NONE)
	{
		check((InDestinationChannel < 4) && (SourceChannel < 4));
		FTextureCopyChannelMapping& ChannelMapping = CopyRequests.FindOrAdd(MoveTemp(CopyRequest));
		ChannelMapping[ChannelOffsets[InDestinationChannel]] = ChannelOffsets[SourceChannel];
		return true;
	}

	return false;
}

struct FSourceDataMipNumber
{
	TOptional<FTextureSource::FMipData> MipData;
	int32 MipNumber = 0;
};

struct FDestinationDataMipNumber
{
	TArray<uint8*> DestinationDataPtr;
	int32 MipNumber = 0;
};

bool FBatchTextureCopy::ProcessTextureCopies()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBatchTextureCopy::ProcessTextureCopyRequest);
	TMap<UTexture2D*, FSourceDataMipNumber> Sources;
	TMap<UTexture*, FDestinationDataMipNumber> Destinations;

	if (CopyRequests.Num() == 0)
	{
		return false;
	}

	// Populate source/destination maps to filter unique occurrences
	for (const TPair<FTextureCopyRequest, FTextureCopyChannelMapping>& CopyRequest : CopyRequests)
	{
		FSourceDataMipNumber& SourceData = Sources.Add(CopyRequest.Key.Source);
		SourceData.MipNumber = CopyRequest.Key.Source->Source.GetNumMips();

		FDestinationDataMipNumber& DestinationData = Destinations.Add(CopyRequest.Key.Destination);
		DestinationData.MipNumber = CopyRequest.Key.Destination->Source.GetNumMips();
	}

	// Decompress (if needed) and get the source textures ready for access
	for (TPair<UTexture2D*, FSourceDataMipNumber>& Source : Sources)
	{
		Source.Value.MipData = Source.Key->Source.GetMipData(nullptr);
	}

	// Lock all destinations mips
	for (TPair<UTexture*, FDestinationDataMipNumber>& Destination : Destinations)
	{
		int32 MipNumber = Destination.Value.MipNumber;
		TArray<uint8*>& DestinationDataPtr = Destination.Value.DestinationDataPtr;

		for (int32 MipLevel = 0; MipLevel < MipNumber; ++MipLevel)
		{
			DestinationDataPtr.Add(Destination.Key->Source.LockMip(MipLevel));
		}
	}

	for (const TPair<FTextureCopyRequest, FTextureCopyChannelMapping>& CopyRequest : CopyRequests)
	{
		const FSourceDataMipNumber* SourceDataMipNumber = Sources.Find(CopyRequest.Key.Source);
		const FDestinationDataMipNumber* DestinationDataMipNumber = Destinations.Find(CopyRequest.Key.Destination);

		check((SourceDataMipNumber != nullptr) && (DestinationDataMipNumber != nullptr));
		check(SourceDataMipNumber->MipNumber == DestinationDataMipNumber->MipNumber);

		const int32 MipNumber = SourceDataMipNumber->MipNumber;

		for (int32 MipLevel = 0; MipLevel < MipNumber; ++MipLevel)
		{
			const int64 MipSizeInBytes = CopyRequest.Key.Source->Source.CalcMipSize(MipLevel);
			
			const int32 MipSize = CopyRequest.Key.Destination->Source.GetSizeX() >> MipLevel;
			check(MipSize == (CopyRequest.Key.Destination->Source.GetSizeY() >> MipLevel));

			int32 MipSizeSquare = FMath::Square(MipSize);
			FSharedBuffer MipSrcData = SourceDataMipNumber->MipData->GetMipData(0, 0, MipLevel);
			const uint8* SourceTextureData = static_cast<const uint8*>(MipSrcData.GetData());
			uint8* DestTextureData = DestinationDataMipNumber->DestinationDataPtr[MipLevel] + CopyRequest.Key.DestinationSlice * MipSizeInBytes;

			check((SourceTextureData != nullptr) && (DestTextureData != nullptr));

			const FTextureCopyChannelMapping& ChannelMapping = CopyRequest.Value;

			// Perform the copy, redirecting channels using mappings
			for (int32 Index = 0; Index < MipSizeSquare; ++Index)
			{
				int32 Base = Index * 4;

				for (int32 Channel = 0; Channel < 4; ++Channel)
				{
					if (ChannelMapping[Channel] == INDEX_NONE)
					{
						continue;
					}

					DestTextureData[Base + Channel] = SourceTextureData[Base + ChannelMapping[Channel]];
				}
			}
		}
	}

	// Note that source textures do not need unlocking, data will be released once the FMipData go out of scope
	
	// Unlock all destination mips
	for (TPair<UTexture*, FDestinationDataMipNumber>& Destination : Destinations)
	{
		int32 MipNumber = Destination.Value.MipNumber;
		
		for (int32 MipLevel = 0; MipLevel < MipNumber; ++MipLevel)
		{
			Destination.Key->Source.UnlockMip(MipLevel);
		}
	}

	return true;
}

FLayerInfoFinder::FLayerInfoFinder()
{
	const UClass* AssetClass = ULandscapeLayerInfoObject::StaticClass();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	FName PackageName = *AssetClass->GetPackage()->GetName();
	FName AssetName = AssetClass->GetFName();
											
	Filter.ClassPaths.Add(FTopLevelAssetPath(PackageName,AssetName));
	AssetRegistryModule.Get().GetAssets(Filter, LayerInfoAssets);
}

ULandscapeLayerInfoObject* FLayerInfoFinder::Find(const FName& LayerName) const
{
	for (const FAssetData& LayerInfoAsset : LayerInfoAssets)
	{
		ULandscapeLayerInfoObject* LayerInfo = CastChecked<ULandscapeLayerInfoObject>(LayerInfoAsset.GetAsset());
		if (LayerInfo && LayerInfo->LayerName == LayerName)
		{
			return LayerInfo;
		}
	}
	return nullptr;
}
#endif //!WITH_EDITOR

} // end namespace UE::Landscape