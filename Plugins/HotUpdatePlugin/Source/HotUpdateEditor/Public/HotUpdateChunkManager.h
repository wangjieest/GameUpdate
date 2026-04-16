// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateEditorTypes.h"
#include "HAL/CriticalSection.h"
#include <atomic>
#include "HotUpdateChunkManager.generated.h"

class IAssetRegistry;

/**
 * Chunk 分析配置（扩展版）
 */
USTRUCT(BlueprintType)
struct HOTUPDATEEDITOR_API FHotUpdateChunkAnalysisConfig
{
	GENERATED_BODY()

	/// 分包策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	EHotUpdateChunkStrategy ChunkStrategy;

	/// 最大 Chunk 大小（MB），0 表示无限制（用于 Size 策略）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk", meta = (ClampMin = "0"))
	int32 MaxChunkSizeMB;

	/// 按大小分包的详细配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Size")
	FHotUpdateSizeBasedChunkConfig SizeBasedConfig;

	/// 目录分包规则列表（用于 Directory 和 Hybrid 策略）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Directory")
	TArray<FHotUpdateDirectoryChunkRule> DirectoryChunkRules;

	/// 是否分析依赖关系
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	bool bAnalyzeDependencies;

	/// 基础包 Chunk ID 起始值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int32 BaseChunkIdStart;

	/// 更新包 Chunk ID 起始值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int32 PatchChunkIdStart;

	/// 未匹配任何规则的资源的默认 Chunk 名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	FString DefaultChunkName;

	/// 未匹配任何规则的资源的默认 Chunk ID（-1 表示自动分配）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int32 DefaultChunkId;

	FHotUpdateChunkAnalysisConfig()
		: ChunkStrategy(EHotUpdateChunkStrategy::PrimaryAsset)
		, MaxChunkSizeMB(256)
		, bAnalyzeDependencies(true)
		, BaseChunkIdStart(0)
		, PatchChunkIdStart(10000)
		, DefaultChunkName(TEXT("Default"))
		, DefaultChunkId(-1)
	{
	}
};

/**
 * Chunk 分析结果
 */
USTRUCT(BlueprintType)
struct HOTUPDATEEDITOR_API FHotUpdateChunkAnalysisResult
{
	GENERATED_BODY()

	/// 所有 Chunk 定义
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FHotUpdateChunkDefinition> Chunks;

	/// 资源到 Chunk 的映射
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TMap<FString, int32> AssetToChunkMap;


	/// 统计信息
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 TotalAssetCount;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 TotalChunkCount;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int64 TotalSize;

	/// 是否成功
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess;

	/// 错误信息
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString ErrorMessage;

	FHotUpdateChunkAnalysisResult()
		: TotalAssetCount(0)
		, TotalChunkCount(0)
		, TotalSize(0)
		, bSuccess(false)
	{
	}
};

/**
 * Chunk 管理器
 * 使用 UE5 标准方法进行 Chunk 划分
 */
UCLASS(BlueprintType)
class HOTUPDATEEDITOR_API UHotUpdateChunkManager : public UObject
{
	GENERATED_BODY()

public:
	UHotUpdateChunkManager();

	/**
	 * 分析资源并创建 Chunk 划分
	 * @param AssetPaths 资源路径列表
	 * @param AssetDiskPaths 资源磁盘路径映射
	 * @param Config 分析配置
	 * @return 分析结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Hot Update|Chunk")
	FHotUpdateChunkAnalysisResult AnalyzeAndCreateChunks(
		const TArray<FString>& AssetPaths,
		const TMap<FString, FString>& AssetDiskPaths,
		const FHotUpdateChunkAnalysisConfig& Config);

	/**
	 * 为更新包创建 Chunk 划分
	 * @param ChangedAssets 变更的资源
	 * @param AssetDiskPaths 资源磁盘路径映射
	 * @param Config 分析配置
	 * @param PatchChunkIdStart 更新包 Chunk ID 起始值
	 * @return 分析结果
	 */
	FHotUpdateChunkAnalysisResult CreatePatchChunks(
		const TArray<FString>& ChangedAssets,
		const TMap<FString, FString>& AssetDiskPaths,
		const FHotUpdateChunkAnalysisConfig& Config);

	/**
	 * 获取资源类型
	 */
	static FString GetAssetType(const FString& AssetPath, IAssetRegistry* AssetRegistry);

	/**
	 * 获取资源类型的默认 Chunk ID
	 */
	static int32 GetDefaultChunkIdForAssetType(const FString& AssetType);

	/**
	 * 获取所有资源类型的默认 Chunk ID 映射
	 */
	static TMap<FString, int32> GetDefaultAssetTypeChunkMap();

private:
	/**
	 * 按 Primary Asset 划分（UE5 标准）
	 */
	bool DivideByPrimaryAsset(
		const TArray<FString>& AssetPaths,
		IAssetRegistry* AssetRegistry,
		TArray<FHotUpdateChunkDefinition>& OutChunks,
		TMap<FString, int32>& OutAssetToChunk);

	/**
	 * 按资源类型划分
	 */
	bool DivideByAssetType(
		const TArray<FString>& AssetPaths,
		IAssetRegistry* AssetRegistry,
		TArray<FHotUpdateChunkDefinition>& OutChunks,
		TMap<FString, int32>& OutAssetToChunk);

	/**
	 * 按大小划分
	 */
	bool DivideBySize(
		const TArray<FString>& AssetPaths,
		const TMap<FString, FString>& AssetDiskPaths,
		int32 MaxChunkSizeMB,
		TArray<FHotUpdateChunkDefinition>& OutChunks,
		TMap<FString, int32>& OutAssetToChunk);

	/**
	 * 按大小划分（扩展版，支持详细配置）
	 */
	bool DivideBySizeWithConfig(
		const TArray<FString>& AssetPaths,
		const TMap<FString, FString>& AssetDiskPaths,
		const FHotUpdateSizeBasedChunkConfig& Config,
		TArray<FHotUpdateChunkDefinition>& OutChunks,
		TMap<FString, int32>& OutAssetToChunk);

	/**
	 * 按目录规则划分
	 */
	bool DivideByDirectory(
		const TArray<FString>& AssetPaths,
		const TMap<FString, FString>& AssetDiskPaths,
		const TArray<FHotUpdateDirectoryChunkRule>& DirectoryRules,
		IAssetRegistry* AssetRegistry,
		TArray<FHotUpdateChunkDefinition>& OutChunks,
		TMap<FString, int32>& OutAssetToChunk,
		TArray<FString>& OutUnmatchedAssets);

	/**
	 * 检查资源路径是否匹配目录规则
	 */
	static bool MatchesDirectoryRule(const FString& AssetPath, const FHotUpdateDirectoryChunkRule& Rule);

	/**
	 * 为目录资源按大小细分
	 */
	bool DivideDirectoryAssetsBySize(
		const TArray<FString>& Assets,
		const TMap<FString, FString>& AssetDiskPaths,
		const FHotUpdateDirectoryChunkRule& Rule,
		TArray<FHotUpdateChunkDefinition>& OutChunks,
		TMap<FString, int32>& OutAssetToChunk);

	/**
	 * 不分包（所有资源放入一个 Chunk）
	 */
	bool CreateSingleChunk(
		const TArray<FString>& AssetPaths,
		const TMap<FString, FString>& AssetDiskPaths,
		const FString& ChunkName,
		int32 ChunkId,
		TArray<FHotUpdateChunkDefinition>& OutChunks,
		TMap<FString, int32>& OutAssetToChunk);

	/**
	 * 构建依赖关系
	 */
	bool BuildDependencies(
		TArray<FHotUpdateChunkDefinition>& Chunks,
		const TMap<FString, int32>& AssetToChunk,
		IAssetRegistry* AssetRegistry);

	/**
	 * 获取资源文件大小
	 */
	static int64 GetAssetSize(const FString& AssetPath, const TMap<FString, FString>& AssetDiskPaths);

	/**
	 * 分配下一个 Chunk ID
	 */
	int32 AllocateNextChunkId();

private:
	/// 下一个自动分配的 Chunk ID
	static int32 NextAutoChunkId;

	/// Chunk ID 分配锁
	static FCriticalSection ChunkIdLock;
};