// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateEditorTypes.h"
#include "HAL/CriticalSection.h"
#include <atomic>
#include "HotUpdateIoStoreBuilder.generated.h"

/**
 * IoStore 构建进度
 */
USTRUCT(BlueprintType)
struct HOTUPDATEEDITOR_API FHotUpdateIoStoreProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString CurrentStage;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString CurrentFile;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 ProcessedFiles;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 TotalFiles;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int64 ProcessedBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int64 TotalBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	bool bIsComplete;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	bool bHasError;

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString ErrorMessage;

	FHotUpdateIoStoreProgress()
		: ProcessedFiles(0)
		, TotalFiles(0)
		, ProcessedBytes(0)
		, TotalBytes(0)
		, bIsComplete(false)
		, bHasError(false)
	{
	}

	float GetProgressPercent() const
	{
		return TotalFiles > 0 ? (float)ProcessedFiles / TotalFiles * 100.0f : 0.0f;
	}
};

// 进度委托
DECLARE_MULTICAST_DELEGATE_OneParam(FOnIoStoreProgressDelegate, const FHotUpdateIoStoreProgress&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnIoStoreCompleteDelegate, const FHotUpdateIoStoreResult&);

/**
 * IoStore 构建器
 * 使用 UE5 IoStore API 创建 .utoc/.ucas 容器文件
 */
UCLASS(BlueprintType)
class HOTUPDATEEDITOR_API UHotUpdateIoStoreBuilder : public UObject
{
	GENERATED_BODY()

public:
	UHotUpdateIoStoreBuilder();

	/**
	 * 构建 IoStore 容器
	 * @param AssetPathToDiskPath 资源包路径到磁盘路径的映射（如 "/Game/Maps/Start" -> "E:/.../Content/Maps/Start.umap"）
	 * @param OutputPath 输出路径（不含扩展名）
	 * @param Config IoStore 配置
	 * @return 构建结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Hot Update|IoStore")
	FHotUpdateIoStoreResult BuildIoStoreContainer(
		const TMap<FString, FString>& AssetPathToDiskPath,
		const FString& OutputPath,
		const FHotUpdateIoStoreConfig& Config);

	/**
	 * 异步构建 IoStore 容器
	 */
	UFUNCTION(BlueprintCallable, Category = "Hot Update|IoStore")
	void BuildIoStoreContainerAsync(
		const TMap<FString, FString>& AssetPathToDiskPath,
		const FString& OutputPath,
		const FHotUpdateIoStoreConfig& Config);

	/**
	 * 取消构建
	 */
	UFUNCTION(BlueprintCallable, Category = "Hot Update|IoStore")
	void CancelBuild();

	/**
	 * 是否正在构建
	 */
	UFUNCTION(BlueprintPure, Category = "Hot Update|IoStore")
	bool IsBuilding() const { return bIsBuilding; }

	/**
	 * 获取当前进度
	 */
	UFUNCTION(BlueprintPure, Category = "Hot Update|IoStore")
	FHotUpdateIoStoreProgress GetCurrentProgress() const;

	/**
	 * 验证配置
	 */
	bool ValidateConfig(const FHotUpdateIoStoreConfig& Config, FString& OutErrorMessage);

	// 进度委托
	FOnIoStoreProgressDelegate OnProgress;

	// 完成委托
	FOnIoStoreCompleteDelegate OnComplete;

private:
	/**
	 * 使用 UnrealPak 创建 IoStore 格式容器
	 */
	bool CreateIoStoreWithUnrealPak(
		const TMap<FString, FString>& AssetPathToDiskPath,
		const FString& OutputPath,
		const FHotUpdateIoStoreConfig& Config,
		FHotUpdateIoStoreResult& OutResult);

	/**
	 * 生成加密密钥文件
	 */
	FString GenerateCryptoKeyFile(
		const FString& TempDir,
		const FHotUpdateIoStoreConfig& Config);

	/**
	 * 查找 UnrealPak 工具路径
	 */
	FString FindUnrealPakPath(FString& OutErrorMessage);

	/**
	 * 准备临时目录
	 */
	bool PrepareTempDirectory(
		const FString& TempDir,
		FString& OutErrorMessage);

	/**
	 * 从 AssetPath 获取 Pak 内部路径
	 * @param AssetPath UE 包路径（如 "/Game/Maps/Start"）
	 * @return Pak 内部路径（如 "/Game/Maps/Start.umap"）
	 */
	FString GetPakInternalPath(const FString& AssetPath, const FString& DiskPath = TEXT(""));

	/**
	 * 生成响应文件
	 */
	bool GenerateResponseFile(
		const TMap<FString, FString>& AssetPathToDiskPath,
		const FString& ResponseFilePath,
		int32& OutValidFileCount,
		int64& OutTotalSize,
		FString& OutErrorMessage);

	/**
	 * 准备输出目录，删除已存在的输出文件
	 */
	bool PrepareOutputDirectory(
		const FString& OutputPath,
		FString& OutErrorMessage);

	/**
	 * 构建 UnrealPak 命令行参数
	 */
	FString BuildUnrealPakCommandLine(
		const FString& OutputPath,
		const FString& ResponseFilePath,
		const FString& CryptoKeyPath,
		const FHotUpdateIoStoreConfig& Config);

	/**
	 * 执行 UnrealPak 并验证结果
	 */
	bool ExecuteUnrealPak(
		const FString& UnrealPakPath,
		const FString& CmdLine,
		const FString& OutputPath,
		FHotUpdateIoStoreResult& OutResult);

	/**
	 * 清理临时目录
	 */
	void CleanupTempDirectory(const FString& TempDir);

	/**
	 * 更新进度
	 */
	void UpdateProgress(
		const FString& Stage,
		const FString& CurrentFile,
		int32 ProcessedFiles,
		int32 TotalFiles,
		int64 ProcessedBytes,
		int64 TotalBytes);

private:
	/// 是否正在构建
	std::atomic<bool> bIsBuilding;

	/// 是否已取消
	std::atomic<bool> bIsCancelled;

	/// 当前进度
	FHotUpdateIoStoreProgress CurrentProgress;

	/// 进度临界区
	mutable FCriticalSection ProgressCriticalSection;

	/// 构建任务
	TFuture<void> BuildTask;
};