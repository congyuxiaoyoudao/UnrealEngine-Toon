// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionPackageHelper.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Engine/Level.h"
#include "Engine/LevelStreamingGCHelper.h"
#include "Engine/World.h"

void FWorldPartitionPackageHelper::UnloadPackage(UPackage* InPackage)
{
	auto TrashPackage = [](UPackage* InPackage)
	{
		// Clears RF_Standalone flag on objects in package (UMetaData)
		ForEachObjectWithPackage(InPackage, [](UObject* Object)
		{
			Object->ClearFlags(RF_Standalone);
			return true;
		}, false);

		// Rename so it isn't found again
		FLevelStreamingGCHelper::TrashPackage(InPackage);
	};

	TrashPackage(InPackage);

	// World specific
	if (UWorld* PackageWorld = UWorld::FindWorldInPackage(InPackage))
	{
		if (PackageWorld->PersistentLevel)
		{
			// Manual cleanup of level since world was not initialized
			PackageWorld->PersistentLevel->CleanupLevel(/*bCleanupResources*/ true, /*bUnloadFromEditor*/true);
		}

		if (PackageWorld->PersistentLevel->IsUsingExternalObjects())
		{
			ForEachObjectWithOuter(PackageWorld->PersistentLevel, [&TrashPackage](UObject* InObject)
			{
				if (UPackage* ExternalPackage = InObject->GetExternalPackage())
				{
					TrashPackage(ExternalPackage);
				}
			}, /*bIncludeNestedObjects*/ true);
		}
	}
}

#endif