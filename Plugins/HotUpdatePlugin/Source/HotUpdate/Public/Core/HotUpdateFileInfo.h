// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateTypes.h"
#include "HotUpdateFileInfo.generated.h"

/**
 * 单个文件信息
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateFileInfo
{
	GENERATED_BODY()

	/// 文件相对路径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString FilePath;

	/// 文件大小（字节）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int64 FileSize;

	/// 文件 Hash (SHA1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString FileHash;

	/// 下载 URL
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString DownloadUrl;

	/// 变更类型
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	EHotUpdateFileChangeType ChangeType;

	/// 依赖的 Chunk ID 列表
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	TArray<int32> DependencyChunks;

	FHotUpdateFileInfo()
		: FileSize(0)
		, ChangeType(EHotUpdateFileChangeType::Unchanged)
	{
	}
};

/**
 * Manifest 文件条目
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateManifestEntry
{
	GENERATED_BODY()

	/// 文件相对路径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString RelativePath;

	/// 文件大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int64 FileSize;

	/// 文件 SHA1 Hash
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString FileHash;

	/// 文件 MD5 Hash（兼容某些服务器）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString FileMd5;

	/// 所属 Chunk ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int32 ChunkId;

	/// 下载优先级
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int32 Priority;

	/// 是否压缩
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	bool bIsCompressed;

	/// 压缩后大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int64 CompressedSize;

	/// 下载 URL（可选覆盖）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString CustomDownloadUrl;

	/// 依赖文件列表
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	TArray<FString> Dependencies;

	/// 文件来源（base/patch）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString Source;

	FHotUpdateManifestEntry()
		: FileSize(0)
		, ChunkId(-1)
		, Priority(0)
		, bIsCompressed(false)
		, CompressedSize(0)
	{
	}
};