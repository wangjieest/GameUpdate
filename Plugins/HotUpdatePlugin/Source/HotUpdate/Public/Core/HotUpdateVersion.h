// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/HotUpdateTypes.h"
#include "Core/HotUpdateVersionInfo.h"
#include "Core/HotUpdateContainerTypes.h"
#include "Core/HotUpdateFileInfo.h"
#include "HotUpdateVersion.generated.h"

/**
 * 版本检查结果
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateVersionCheckResult
{
	GENERATED_BODY()

	/// 是否有更新
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	bool bHasUpdate;

	/// 当前版本
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	FHotUpdateVersionInfo CurrentVersion;

	/// 最新版本
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	FHotUpdateVersionInfo LatestVersion;

	/// 需要下载的容器列表（IoStore 容器文件）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	TArray<FHotUpdateContainerInfo> UpdateContainers;

	/// 更新文件列表（用于显示和验证）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	TArray<FHotUpdateFileInfo> UpdateFiles;

	/// 错误信息
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	FString ErrorMessage;

	// == 增量下载统计 ==

	/// 跳过的文件数（本地已存在且未变更）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int32 SkippedFileCount;

	/// 跳过的文件总大小（字节）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int64 SkippedTotalSize;

	/// 新增文件数
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int32 AddedFileCount;

	/// 修改文件数
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int32 ModifiedFileCount;

	/// 删除文件数
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int32 DeletedFileCount;

	/// 实际需要下载的大小（增量下载大小）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int64 IncrementalDownloadSize;

	FHotUpdateVersionCheckResult()
		: bHasUpdate(false)
		, SkippedFileCount(0)
		, SkippedTotalSize(0)
		, AddedFileCount(0)
		, ModifiedFileCount(0)
		, DeletedFileCount(0)
		, IncrementalDownloadSize(0)
	{
	}
};