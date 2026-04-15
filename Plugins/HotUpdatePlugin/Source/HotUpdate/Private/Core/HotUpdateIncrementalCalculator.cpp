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
	const FString& StoragePath,
	const FHotUpdateVersionInfo& CurrentVersion,
	const FHotUpdateVersionInfo& LatestVersion,
	FHotUpdateVersionCheckResult& OutResult)
{
	// 构建本地文件索引（路径 -> 文件条目）
	TMap<FString, const FHotUpdateManifestEntry*> LocalFileIndex;
	for (const FHotUpdateManifestEntry& Entry : LocalManifest.Files)
	{
		LocalFileIndex.Add(Entry.FilePath, &Entry);
	}

	// 用于跟踪本地文件是否在服务器 Manifest 中存在
	TSet<FString> ServerFilePaths;

	// 收集需要下载的 Patch Chunk ID
	TSet<int32> RequiredPatchChunkIds;

	// 遍历服务器文件列表，分析文件差异
	for (const FHotUpdateManifestEntry& ServerEntry : ServerManifest.Files)
	{
		ServerFilePaths.Add(ServerEntry.FilePath);

		FHotUpdateFileInfo FileInfo;
		FileInfo.FilePath = ServerEntry.FilePath;
		FileInfo.FileSize = ServerEntry.FileSize;
		FileInfo.FileHash = ServerEntry.FileHash;
		FileInfo.DownloadUrl = ServerEntry.CustomDownloadUrl;

		const FHotUpdateManifestEntry* const* LocalEntryPtr = LocalFileIndex.Find(ServerEntry.FilePath);

		if (LocalEntryPtr == nullptr)
		{
			// 新增文件
			FileInfo.ChangeType = EHotUpdateFileChangeType::Added;
			OutResult.UpdateFiles.Add(FileInfo);
			OutResult.TotalUpdateSize += ServerEntry.FileSize;
			OutResult.AddedFileCount++;

			// 如果是 patch 文件，记录 Chunk ID
			if (ServerEntry.Source == TEXT("patch") && ServerEntry.ChunkId >= 0)
			{
				RequiredPatchChunkIds.Add(ServerEntry.ChunkId);
			}

			UE_LOG(LogHotUpdate, Verbose, TEXT("Added file: %s (source: %s, chunkId: %d)"),
				*ServerEntry.FilePath, *ServerEntry.Source, ServerEntry.ChunkId);
		}
		else
		{
			const FHotUpdateManifestEntry* LocalEntry = *LocalEntryPtr;

			// 检查文件是否变更（通过 Hash 对比）
			if (LocalEntry->FileHash == ServerEntry.FileHash)
			{
				// 文件未变更，检查本地文件是否实际存在
				if (IsLocalFileValid(StoragePath, ServerEntry.FilePath, ServerEntry.FileHash, ServerEntry.FileSize, CurrentVersion, LatestVersion))
				{
					// 跳过下载
					FileInfo.ChangeType = EHotUpdateFileChangeType::Unchanged;
					OutResult.SkippedFileCount++;
					OutResult.SkippedTotalSize += ServerEntry.FileSize;

					UE_LOG(LogHotUpdate, Verbose, TEXT("Skipped file (unchanged): %s"), *ServerEntry.FilePath);
				}
				else
				{
					// Hash 相同但本地文件丢失或损坏，需要重新下载
					FileInfo.ChangeType = EHotUpdateFileChangeType::Modified;
					OutResult.UpdateFiles.Add(FileInfo);
					OutResult.TotalUpdateSize += ServerEntry.FileSize;
					OutResult.ModifiedFileCount++;

					if (ServerEntry.Source == TEXT("patch") && ServerEntry.ChunkId >= 0)
					{
						RequiredPatchChunkIds.Add(ServerEntry.ChunkId);
					}

					UE_LOG(LogHotUpdate, Verbose, TEXT("Modified file (local missing): %s"), *ServerEntry.FilePath);
				}
			}
			else
			{
				// 文件已修改
				FileInfo.ChangeType = EHotUpdateFileChangeType::Modified;
				OutResult.UpdateFiles.Add(FileInfo);
				OutResult.TotalUpdateSize += ServerEntry.FileSize;
				OutResult.ModifiedFileCount++;

				if (ServerEntry.Source == TEXT("patch") && ServerEntry.ChunkId >= 0)
				{
					RequiredPatchChunkIds.Add(ServerEntry.ChunkId);
				}

				UE_LOG(LogHotUpdate, Verbose, TEXT("Modified file: %s (old hash: %s, new hash: %s)"),
					*ServerEntry.FilePath, *LocalEntry->FileHash, *ServerEntry.FileHash);
			}
		}
	}

	// 检测已删除的文件（存在于本地但不在服务器 Manifest 中）
	for (const auto& Pair : LocalFileIndex)
	{
		if (!ServerFilePaths.Contains(Pair.Key))
		{
			OutResult.DeletedFileCount++;
			UE_LOG(LogHotUpdate, Verbose, TEXT("Deleted file: %s"), *Pair.Key);
		}
	}

	// 根据需要的 Chunk ID，收集需要下载的容器
	for (const FHotUpdateContainerInfo& Container : ServerManifest.Containers)
	{
		if (RequiredPatchChunkIds.Contains(Container.ChunkId))
		{
			OutResult.UpdateContainers.Add(Container);
			OutResult.IncrementalDownloadSize += Container.UtocSize + Container.UcasSize;
			UE_LOG(LogHotUpdate, Log, TEXT("Required container: %s (chunkId: %d, size: %lld)"),
				*Container.ContainerName, Container.ChunkId, Container.UtocSize + Container.UcasSize);
		}
	}

	UE_LOG(LogHotUpdate, Log, TEXT("Incremental analysis complete: %d added, %d modified, %d deleted, %d skipped"),
		OutResult.AddedFileCount, OutResult.ModifiedFileCount, OutResult.DeletedFileCount, OutResult.SkippedFileCount);

	UE_LOG(LogHotUpdate, Log, TEXT("Required containers: %d, total download size: %lld bytes"),
		OutResult.UpdateContainers.Num(), OutResult.IncrementalDownloadSize);
}

bool UHotUpdateIncrementalCalculator::IsLocalFileValid(
	const FString& StoragePath,
	const FString& RelativePath,
	const FString& ExpectedHash,
	int64 ExpectedSize,
	const FHotUpdateVersionInfo& CurrentVersion,
	const FHotUpdateVersionInfo& LatestVersion) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 检查当前版本目录
	FString LocalFilePath = StoragePath / CurrentVersion.ToString() / RelativePath;

	if (!PlatformFile.FileExists(*LocalFilePath))
	{
		// 也检查新版本目录
		LocalFilePath = StoragePath / LatestVersion.ToString() / RelativePath;
		if (!PlatformFile.FileExists(*LocalFilePath))
		{
			return false;
		}
	}

	// 检查文件大小
	if (ExpectedSize > 0)
	{
		int64 ActualSize = IFileManager::Get().FileSize(*LocalFilePath);
		if (ActualSize != ExpectedSize)
		{
			UE_LOG(LogHotUpdate, Verbose, TEXT("File size mismatch: %s (expected: %lld, actual: %lld)"),
				*RelativePath, ExpectedSize, ActualSize);
			return false;
		}
	}

	// 如果需要 Hash 验证
	if (!ExpectedHash.IsEmpty())
	{
		FString ActualHash = UHotUpdateFileUtils::CalculateFileHash(LocalFilePath);
		if (ActualHash != ExpectedHash)
		{
			UE_LOG(LogHotUpdate, Verbose, TEXT("File hash mismatch: %s"), *RelativePath);
			return false;
		}
	}

	return true;
}