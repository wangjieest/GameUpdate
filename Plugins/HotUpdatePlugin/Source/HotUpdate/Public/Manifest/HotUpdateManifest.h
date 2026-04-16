// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/HotUpdateTypes.h"
#include "Core/HotUpdateVersionInfo.h"
#include "Core/HotUpdateChunkTypes.h"
#include "Core/HotUpdateFileInfo.h"
#include "Core/HotUpdateContainerTypes.h"
#include "HotUpdateManifest.generated.h"

/**
 * 完整 Manifest 数据
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateManifest
{
	GENERATED_BODY()

	/// Manifest 版本（2 = 支持基础包/更新包）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int32 ManifestVersion;

	/// 包类型（基础包/更新包）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	EHotUpdatePackageKind PackageKind;

	/// 资源版本信息
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FHotUpdateVersionInfo VersionInfo;

	/// === 基础包字段 ===

	/// Chunk 定义列表（包含依赖关系）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	TArray<FHotUpdateChunkInfo> Chunks;

	/// === 更新包字段 ===

	/// 基础版本号（仅更新包有效）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString BaseVersion;

	/// === 全量热更新字段 ===

	/// 是否包含基础版本容器
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	bool bIncludesBaseContainers;

	/// 是否需要预先安装基础包
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	bool bRequiresBasePackage;

	/// 总下载大小（包含基础版本容器）
	UPROPERTY(BlueprintReadOnly, Category = "HotUpdate")
	int64 TotalDownloadSize;

	/// === 通用字段 ===

	/// 容器文件列表（.utoc/.ucas）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	TArray<FHotUpdateContainerInfo> Containers;

	/// 所有文件条目
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	TArray<FHotUpdateManifestEntry> Files;

	/// 计算总大小
	void BuildPathIndex();

	/// 按路径查找文件（使用索引快速查找，编辑器端使用）
	const FHotUpdateManifestEntry* FindFile(const FString& FilePath) const;

	/// 获取 Chunk 信息
	const FHotUpdateChunkInfo* FindChunk(int32 ChunkId) const;

	/// 按 ChunkId 查找 Container（运行时热更新使用）
	const FHotUpdateContainerInfo* FindContainer(int32 ChunkId) const;

	FHotUpdateManifest()
		: ManifestVersion(2)
		, PackageKind(EHotUpdatePackageKind::Base)
		, bIncludesBaseContainers(false)
		, bRequiresBasePackage(true)
		, TotalDownloadSize(0)
		, bPathIndexBuilt(false)
	{
	}

private:
	/// 路径到索引的映射（用于快速查找文件，编辑器端使用）
	mutable TMap<FString, int32> PathIndex;

	/// Chunk ID 到索引的映射（用于快速查找 Chunk）
	mutable TMap<int32, int32> ChunkIndex;

	/// Chunk ID 到 Container 索引的映射（运行时热更新使用）
	mutable TMap<int32, int32> ContainerIndex;

	/// 索引是否已构建
	mutable bool bPathIndexBuilt;
};