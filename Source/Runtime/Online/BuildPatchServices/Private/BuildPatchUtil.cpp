// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildPatchUtil.h"

#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceRedirector.h"

#include "Data/ChunkData.h"
#include "Data/ManifestData.h"
#include "Common/FileSystem.h"
#include "BuildPatchHash.h"
#include "BuildPatchServicesModule.h"

using namespace BuildPatchServices;

/* FBuildPatchUtils implementation
*****************************************************************************/
FString FBuildPatchUtils::GetChunkNewFilename(const EFeatureLevel FeatureLevel, const FGuid& ChunkGUID, const uint64& ChunkHash)
{
	check(ChunkGUID.IsValid());
	if (FeatureLevel < EFeatureLevel::DataFileRenames)
	{
		return GetChunkOldFilename(ChunkGUID);
	}
	return FString::Printf(TEXT("%s/%02d/%016llX_%s.chunk"), ManifestVersionHelpers::GetChunkSubdir(FeatureLevel), FCrc::MemCrc32(&ChunkGUID, sizeof(FGuid)) % 100, ChunkHash, *ChunkGUID.ToString());
}

FString FBuildPatchUtils::GetFileNewFilename(const EFeatureLevel FeatureLevel, const FGuid& FileGUID, const FSHAHash& FileHash)
{
	check(FileGUID.IsValid());
	return FString::Printf(TEXT("%s/%02d/%s_%s.file"), ManifestVersionHelpers::GetFileSubdir(FeatureLevel), FCrc::MemCrc32(&FileGUID, sizeof(FGuid)) % 100, *FileHash.ToString(), *FileGUID.ToString());
}

FString FBuildPatchUtils::GetFileNewFilename(const EFeatureLevel FeatureLevel, const FGuid& FileGUID, const uint64& FileHash)
{
	check(FileGUID.IsValid());
	return FString::Printf(TEXT("%s/%02d/%016llX_%s.file"), ManifestVersionHelpers::GetFileSubdir(FeatureLevel), FCrc::MemCrc32(&FileGUID, sizeof(FGuid)) % 100, FileHash, *FileGUID.ToString());
}

void FBuildPatchUtils::GetChunkDetailFromNewFilename( const FString& ChunkNewFilename, FGuid& ChunkGUID, uint64& ChunkHash )
{
	const FString ChunkFilename = FPaths::GetBaseFilename( ChunkNewFilename );
	FString GuidString;
	FString HashString;
	ChunkFilename.Split( TEXT( "_" ), &HashString, &GuidString );
	// Check that the strings are exactly as we expect otherwise this is not being used properly
	check( GuidString.Len() == 32 );
	check( HashString.Len() == 16 );
	ChunkHash = FCString::Strtoui64( *HashString, NULL, 16 );
	FGuid::Parse( GuidString, ChunkGUID );
}

void FBuildPatchUtils::GetFileDetailFromNewFilename(const FString& FileNewFilename, FGuid& FileGUID, FSHAHash& FileHash)
{
	const FString FileFilename = FPaths::GetBaseFilename( FileNewFilename );
	FString GuidString;
	FString HashString;
	FileFilename.Split( TEXT( "_" ), &HashString, &GuidString );
	// Check that the strings are exactly as we expect otherwise this is not being used properly
	check( GuidString.Len() == 32 );
	check( HashString.Len() == 40 );
	HexToBytes( HashString, FileHash.Hash );
	FGuid::Parse( GuidString, FileGUID );
}

FString FBuildPatchUtils::GetChunkOldFilename(const FGuid& ChunkGUID)
{
	check(ChunkGUID.IsValid());
	return FString::Printf(TEXT("Chunks/%02d/%s.chunk"), FCrc::MemCrc_DEPRECATED(&ChunkGUID, sizeof(FGuid)) % 100, *ChunkGUID.ToString());
}

FString FBuildPatchUtils::GetFileOldFilename(const FGuid& FileGUID)
{
	check(FileGUID.IsValid());
	return FString::Printf(TEXT("Files/%02d/%s.file"), FCrc::MemCrc_DEPRECATED(&FileGUID, sizeof(FGuid)) % 100, *FileGUID.ToString());
}

FString FBuildPatchUtils::GetDataTypeOldFilename(EBuildPatchDataType DataType, const FGuid& Guid)
{
	check(Guid.IsValid());

	switch (DataType)
	{
	case EBuildPatchDataType::ChunkData:
		return GetChunkOldFilename(Guid);
	case EBuildPatchDataType::FileData:
		return GetFileOldFilename(Guid);
	}

	// Error, didn't case type
	check(false);
	return TEXT("");
}

FString FBuildPatchUtils::GetDataFilename(const FBuildPatchAppManifestRef& Manifest, const FGuid& DataGUID)
{
	return GetDataFilename(Manifest.Get(), DataGUID);
}

FString FBuildPatchUtils::GetDataFilename(const FBuildPatchAppManifest&    Manifest, const FGuid& DataGUID)
{
	const EBuildPatchDataType DataType = Manifest.IsFileDataManifest() ? EBuildPatchDataType::FileData : EBuildPatchDataType::ChunkData;
	if (Manifest.GetFeatureLevel() < EFeatureLevel::DataFileRenames)
	{
		return FBuildPatchUtils::GetDataTypeOldFilename(DataType, DataGUID);
	}
	else if (!Manifest.IsFileDataManifest())
	{
		uint64 ChunkHash;
		const bool bFound = Manifest.GetChunkHash(DataGUID, ChunkHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetChunkNewFilename(Manifest.GetFeatureLevel(), DataGUID, ChunkHash);
	}
	else if (Manifest.GetFeatureLevel() <= EFeatureLevel::StoredAsCompressedUClass)
	{
		FSHAHash FileHash;
		const bool bFound = Manifest.GetFileHash(DataGUID, FileHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetFileNewFilename(Manifest.GetFeatureLevel(), DataGUID, FileHash);
	}
	else
	{
		uint64 FileHash;
		const bool bFound = Manifest.GetFilePartHash(DataGUID, FileHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetFileNewFilename(Manifest.GetFeatureLevel(), DataGUID, FileHash);
	}
}

bool FBuildPatchUtils::GetGUIDFromFilename( const FString& DataFilename, FGuid& DataGUID )
{
	const FString DataBaseFilename = FPaths::GetBaseFilename( DataFilename );
	FString GuidString;
	if( DataBaseFilename.Contains( TEXT( "_" ) ) )
	{
		DataBaseFilename.Split( TEXT( "_" ), NULL, &GuidString );
	}
	else
	{
		GuidString = DataBaseFilename;
	}
	if(GuidString.Len() == 32)
	{
		return FGuid::Parse(GuidString, DataGUID);
	}
	return false;
}

FString FBuildPatchUtils::GenerateNewBuildId()
{
	FGuid NewGuid = FGuid::NewGuid();
	// Minimise string length using base 64 string encode.
	FString BuildId = FBase64::Encode((const uint8*)&NewGuid, sizeof(FGuid));
	// Make URI safe.
	BuildId.ReplaceInline(TEXT("+"), TEXT("-"), ESearchCase::CaseSensitive);
	BuildId.ReplaceInline(TEXT("/"), TEXT("_"), ESearchCase::CaseSensitive);
	// Trim = characters.
	BuildId.ReplaceInline(TEXT("="), TEXT(""), ESearchCase::CaseSensitive);
	return BuildId;
}

FString FBuildPatchUtils::GetBackwardsCompatibleBuildId(const FManifestMeta& ManifestMeta)
{
	// Use an SHA to generate a fixed length unique identifier referring to some of the meta values.
	FSHA1 Sha;
	FSHAHash Hash;
	Sha.Update((const uint8*)&ManifestMeta.AppID, sizeof(ManifestMeta.AppID));
	// For platform agnostic result, we must use UTF8. TCHAR can be 16b, or 32b etc.
	FTCHARToUTF8 UTF8AppName(*ManifestMeta.AppName);
	FTCHARToUTF8 UTF8BuildVersion(*ManifestMeta.BuildVersion);
	FTCHARToUTF8 UTF8LaunchExe(*ManifestMeta.LaunchExe);
	FTCHARToUTF8 UTF8LaunchCommand(*ManifestMeta.LaunchCommand);
	Sha.Update((const uint8*)UTF8AppName.Get(), sizeof(ANSICHAR) * UTF8AppName.Length());
	Sha.Update((const uint8*)UTF8BuildVersion.Get(), sizeof(ANSICHAR) * UTF8BuildVersion.Length());
	Sha.Update((const uint8*)UTF8LaunchExe.Get(), sizeof(ANSICHAR) * UTF8LaunchExe.Length());
	Sha.Update((const uint8*)UTF8LaunchCommand.Get(), sizeof(ANSICHAR) * UTF8LaunchCommand.Length());
	Sha.Final();
	Sha.GetHash(Hash.Hash);

	// Minimise string length using base 64 string encode.
	FString BuildId = FBase64::Encode(Hash.Hash, FSHA1::DigestSize);
	// Make URI safe.
	BuildId.ReplaceInline(TEXT("+"), TEXT("-"), ESearchCase::CaseSensitive);
	BuildId.ReplaceInline(TEXT("/"), TEXT("_"), ESearchCase::CaseSensitive);
	// Trim = characters.
	BuildId.ReplaceInline(TEXT("="), TEXT(""), ESearchCase::CaseSensitive);
	return BuildId;
}

FString FBuildPatchUtils::GetChunkDeltaDirectory(const FBuildPatchAppManifest& DestinationManifest)
{
	return TEXT("Deltas") / DestinationManifest.GetBuildId();
}

FString FBuildPatchUtils::GetChunkDeltaFilename(const FBuildPatchAppManifest& SourceManifest, const FBuildPatchAppManifest& DestinationManifest)
{
	return GetChunkDeltaDirectory(DestinationManifest) / SourceManifest.GetBuildId() + TEXT(".delta");
}

uint8 FBuildPatchUtils::VerifyFile(IFileSystem* FileSystem, const FString& FileToVerify, const FSHAHash& Hash1, const FSHAHash& Hash2)
{
	FBuildPatchFloatDelegate NoProgressDelegate;
	FBuildPatchBoolRetDelegate NoPauseDelegate;
	FBuildPatchBoolRetDelegate NoAbortDelegate;
	return VerifyFile(FileSystem, FileToVerify, Hash1, Hash2, NoProgressDelegate, NoPauseDelegate, NoAbortDelegate);
}

uint8 FBuildPatchUtils::VerifyFile(IFileSystem* FileSystem, const FString& FileToVerify, const FSHAHash& Hash1, const FSHAHash& Hash2, FBuildPatchFloatDelegate ProgressDelegate, FBuildPatchBoolRetDelegate ShouldPauseDelegate, FBuildPatchBoolRetDelegate ShouldAbortDelegate)
{
	uint8 ReturnValue = 0;
	TUniquePtr<FArchive> FileReader = FileSystem->CreateFileReader(*FileToVerify);
	ProgressDelegate.ExecuteIfBound(0.0f);
	if (FileReader.IsValid())
	{
		FSHA1 HashState;
		FSHAHash HashValue;
		const int64 FileSize = FileReader->TotalSize();
		uint8* FileReadBuffer = new uint8[FileBufferSize];
		while (!FileReader->AtEnd() && (!ShouldAbortDelegate.IsBound() || !ShouldAbortDelegate.Execute()))
		{
			// Pause if necessary
			while ((ShouldPauseDelegate.IsBound() && ShouldPauseDelegate.Execute())
			   && (!ShouldAbortDelegate.IsBound() || !ShouldAbortDelegate.Execute()))
			{
				FPlatformProcess::Sleep(0.1f);
			}
			// Read file and update hash state
			const int64 SizeLeft = FileSize - FileReader->Tell();
			const uint32 ReadLen = FMath::Min< int64 >(FileBufferSize, SizeLeft);
			FileReader->Serialize(FileReadBuffer, ReadLen);
			HashState.Update(FileReadBuffer, ReadLen);
			const double FileSizeTemp = FileSize;
			const float Progress = 1.0f - ((SizeLeft - ReadLen) / FileSizeTemp);
			ProgressDelegate.ExecuteIfBound(Progress);
		}
		delete[] FileReadBuffer;
		HashState.Final();
		HashState.GetHash(HashValue.Hash);
		ReturnValue = (HashValue == Hash1) ? 1 : (HashValue == Hash2) ? 2 : 0;
		if (ReturnValue == 0)
		{
			GLog->Logf(TEXT("BuildDataGenerator: Verify failed on %s"), *FileToVerify);
		}
		FileReader->Close();
	}
	else
	{
		GLog->Logf(TEXT("BuildDataGenerator: ERROR VerifyFile cannot open %s"), *FileToVerify);
	}
	ProgressDelegate.ExecuteIfBound(1.0f);
	return ReturnValue;
}


uint64 FBuildPatchUtils::CalculateDiskSpaceRequirementsWithDeleteDuringInstall(
	const TArray<FString>& InFilesToConstruct, 
	int32 InCompletedFileCount, 
	IBuildManifestSet* InManifestSet,
	const TArray<uint64>& InChunkDbSizesAtPosition,
	uint64 InTotalChunkDbSize
	)
{
	uint64 TotalDeletedSize = 0;
	uint64 TotalWrittenSize = 0;

	uint64 MaxDiskSize = 0;
	int32 MaxDiskSizeFileIndex = 0;

	// We start off with full chunk db size.
	uint64 TotalChunkDbSizeAtLastFile = InTotalChunkDbSize;

	for (int32 FileIndex = InCompletedFileCount; FileIndex < InFilesToConstruct.Num(); FileIndex++)
	{
		uint64 ChunkDbSize = InChunkDbSizesAtPosition[FileIndex];

		// We've completed this file
		uint64 NewFileSize = 0;
		{
			const FFileManifest* IncomingFileManifest = InManifestSet->GetNewFileManifest(InFilesToConstruct[FileIndex]);			
			if (IncomingFileManifest)
			{
				NewFileSize = IncomingFileManifest->FileSize;
			}
		}

		TotalWrittenSize += NewFileSize;

		// We delete the chunkdbs _after_ we write the output so we can't use this size until 
		// the next file gets done. Be sure to handle the case where we've deleted so much data
		// we are below the waterline.
		if (TotalDeletedSize < (TotalWrittenSize + TotalChunkDbSizeAtLastFile) &&
			(TotalChunkDbSizeAtLastFile + TotalWrittenSize - TotalDeletedSize) > MaxDiskSize)
		{
			MaxDiskSize = TotalChunkDbSizeAtLastFile + TotalWrittenSize - TotalDeletedSize;
			MaxDiskSizeFileIndex = FileIndex;
		}

		//UE_LOG(LogTemp, Display, TEXT("...@ file %d chunks = %llu, install = %llu"), FileIndex, TotalChunkDbSizeAtLastFile, TotalWrittenSize);

		// If we are patching, we can now delete the output file, which decreases our disk presence, however
		// we update this after the check because we can't delete until after we have the file fully constructed.
		const FFileManifest* OnDiskFileManifest = InManifestSet->GetCurrentFileManifest(InFilesToConstruct[FileIndex]);
		if (OnDiskFileManifest)
		{
			TotalDeletedSize += OnDiskFileManifest->FileSize;
		}

		TotalChunkDbSizeAtLastFile = ChunkDbSize;
	}

	//UE_LOG(LogTemp, Display, TEXT("Max disk use %llu (+%llu after d/l) (install size: %llu: +%llu) after file %d"), MaxDiskSize, PostDlSize, TotalWrittenSize, MaxDiskSize - TotalWrittenSize, MaxDiskSizeFileIndex);

	return MaxDiskSize;
}