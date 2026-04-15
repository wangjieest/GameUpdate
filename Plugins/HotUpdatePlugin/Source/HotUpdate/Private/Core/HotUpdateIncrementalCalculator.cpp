// Copyright czm. All Rights Reserved.

#include "Core/HotUpdateIncrementalCalculator.h"
#include "Core/HotUpdateFileUtils.h"
#include "HotUpdate.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"

UHotUpdateIncrementalCalculator::UHotUpdateIncrementalCalculator()
{
}

void UHotUpdateIncrementalCalculator::CalculateIncrementalDownload(
	const FHotUpdateManifest& ServerManifest,
	const FHotUpdateManifest& LocalManifest,
	FHotUpdateVersionCheckResult& OutResult)
{
	// 构建本地 Container 索引（ChunkId -> Container）
	TMap<int32, const FHotUpdateContainerInfo*> LocalContainerIndex;
	for (const FHotUpdateContainerInfo& Container : LocalManifest.Containers)
	{
		LocalContainerIndex.Add(Container.ChunkId, &Container);
	}

	// 遍历服务端 Containers，分析差异
	for (const FHotUpdateContainerInfo& ServerContainer : ServerManifest.Containers)
	{
		const FHotUpdateContainerInfo* const* LocalContainerPtr = LocalContainerIndex.Find(ServerContainer.ChunkId);

		bool bNeedDownload = false;
		FString Reason;

		if (LocalContainerPtr == nullptr)
		{
			// 新增 Container
			bNeedDownload = true;
			Reason = TEXT("new container");
			OutResult.AddedFileCount++; // 使用 AddedFileCount 表示新增的 Container 数量
		}
		else
		{
			const FHotUpdateContainerInfo* LocalContainer = *LocalContainerPtr;

			// 对比 Hash 判断是否需要更新
			if (LocalContainer->UcasHash != ServerContainer.UcasHash)
			{
				bNeedDownload = true;
				Reason = TEXT("ucas hash changed");
				OutResult.ModifiedFileCount++; // 使用 ModifiedFileCount 表示修改的 Container 数量
			}
			else if (LocalContainer->UtocHash != ServerContainer.UtocHash)
			{
				bNeedDownload = true;
				Reason = TEXT("utoc hash changed");
				OutResult.ModifiedFileCount++;
			}
		}

		if (bNeedDownload)
		{
			OutResult.UpdateContainers.Add(ServerContainer);
			OutResult.IncrementalDownloadSize += ServerContainer.UtocSize + ServerContainer.UcasSize;

			UE_LOG(LogHotUpdate, Log, TEXT("Need download container: %s (chunkId: %d, reason: %s, size: %.2f MB)"),
				*ServerContainer.ContainerName, ServerContainer.ChunkId, *Reason,
				(ServerContainer.UtocSize + ServerContainer.UcasSize) / (1024.0 * 1024.0));
		}
		else
		{
			OutResult.SkippedFileCount++; // 使用 SkippedFileCount 表示跳过的 Container 数量
			OutResult.SkippedTotalSize += ServerContainer.UtocSize + ServerContainer.UcasSize;
			UE_LOG(LogHotUpdate, Verbose, TEXT("Skipped container: %s (chunkId: %d, unchanged)"),
				*ServerContainer.ContainerName, ServerContainer.ChunkId);
		}
	}

	// 检测已删除的 Container（存在于本地但不在服务端）
	for (const FHotUpdateContainerInfo& LocalContainer : LocalManifest.Containers)
	{
		bool bFound = false;
		for (const FHotUpdateContainerInfo& ServerContainer : ServerManifest.Containers)
		{
			if (ServerContainer.ChunkId == LocalContainer.ChunkId)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			OutResult.DeletedFileCount++; // 使用 DeletedFileCount 表示删除的 Container 数量
			UE_LOG(LogHotUpdate, Verbose, TEXT("Deleted container: %s (chunkId: %d)"),
				*LocalContainer.ContainerName, LocalContainer.ChunkId);
		}
	}

	UE_LOG(LogHotUpdate, Log, TEXT("Incremental analysis complete: %d added, %d modified, %d deleted, %d skipped"),
		OutResult.AddedFileCount, OutResult.ModifiedFileCount, OutResult.DeletedFileCount, OutResult.SkippedFileCount);

	UE_LOG(LogHotUpdate, Log, TEXT("Required containers: %d, total download size: %.2f MB"),
		OutResult.UpdateContainers.Num(), OutResult.IncrementalDownloadSize / (1024.0 * 1024.0));
}

bool UHotUpdateIncrementalCalculator::IsLocalContainerValid(
	const FString& StoragePath,
	const FHotUpdateContainerInfo& Container) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 检查 utoc 文件是否存在
	FString UtocPath = StoragePath / Container.UtocPath;
	if (!PlatformFile.FileExists(*UtocPath))
	{
		return false;
	}

	// 检查 ucas 文件是否存在
	FString UcasPath = StoragePath / Container.UcasPath;
	if (!PlatformFile.FileExists(*UcasPath))
	{
		return false;
	}

	return true;
}