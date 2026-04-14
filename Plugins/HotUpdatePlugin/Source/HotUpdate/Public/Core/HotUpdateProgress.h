// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateProgress.generated.h"

/**
 * 下载进度信息
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateProgress
{
	GENERATED_BODY()

	/// 已下载字节数
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int64 DownloadedBytes;

	/// 总字节数
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int64 TotalBytes;

	/// 当前下载速度（字节/秒）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	float DownloadSpeed;

	/// 剩余时间（秒）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	float RemainingTime;

	/// 当前文件索引
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int32 CurrentFileIndex;

	/// 总文件数
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int32 TotalFiles;

	FHotUpdateProgress()
		: DownloadedBytes(0)
		, TotalBytes(0)
		, DownloadSpeed(0.0f)
		, RemainingTime(0.0f)
		, CurrentFileIndex(0)
		, TotalFiles(0)
	{
	}

	};